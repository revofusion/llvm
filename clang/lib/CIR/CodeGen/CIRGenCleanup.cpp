//===--- CIRGenCleanup.cpp - Bookkeeping and code emission for cleanups ---===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains code dealing with the IR generation for cleanups
// and related information.
//
// A "cleanup" is a piece of code which needs to be executed whenever
// control transfers out of a particular scope.  This can be
// conditionalized to occur only on exceptional control flow, only on
// normal control flow, or both.
//
//===----------------------------------------------------------------------===//

#include "CIRGenCleanup.h"
#include "CIRGenFunction.h"

#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CIR/MissingFeatures.h"
#include "clang/UnifiedSymbolResolution/USRGeneration.h"

using namespace clang;
using namespace clang::CIRGen;

namespace {
/// Return true if the expression tree contains an AbstractConditionalOperator
/// (ternary ?:), which is the only construct whose CIR codegen calls
/// ConditionalEvaluation::beginEvaluation() and thus causes cleanups to be
/// deferred via pushFullExprCleanup.  Logical &&/|| do NOT call
/// beginEvaluation(); their branch-local cleanups are handled by LexicalScope.
class ConditionalEvaluationFinder
    : public RecursiveASTVisitor<ConditionalEvaluationFinder> {
  bool foundConditional = false;

public:
  bool found() const { return foundConditional; }

  bool VisitAbstractConditionalOperator(AbstractConditionalOperator *) {
    foundConditional = true;
    return false;
  }

  // Don't cross evaluation-context boundaries.
  bool TraverseLambdaExpr(LambdaExpr *) { return true; }
  bool TraverseBlockExpr(BlockExpr *) { return true; }
  bool TraverseStmtExpr(StmtExpr *) { return true; }
};

class AutomaticObjectDeclarationOrdinalFinder
    : public RecursiveASTVisitor<AutomaticObjectDeclarationOrdinalFinder> {
public:
  explicit AutomaticObjectDeclarationOrdinalFinder(const VarDecl *target)
      : target(target) {}

  bool shouldVisitImplicitCode() const { return true; }

  bool VisitVarDecl(VarDecl *variable) {
    if (!variable || !variable->isLocalVarDecl() ||
        variable->hasExternalStorage() || variable->isStaticLocal())
      return true;
    const VarDecl *canonical = variable->getCanonicalDecl();
    if (!seen.insert(canonical).second)
      return true;
    if (canonical == target->getCanonicalDecl())
      ordinal = nextOrdinal;
    ++nextOrdinal;
    return true;
  }

  // A closure body is a different FunctionDecl preorder domain. Its capture
  // initializers remain part of the enclosing expression and are traversed by
  // the ordinary LambdaExpr children before this hook.
  bool TraverseLambdaExpr(LambdaExpr *lambda) {
    if (!lambda)
      return true;
    for (Expr *captureInit : lambda->capture_inits())
      if (captureInit && !TraverseStmt(captureInit))
        return false;
    return true;
  }

  std::optional<uint64_t> result() const { return ordinal; }

private:
  const VarDecl *target;
  llvm::DenseSet<const VarDecl *> seen;
  uint64_t nextOrdinal = 0;
  std::optional<uint64_t> ordinal;
};

std::optional<uint64_t>
getAutomaticObjectDeclarationOrdinal(const FunctionDecl *function,
                                     const VarDecl *target) {
  if (!function || !function->hasBody() || !target)
    return std::nullopt;
  AutomaticObjectDeclarationOrdinalFinder finder(target);
  if (!finder.TraverseStmt(const_cast<Stmt *>(function->getBody())))
    return std::nullopt;
  return finder.result();
}

std::optional<mlir::DictionaryAttr>
transferTemporaryIdentityToAutomaticObject(
    cir::AllocaOp alloca, mlir::DictionaryAttr temporaryIdentity) {
  if (!alloca)
    return temporaryIdentity;
  mlir::DictionaryAttr automaticIdentity =
      alloca.getAstAutomaticObjectIdentityAttr();
  if (!automaticIdentity)
    return temporaryIdentity;

  auto declarationOrdinal =
      automaticIdentity.getAs<mlir::IntegerAttr>("declaration_ordinal");
  if (!declarationOrdinal)
    return std::nullopt;
  mlir::NamedAttrList transferredIdentity(temporaryIdentity);
  transferredIdentity.set("transferred_to_automatic_decl_ordinal",
                          declarationOrdinal);
  if (auto declarationUSR =
          automaticIdentity.getAs<mlir::StringAttr>("declaration_usr")) {
    transferredIdentity.set("transferred_to_automatic_decl_usr",
                            declarationUSR);
  }
  return transferredIdentity.getDictionary(temporaryIdentity.getContext());
}

} // namespace

std::optional<uint64_t> CIRGenFunction::getBindTemporaryDeclarationOrdinal(
    const CXXBindTemporaryExpr *temporary) {
  if (!temporary)
    return std::nullopt;
  auto [ordinal, inserted] = bindTemporaryDeclarationOrdinals.try_emplace(
      temporary, nextBindTemporaryDeclarationOrdinal);
  if (inserted)
    ++nextBindTemporaryDeclarationOrdinal;
  return ordinal->second;
}

std::optional<uint64_t>
CIRGenFunction::getMaterializedTemporaryDeclarationOrdinal(
    const MaterializeTemporaryExpr *temporary) {
  if (!temporary)
    return std::nullopt;
  auto [ordinal, inserted] =
      materializedTemporaryDeclarationOrdinals.try_emplace(
          temporary, nextMaterializedTemporaryDeclarationOrdinal);
  if (inserted)
    ++nextMaterializedTemporaryDeclarationOrdinal;
  return ordinal->second;
}

void CIRGenFunction::setCXXBindTemporaryObjectIdentity(
    const CXXBindTemporaryExpr *binding, const CXXTemporary *temporary,
    Address address) {
  assert(binding && temporary &&
         "temporary cleanup identity requires its exact AST owner");
  cir::AllocaOp alloca = address.getUnderlyingAllocaOp();
  mlir::Operation *identityOwner =
      alloca ? alloca.getOperation() : address.getDefiningOp();
  if (!identityOwner) {
    cgm.errorNYI(binding->getSourceRange(),
                 "temporary cleanup storage has no producer operation");
    return;
  }
  // A CXXBindTemporaryExpr constructed directly into the function return
  // value transfers destruction to the caller. The return alloca is storage,
  // not a local cleanup owner, so it must not carry temporary-cleanup
  // identity. NRVO declarations still carry their separate automatic-object
  // identity on this storage.
  if (returnValue.isValid() && alloca &&
      alloca == returnValue.getUnderlyingAllocaOp())
    return;

  // Structured-op region builders run before the enclosing operation is
  // attached to its owner, so the alloca's parent chain is not always
  // complete yet. curFn is the producer-owned concrete function or global.
  auto function = mlir::dyn_cast_or_null<cir::FuncOp>(curFn);
  auto global = mlir::dyn_cast_or_null<cir::GlobalOp>(curFn);
  SourceLocation begin = binding->getBeginLoc();
  SourceLocation end = binding->getEndLoc();
  const CXXDestructorDecl *destructor = temporary->getDestructor();
  if ((!function && !global) || !destructor) {
    std::string reason = "temporary cleanup identity has no";
    if (!function && !global) {
      reason += " symbolic owner";
      if (curFn)
        reason +=
            " (owner operation: " + curFn->getName().getStringRef().str() + ")";
    }
    if (!destructor)
      reason += " destructor";
    cgm.errorNYI(binding->getSourceRange(), reason);
    return;
  }
  if (begin.isInvalid() || end.isInvalid()) {
    cgm.errorNYI(binding->getSourceRange(),
                 "temporary identity has invalid source provenance");
    return;
  }

  // Keep allocation locations aligned with their exact AST temporary. Derived
  // storage keeps its own producer location because the root may be a function
  // argument rather than an alloca.
  if (alloca)
    alloca->setLoc(getLoc(binding->getSourceRange()));
  CIRGenBuilderTy &builder = getBuilder();
  mlir::NamedAttrList identity;
  mlir::FlatSymbolRefAttr owner = mlir::FlatSymbolRefAttr::get(
      function ? function.getSymNameAttr() : global.getSymNameAttr());
  identity.set("owner", owner);
  identity.set("owner_kind",
               builder.getStringAttr(function ? "function" : "global"));
  if (function)
    identity.set("function", owner);
  identity.set("begin_raw", builder.getI64IntegerAttr(begin.getRawEncoding()));
  identity.set("end_raw", builder.getI64IntegerAttr(end.getRawEncoding()));
  std::optional<uint64_t> declarationOrdinal =
      getBindTemporaryDeclarationOrdinal(binding);
  if (!declarationOrdinal) {
    cgm.errorNYI(binding->getSourceRange(),
                 "temporary cleanup has no owning-declaration preorder "
                 "identity");
    return;
  }
  identity.set("declaration_ordinal",
               builder.getI64IntegerAttr(*declarationOrdinal));
  mlir::ArrayAttr existing =
      alloca ? alloca.getAstTemporaryObjectIdentitiesAttr()
             : identityOwner->getAttrOfType<mlir::ArrayAttr>(
                   "cir.ast_temporary_object_identities");
  mlir::StringAttr instanceToken;
  if (!existing || existing.empty()) {
    if (alloca) {
      if (mlir::DictionaryAttr materializeIdentity =
              alloca.getAstMaterializeTemporaryIdentityAttr())
        instanceToken =
            materializeIdentity.getAs<mlir::StringAttr>("instance_token");
    }
  } else {
    for (mlir::Attribute attribute : existing) {
      auto existingIdentity = mlir::dyn_cast<mlir::DictionaryAttr>(attribute);
      if (!existingIdentity)
        continue;
      auto existingOwner =
          existingIdentity.getAs<mlir::FlatSymbolRefAttr>("owner");
      auto existingOrdinal =
          existingIdentity.getAs<mlir::IntegerAttr>("declaration_ordinal");
      if (existingOwner && existingOrdinal &&
          !existingOrdinal.getValue().isNegative() && existingOwner == owner &&
          existingOrdinal.getValue().getZExtValue() == *declarationOrdinal) {
        instanceToken =
            existingIdentity.getAs<mlir::StringAttr>("instance_token");
        break;
      }
    }
  }
  if (!instanceToken)
    instanceToken = builder.getStringAttr(getCXXTemporaryObjectInstanceToken());
  identity.set("instance_token", instanceToken);
  identity.set("cleanup_kind", builder.getStringAttr("cxx_destructor"));
  identity.set("destructor_symbol",
               builder.getStringAttr(
                   cgm.getMangledName(GlobalDecl(destructor, Dtor_Complete))));

  llvm::SmallString<256> destructorUSR;
  if (clang::index::generateUSRForDecl(destructor->getCanonicalDecl(),
                                       destructorUSR)) {
    cgm.errorNYI(binding->getSourceRange(),
                 "temporary cleanup destructor has no exact canonical USR");
    return;
  }
  identity.set("destructor_usr", builder.getStringAttr(destructorUSR));
  bool requiresObservedConstructorCall = false;
  const Expr *subExpr = binding->getSubExpr()->IgnoreParenImpCasts();
  while (const auto *cast = dyn_cast<CXXFunctionalCastExpr>(subExpr)) {
    const bool sameType = getContext().hasSameUnqualifiedType(
        cast->getType(), cast->getSubExpr()->getType());
    if (cast->getCastKind() != CK_NoOp &&
        cast->getCastKind() != CK_ConstructorConversion &&
        (cast->getCastKind() != CK_UserDefinedConversion || !sameType))
      break;
    subExpr = cast->getSubExpr()->IgnoreParenImpCasts();
  }
  const CXXConstructorDecl *constructor = nullptr;
  if (const auto *construct = dyn_cast<CXXConstructExpr>(subExpr)) {
    constructor = construct->getConstructor();
  } else if (const auto *init = dyn_cast<InitListExpr>(subExpr)) {
    if (getContext().getAsConstantArrayType(init->getType())) {
      const Expr *filler = init->getArrayFiller();
      const auto *construct = dyn_cast_or_null<CXXConstructExpr>(
          filler ? filler->IgnoreParenImpCasts() : nullptr);
      constructor = construct ? construct->getConstructor() : nullptr;
    } else {
      const CXXRecordDecl *record = init->getType()->getAsCXXRecordDecl();
      if (record && record->getDefinition())
        record = record->getDefinition();
      mlir::Type recordSchema = cgm.getTypes().convertRecordDeclType(record);
      mlir::StringAttr recordUSR = cgm.exactRecordUSRAttr(record);
      if (!recordUSR || recordUSR.getValue().empty()) {
        cgm.errorNYI(
            binding->getSourceRange(),
            "aggregate temporary cleanup constructor has no exact canonical "
            "RecordDecl USR");
        return;
      }
      // Aggregate initialization has no constructor FunctionDecl or ABI call.
      // Its exact producer is the canonical RecordDecl plus this semantic
      // initialization kind; consumers must not invent a callable identity.
      identity.set("constructor_kind",
                   builder.getStringAttr("implicit_aggregate_initialization"));
      identity.set("constructor_record_usr", recordUSR);
      identity.set("constructor_record_schema",
                   mlir::TypeAttr::get(recordSchema));
    }
  }
  if (constructor) {
    requiresObservedConstructorCall = !constructor->isTrivial();
    identity.set("constructor_symbol",
                 builder.getStringAttr(cgm.getMangledName(
                     GlobalDecl(constructor, Ctor_Complete))));
    llvm::SmallString<256> constructorUSR;
    if (clang::index::generateUSRForDecl(constructor->getCanonicalDecl(),
                                         constructorUSR)) {
      cgm.errorNYI(binding->getSourceRange(),
                   "temporary cleanup constructor has no exact canonical USR");
      return;
    }
    identity.set("constructor_usr", builder.getStringAttr(constructorUSR));
    // A member of a class-template specialization has a position-spelled
    // USR shared by every specialization. The parent record's minted
    // identity is concrete; consumers resolve the constructor through it.
    if (mlir::StringAttr ownerRecordUSR =
            cgm.exactRecordUSRAttr(constructor->getParent()))
      identity.set("constructor_owner_record_usr", ownerRecordUSR);
  }
  identity.set("requires_observed_constructor_call",
               builder.getBoolAttr(requiresObservedConstructorCall));

  mlir::DictionaryAttr temporaryIdentity =
      identity.getDictionary(&getMLIRContext());
  auto transferredIdentity =
      transferTemporaryIdentityToAutomaticObject(alloca, temporaryIdentity);
  if (!transferredIdentity) {
    cgm.errorNYI(binding->getSourceRange(),
                 "automatic cleanup identity lacks an exact destination for "
                 "late temporary transfer");
    return;
  }
  temporaryIdentity = *transferredIdentity;
  bool matchedExistingTuple = false;
  if (existing) {
    for (mlir::Attribute attribute : existing) {
      auto existingIdentity = mlir::dyn_cast<mlir::DictionaryAttr>(attribute);
      if (!existingIdentity)
        continue;
      auto existingOwner =
          existingIdentity.getAs<mlir::FlatSymbolRefAttr>("owner");
      auto existingOrdinal =
          existingIdentity.getAs<mlir::IntegerAttr>("declaration_ordinal");
      if (!existingOwner || !existingOrdinal ||
          existingOrdinal.getValue().isNegative() || existingOwner != owner ||
          existingOrdinal.getValue().getZExtValue() != *declarationOrdinal)
        continue;
      matchedExistingTuple = true;
      if (existingIdentity != temporaryIdentity)
        cgm.errorNYI(binding->getSourceRange(),
                     "conflicting automatic temporary identities");
    }
  }
  if (matchedExistingTuple)
    return;
  llvm::SmallVector<mlir::Attribute> identities;
  if (existing)
    identities.append(existing.begin(), existing.end());
  identities.push_back(temporaryIdentity);
  if (alloca)
    alloca.setAstTemporaryObjectIdentitiesAttr(
        builder.getArrayAttr(identities));
  else
    identityOwner->setAttr("cir.ast_temporary_object_identities",
                           builder.getArrayAttr(identities));
}
bool CIRGenFunction::setMaterializedConversionTemporaryObjectIdentity(
    const MaterializeTemporaryExpr *temporary, Address address) {
  if (!temporary)
    return false;
  const Expr *producer = temporary->getSubExpr()->IgnoreParenImpCasts();
  const auto *construct = dyn_cast_or_null<CXXConstructExpr>(producer);
  if (!construct)
    return false;
  const CXXConstructorDecl *constructor = construct->getConstructor();
  const CXXRecordDecl *record = temporary->getType()->getAsCXXRecordDecl();
  if (record && record->getDefinition())
    record = record->getDefinition();
  const CXXDestructorDecl *destructor =
      record ? record->getDestructor() : nullptr;
  if (!constructor || !destructor || destructor->isTrivial())
    return false;

  cir::AllocaOp alloca = address.getUnderlyingAllocaOp();
  mlir::Operation *identityOwner =
      alloca ? alloca.getOperation() : address.getDefiningOp();
  auto function = mlir::dyn_cast_or_null<cir::FuncOp>(curFn);
  auto global = mlir::dyn_cast_or_null<cir::GlobalOp>(curFn);
  SourceLocation begin = temporary->getBeginLoc();
  SourceLocation end = temporary->getEndLoc();
  if (!identityOwner || (!function && !global) || begin.isInvalid() ||
      end.isInvalid()) {
    cgm.errorNYI(temporary->getSourceRange(),
                 "materialized conversion cleanup lacks exact storage, "
                 "symbolic owner, or source provenance");
    return true;
  }

  CIRGenBuilderTy &builder = getBuilder();
  mlir::NamedAttrList identity;
  mlir::FlatSymbolRefAttr owner = mlir::FlatSymbolRefAttr::get(
      function ? function.getSymNameAttr() : global.getSymNameAttr());
  identity.set("owner", owner);
  identity.set("owner_kind",
               builder.getStringAttr(function ? "function" : "global"));
  if (function)
    identity.set("function", owner);
  identity.set("producer_kind",
               builder.getStringAttr("materialized_conversion"));
  identity.set("begin_raw", builder.getI64IntegerAttr(begin.getRawEncoding()));
  identity.set("end_raw", builder.getI64IntegerAttr(end.getRawEncoding()));
  std::optional<uint64_t> declarationOrdinal =
      getMaterializedTemporaryDeclarationOrdinal(temporary);
  if (!declarationOrdinal) {
    cgm.errorNYI(temporary->getSourceRange(),
                 "materialized conversion cleanup has no FunctionDecl "
                 "preorder identity");
    return true;
  }
  // Declaration ordinals for CXXBindTemporaryExpr and direct
  // MaterializeTemporaryExpr producers occupy distinct AST node domains.
  // Tag the latter while retaining its exact FunctionDecl preorder ordinal.
  constexpr uint64_t materializedConversionOrdinalDomain = uint64_t{1} << 62;
  identity.set("declaration_ordinal",
               builder.getI64IntegerAttr(materializedConversionOrdinalDomain |
                                         *declarationOrdinal));
  mlir::StringAttr instanceToken;
  if (alloca) {
    if (mlir::DictionaryAttr materializeIdentity =
            alloca.getAstMaterializeTemporaryIdentityAttr())
      instanceToken =
          materializeIdentity.getAs<mlir::StringAttr>("instance_token");
  }
  if (!instanceToken)
    instanceToken =
        builder.getStringAttr(getMaterializedTemporaryInstanceToken());
  identity.set("instance_token", instanceToken);
  identity.set("cleanup_kind", builder.getStringAttr("cxx_destructor"));
  identity.set("destructor_symbol",
               builder.getStringAttr(
                   cgm.getMangledName(GlobalDecl(destructor, Dtor_Complete))));
  llvm::SmallString<256> destructorUSR;
  if (clang::index::generateUSRForDecl(destructor->getCanonicalDecl(),
                                       destructorUSR)) {
    cgm.errorNYI(temporary->getSourceRange(),
                 "materialized conversion destructor has no exact canonical "
                 "USR");
    return true;
  }
  identity.set("destructor_usr", builder.getStringAttr(destructorUSR));
  identity.set("constructor_symbol",
               builder.getStringAttr(
                   cgm.getMangledName(GlobalDecl(constructor, Ctor_Complete))));
  llvm::SmallString<256> constructorUSR;
  if (clang::index::generateUSRForDecl(constructor->getCanonicalDecl(),
                                       constructorUSR)) {
    cgm.errorNYI(temporary->getSourceRange(),
                 "materialized conversion constructor has no exact canonical "
                 "USR");
    return true;
  }
  identity.set("constructor_usr", builder.getStringAttr(constructorUSR));
  identity.set("requires_observed_constructor_call",
               builder.getBoolAttr(!constructor->isTrivial() &&
                                   !construct->isElidable()));

  mlir::DictionaryAttr temporaryIdentity =
      identity.getDictionary(&getMLIRContext());
  auto transferredIdentity =
      transferTemporaryIdentityToAutomaticObject(alloca, temporaryIdentity);
  if (!transferredIdentity) {
    cgm.errorNYI(temporary->getSourceRange(),
                 "automatic cleanup identity lacks an exact destination for "
                 "late materialized-conversion transfer");
    return true;
  }
  temporaryIdentity = *transferredIdentity;
  mlir::ArrayAttr existing =
      alloca ? alloca.getAstTemporaryObjectIdentitiesAttr()
             : identityOwner->getAttrOfType<mlir::ArrayAttr>(
                   "cir.ast_temporary_object_identities");
  llvm::SmallVector<mlir::Attribute> identities;
  if (existing)
    identities.append(existing.begin(), existing.end());
  identities.push_back(temporaryIdentity);
  if (alloca)
    alloca.setAstTemporaryObjectIdentitiesAttr(
        builder.getArrayAttr(identities));
  else
    identityOwner->setAttr("cir.ast_temporary_object_identities",
                           builder.getArrayAttr(identities));
  return true;
}


std::optional<mlir::DictionaryAttr>
CIRGenFunction::setCXXAutomaticObjectIdentity(const VarDecl *variable,
                                              Address address) {
  assert(variable && "automatic cleanup identity requires its VarDecl owner");
  cir::AllocaOp alloca = address.getUnderlyingAllocaOp();
  if (!alloca) {
    cgm.errorNYI(variable->getSourceRange(),
                 "automatic cleanup storage has no underlying CIR alloca");
    return std::nullopt;
  }
  // Only an actual NRVO declaration may own automatic cleanup identity on
  // the function return storage. Match the exact VarDecl classification used
  // when that declaration is assigned the return allocation.
  if (returnValue.isValid() && alloca == returnValue.getUnderlyingAllocaOp() &&
      !(getContext().getLangOpts().ElideConstructors &&
        variable->isNRVOVariable())) {
    cgm.errorNYI(variable->getSourceRange(),
                 "non-NRVO automatic cleanup uses function return storage");
    return std::nullopt;
  }

  const VarDecl *canonicalVariable = variable->getCanonicalDecl();
  if (!canonicalVariable) {
    cgm.errorNYI(variable->getSourceRange(),
                 "automatic cleanup declaration has no canonical VarDecl");
    return std::nullopt;
  }


  auto function = mlir::dyn_cast_or_null<cir::FuncOp>(curFn);
  const auto *functionDecl = dyn_cast_or_null<FunctionDecl>(curCodeDecl);
  std::optional<uint64_t> declarationOrdinal =
      getAutomaticObjectDeclarationOrdinal(functionDecl, canonicalVariable);
  llvm::SmallString<256> declarationUSR;
  const bool hasDeclarationUSR =
      !clang::index::generateUSRForDecl(canonicalVariable, declarationUSR);
  if (!function || !declarationOrdinal) {
    cgm.errorNYI(variable->getSourceRange(),
                 "automatic cleanup allocation has no owning function or "
                 "FunctionDecl preorder identity");
    return std::nullopt;
  }

  SourceLocation begin = variable->getBeginLoc();
  SourceLocation end = variable->getEndLoc();
  QualType objectType = getContext().getBaseElementType(variable->getType());
  const CXXRecordDecl *record = objectType->getAsCXXRecordDecl();
  if (record && record->getDefinition())
    record = record->getDefinition();
  const CXXDestructorDecl *destructor =
      record ? record->getDestructor() : nullptr;
  if (begin.isInvalid() || end.isInvalid()) {
    cgm.errorNYI(variable->getSourceRange(),
                 "automatic cleanup declaration has no source range");
    return std::nullopt;
  }
  if (!destructor) {
    cgm.errorNYI(variable->getSourceRange(),
                 "automatic cleanup declaration has no destructor");
    return std::nullopt;
  }
  if (destructor->isTrivial()) {
    cgm.errorNYI(variable->getSourceRange(),
                 "automatic cleanup declaration has a trivial destructor");
    return std::nullopt;
  }

  CIRGenBuilderTy &builder = getBuilder();
  mlir::NamedAttrList identity;
  identity.set("function",
               mlir::FlatSymbolRefAttr::get(function.getSymNameAttr()));
  if (hasDeclarationUSR)
    identity.set("declaration_usr", builder.getStringAttr(declarationUSR));
  identity.set("declaration_ordinal",
               builder.getI64IntegerAttr(*declarationOrdinal));
  identity.set("begin_raw", builder.getI64IntegerAttr(begin.getRawEncoding()));
  identity.set("end_raw", builder.getI64IntegerAttr(end.getRawEncoding()));
  identity.set("declaration_is_macro_expansion",
               builder.getBoolAttr(variable->getLocation().isMacroID()));
  identity.set("cleanup_kind", builder.getStringAttr("cxx_destructor"));
  identity.set("destructor_symbol",
               builder.getStringAttr(
                   cgm.getMangledName(GlobalDecl(destructor, Dtor_Complete))));

  llvm::SmallString<256> destructorUSR;
  if (clang::index::generateUSRForDecl(destructor->getCanonicalDecl(),
                                       destructorUSR)) {
    cgm.errorNYI(variable->getSourceRange(),
                 "automatic cleanup destructor has no exact canonical USR");
    return std::nullopt;
  }
  identity.set("destructor_usr", builder.getStringAttr(destructorUSR));

  bool requiresObservedConstructorCall = false;
  bool hasAutomaticConstructorIdentity = false;
  const Expr *initializer = variable->getInit();
  while (initializer) {
    initializer = initializer->IgnoreParenImpCasts();
    if (const auto *cleanups = dyn_cast<ExprWithCleanups>(initializer)) {
      initializer = cleanups->getSubExpr();
      continue;
    }
    if (const auto *constant = dyn_cast<ConstantExpr>(initializer)) {
      initializer = constant->getSubExpr();
      continue;
    }
    break;
  }
  if (const auto *construct = dyn_cast_or_null<CXXConstructExpr>(initializer)) {
    if (const CXXConstructorDecl *constructor = construct->getConstructor()) {
      requiresObservedConstructorCall = !constructor->isTrivial();
      identity.set("constructor_symbol",
                   builder.getStringAttr(cgm.getMangledName(
                       GlobalDecl(constructor, Ctor_Complete))));
      llvm::SmallString<256> constructorUSR;
      if (clang::index::generateUSRForDecl(constructor->getCanonicalDecl(),
                                           constructorUSR)) {
        cgm.errorNYI(variable->getSourceRange(),
                     "automatic cleanup constructor has no exact canonical "
                     "USR");
        return std::nullopt;
      }
      identity.set("constructor_usr", builder.getStringAttr(constructorUSR));
      if (mlir::StringAttr ownerRecordUSR =
              cgm.exactRecordUSRAttr(constructor->getParent()))
        identity.set("constructor_owner_record_usr", ownerRecordUSR);
      hasAutomaticConstructorIdentity = true;
    }
  }
  if (!hasAutomaticConstructorIdentity) {
    // In guaranteed copy-elision, CIRGen may have already placed the one
    // bound temporary directly in this automatic VarDecl's alloca. The
    // singleton temporary dictionary is then the exact constructor producer
    // for this storage; carry that constructor identity onto the destination
    // dictionary before recording the transfer.
    mlir::ArrayAttr temporaryIdentities =
        alloca.getAstTemporaryObjectIdentitiesAttr();
    if (temporaryIdentities && temporaryIdentities.size() == 1) {
      auto temporaryIdentity =
          mlir::dyn_cast<mlir::DictionaryAttr>(temporaryIdentities[0]);
      auto temporaryFunction =
          temporaryIdentity
              ? temporaryIdentity.getAs<mlir::FlatSymbolRefAttr>("function")
              : mlir::FlatSymbolRefAttr{};
      auto temporaryDestructorUSR =
          temporaryIdentity
              ? temporaryIdentity.getAs<mlir::StringAttr>("destructor_usr")
              : mlir::StringAttr{};
      auto temporaryConstructor =
          temporaryIdentity
              ? temporaryIdentity.getAs<mlir::StringAttr>("constructor_symbol")
              : mlir::StringAttr{};
      auto temporaryConstructorUSR =
          temporaryIdentity
              ? temporaryIdentity.getAs<mlir::StringAttr>("constructor_usr")
              : mlir::StringAttr{};
      auto temporaryRequiresObserved =
          temporaryIdentity
              ? temporaryIdentity.getAs<mlir::BoolAttr>(
                    "requires_observed_constructor_call")
              : mlir::BoolAttr{};
      if (temporaryFunction &&
          temporaryFunction.getValue() == function.getSymName() &&
          temporaryDestructorUSR &&
          temporaryDestructorUSR.getValue() == destructorUSR &&
          temporaryConstructor && !temporaryConstructor.getValue().empty() &&
          temporaryConstructorUSR &&
          !temporaryConstructorUSR.getValue().empty() &&
          temporaryRequiresObserved) {
        identity.set("constructor_symbol", temporaryConstructor);
        identity.set("constructor_usr", temporaryConstructorUSR);
        if (auto ownerRecordUSR =
                temporaryIdentity.getAs<mlir::StringAttr>(
                    "constructor_owner_record_usr")) {
          identity.set("constructor_owner_record_usr", ownerRecordUSR);
        }
        requiresObservedConstructorCall =
            temporaryRequiresObserved.getValue();
      }
    }
  }
  identity.set("requires_observed_constructor_call",
               builder.getBoolAttr(requiresObservedConstructorCall));
  mlir::DictionaryAttr currentIdentity =
      identity.getDictionary(&getMLIRContext());
  // The canonical declaration, rather than its USR or source spelling, owns
  // this association while CIRGen is running. Distinct NRVO declarations may
  // share the physical return slot, so its singular sidecar is removed on a
  // conflict; each cleanup object still retains its own returned dictionary.
  auto [identityOwner, inserted] = automaticObjectIdentityDecls.try_emplace(
      alloca.getOperation(), canonicalVariable);
  if (!inserted &&
      (!identityOwner->second ||
       identityOwner->second != canonicalVariable)) {
    identityOwner->second = nullptr;
    alloca->removeAttr("ast_automatic_object_identity");
    return currentIdentity;
  }
  if (mlir::ArrayAttr temporaryIdentities =
          alloca.getAstTemporaryObjectIdentitiesAttr()) {
    llvm::SmallVector<mlir::Attribute> transferredIdentities;
    for (mlir::Attribute attribute : temporaryIdentities) {
      auto temporaryIdentity = mlir::dyn_cast<mlir::DictionaryAttr>(attribute);
      if (!temporaryIdentity)
        continue;
      mlir::NamedAttrList transferredIdentity(temporaryIdentity);
      transferredIdentity.set(
          "transferred_to_automatic_decl_ordinal",
          builder.getI64IntegerAttr(*declarationOrdinal));
      if (hasDeclarationUSR) {
        transferredIdentity.set("transferred_to_automatic_decl_usr",
                                builder.getStringAttr(declarationUSR));
      }
      transferredIdentities.push_back(
          transferredIdentity.getDictionary(&getMLIRContext()));
    }
    alloca.setAstTemporaryObjectIdentitiesAttr(
        builder.getArrayAttr(transferredIdentities));
  }
  if (identityOwner->second)
    alloca.setAstAutomaticObjectIdentityAttr(currentIdentity);
  return currentIdentity;
}

//===----------------------------------------------------------------------===//
// CIRGenFunction cleanup related
//===----------------------------------------------------------------------===//

/// Emits all the code to cause the given temporary to be cleaned up.
void CIRGenFunction::emitCXXTemporary(const CXXTemporary *temporary,
                                      QualType tempType, Address ptr,
                                      const CXXBindTemporaryExpr *binding) {
  setCXXBindTemporaryObjectIdentity(binding, temporary, ptr);
  pushDestroy(NormalAndEHCleanup, ptr, tempType, destroyCXXObject);
}

Address CIRGenFunction::createCleanupActiveFlag() {
  assert(isInConditionalBranch());
  mlir::Location loc = builder.getUnknownLoc();

  // Place the alloca in the function entry block so it dominates everything,
  // including both regions of any enclosing cir.cleanup.scope.  We can't rely
  // on the default curLexScope path because we may be inside a ternary branch
  // whose LexicalScope would capture the alloca.
  Address active = createTempAllocaWithoutCast(
      builder.getBoolTy(), CharUnits::One(), loc, "cleanup.cond",
      /*arraySize=*/nullptr,
      builder.getBestAllocaInsertPoint(getCurFunctionEntryBlock()));

  // Initialize to false before the outermost conditional. The conditional's
  // saved insertion point can precede an alloca added to the entry block after
  // that point was captured, so advance past that exact alloca when both are
  // in the same block. This preserves per-evaluation initialization in loops
  // while guaranteeing alloca -> false store -> conditional branch ordering.
  {
    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.restoreInsertionPoint(outermostConditional->getInsertPoint());
    cir::AllocaOp activeAlloca = active.getUnderlyingAllocaOp();
    if (activeAlloca &&
        activeAlloca->getBlock() == builder.getInsertionBlock()) {
      mlir::Block::iterator insertPoint = builder.getInsertionPoint();
      if (insertPoint != activeAlloca->getBlock()->end() &&
          !activeAlloca->isBeforeInBlock(&*insertPoint))
        builder.setInsertionPointAfter(activeAlloca);
    }
    builder.createFlagStore(loc, false, active.getPointer());
  }

  // Set to true at the current location (inside the conditional branch).
  builder.createFlagStore(loc, true, active.getPointer());

  return active;
}

void CIRGenFunction::initFullExprCleanup() {
  initFullExprCleanupWithFlag(createCleanupActiveFlag());
}

void CIRGenFunction::initFullExprCleanupWithFlag(Address activeFlag) {
  EHCleanupScope &cleanup = cast<EHCleanupScope>(*ehStack.begin());
  assert(!cleanup.hasActiveFlag() && "cleanup already has active flag?");
  cleanup.setActiveFlag(activeFlag);

  cleanup.setTestFlagInNormalCleanup(cleanup.isNormalCleanup());
  cleanup.setTestFlagInEHCleanup(cleanup.isEHCleanup());
}

CIRGenFunction::FullExprCleanupScope::FullExprCleanupScope(CIRGenFunction &cgf,
                                                           const Expr *subExpr)
    : cgf(cgf), cleanups(cgf), scope(nullptr),
      deferredCleanupStackSize(cgf.deferredConditionalCleanupStack.size()) {

  assert(subExpr && "ExprWithCleanups always has a sub-expression");
  ConditionalEvaluationFinder finder;
  finder.TraverseStmt(const_cast<Expr *>(subExpr));
  if (finder.found()) {
    mlir::Location loc = cgf.getLoc(subExpr->getSourceRange());
    cir::CleanupKind cleanupKind = cgf.getLangOpts().Exceptions
                                       ? cir::CleanupKind::All
                                       : cir::CleanupKind::Normal;
    scope = cir::CleanupScopeOp::create(
        cgf.builder, loc, cleanupKind,
        /*bodyBuilder=*/
        [&](mlir::OpBuilder &b, mlir::Location loc) {},
        /*cleanupBuilder=*/
        [&](mlir::OpBuilder &b, mlir::Location loc) {});
    cgf.builder.setInsertionPointToEnd(&scope.getBodyRegion().front());
  }
}

/// If the alloca that backs \p addr is currently nested inside the body
/// region of \p scope, hoist it, and any cast chain leading to it, out of the
// scope so the alloca dominates the scope's sibling cleanup region.
static void hoistAllocaOutOfCleanupScope(CIRGenFunction &cgf, Address addr,
                                         cir::CleanupScopeOp scope) {
  cir::AllocaOp alloca = addr.getUnderlyingAllocaOp();
  if (!alloca)
    return;

  // If the alloca is not contained within the cleanup scope we're currently
  // proccessing we don't need to hoist it.
  auto cur = alloca->getParentOfType<cir::CleanupScopeOp>();
  while (cur && cur != scope)
    cur = cur->getParentOfType<cir::CleanupScopeOp>();
  if (cur != scope)
    return;

  // Place the alloca at the canonical alloca insertion point of the block
  // containing the cleanup scope op, so it groups with any preceding
  // allocas / labels and dominates both the body and cleanup regions.
  mlir::Block *parentBlock = scope->getBlock();
  mlir::OpBuilder::InsertPoint ip =
      CIRGenBuilderTy::getBestAllocaInsertPoint(parentBlock);
  alloca->moveBefore(parentBlock, ip.getPoint());

  // Move any cast chain that consumes the alloca's result to immediately after
  // the alloca, so the address used by the deferred cleanup also dominates the
  // cleanup region. We walk down the chain starting from the alloca's user
  // that the Address was built from. This is very conservative. In practice,
  // we should only ever see alloca or address_space(alloca) operations here.
  mlir::Value ptr = addr.getPointer();
  llvm::SmallVector<cir::CastOp> casts;
  for (mlir::Operation *cur = ptr.getDefiningOp(); cur && cur != alloca;) {
    auto cast = mlir::dyn_cast<cir::CastOp>(cur);
    if (!cast)
      break;
    casts.push_back(cast);
    cur = cast.getSrc().getDefiningOp();
  }
  // Move casts in source order (closest to the alloca first).
  mlir::Operation *prev = alloca;
  for (cir::CastOp cast : llvm::reverse(casts)) {
    cast->moveAfter(prev);
    prev = cast;
  }
}

void CIRGenFunction::FullExprCleanupScope::exit(
    ArrayRef<mlir::Value *> valuesToReload) {
  assert(!exited && "FullExprCleanupScope::exit called twice");
  exited = true;

  size_t oldSize = deferredCleanupStackSize;
  bool hasDeferredCleanups =
      cgf.deferredConditionalCleanupStack.size() > oldSize;

  if (!scope) {
    cgf.deferredConditionalCleanupStack.truncate(oldSize);
    cleanups.forceCleanup(valuesToReload);
    return;
  }

  // Spill any values that callers need after the scope is closed.
  SmallVector<Address> tempAllocas;
  for (mlir::Value *valPtr : valuesToReload) {
    mlir::Value val = *valPtr;
    if (!val) {
      tempAllocas.push_back(Address::invalid());
      continue;
    }
    Address temp = cgf.createDefaultAlignTempAlloca(val.getType(), val.getLoc(),
                                                    "tmp.exprcleanup");
    tempAllocas.push_back(temp);
    cgf.builder.createStore(val.getLoc(), val, temp);
  }

  // Pop any EH cleanups that were pushed during the expression but leave
  // any lifetime-extended cleanups so that they can be promoted to the EH
  // stack after we've finished emitting any deferred cleanups.
  cleanups.forceCleanupExceptLifetimeExtended();

  // Make sure the cleanup scope body region has a terminator.
  {
    mlir::OpBuilder::InsertionGuard guard(cgf.builder);
    mlir::Block &lastBodyBlock = scope.getBodyRegion().back();
    cgf.builder.setInsertionPointToEnd(&lastBodyBlock);
    if (lastBodyBlock.empty() ||
        !lastBodyBlock.back().hasTrait<mlir::OpTrait::IsTerminator>())
      cgf.builder.createYield(scope.getLoc());
  }

  // Each deferred conditional cleanup will reference its addr from the
  // sibling cleanup region we are about to fill.  If the alloca that backs
  // that addr was created inside this scope's body region, hoist it out so it
  // dominates the cleanup region.
  if (hasDeferredCleanups) {
    for (const PendingCleanupEntry &entry :
         llvm::make_range(cgf.deferredConditionalCleanupStack.begin() + oldSize,
                          cgf.deferredConditionalCleanupStack.end())) {
      hoistAllocaOutOfCleanupScope(cgf, entry.addr, scope);
    }
  }

  // Emit any deferred cleanups.
  {
    mlir::OpBuilder::InsertionGuard guard(cgf.builder);
    mlir::Block &cleanupBlock = scope.getCleanupRegion().front();
    cgf.builder.setInsertionPointToEnd(&cleanupBlock);

    if (hasDeferredCleanups) {
      for (const PendingCleanupEntry &entry : llvm::reverse(llvm::make_range(
               cgf.deferredConditionalCleanupStack.begin() + oldSize,
               cgf.deferredConditionalCleanupStack.end()))) {
        if (entry.activeFlag.isValid()) {
          // We may have hoisted this alloca out of the cleanup scope. If so,
          // we will have also hoisted any casts between it and the address that
          // we stored in the deferredConditionalCleanupStack. While I can't
          // find a case where this actually happens, there is a theoretical
          // possibility that we could have a second address that uses an
          // alloca that has already been hoisted but a different cast chain.
          // This assert guards against that possibility.
          cir::AllocaOp alloca = entry.addr.getUnderlyingAllocaOp();
          assert(alloca &&
                 (alloca->getBlock() ==
                  entry.addr.getPointer().getDefiningOp()->getBlock()) &&
                 "alloca and cast are in different blocks");

          // Preserve every exact binding identity when one conditional cleanup
          // guards materialized storage shared by multiple alternatives.
          mlir::ArrayAttr conditionalCleanupIdentities;
          if (entry.destroyer == destroyCXXObject)
            conditionalCleanupIdentities =
                alloca.getAstTemporaryObjectIdentitiesAttr();

          mlir::Value flag =
              cgf.builder.createLoad(scope.getLoc(), entry.activeFlag);
          cir::IfOp cleanupGuard = cir::IfOp::create(
              cgf.builder, scope.getLoc(), flag, /*withElseRegion=*/false,
              [&](mlir::OpBuilder &b, mlir::Location loc) {
                cgf.emitDestroy(entry.addr, entry.type, entry.destroyer);
                cgf.builder.createYield(loc);
              });
          if (conditionalCleanupIdentities)
            cleanupGuard.setAstConditionalCleanupIdentitiesAttr(
                conditionalCleanupIdentities);
        } else {
          cgf.emitDestroy(entry.addr, entry.type, entry.destroyer);
        }
      }
    }
    cgf.builder.createYield(scope.getLoc());
  }

  cgf.deferredConditionalCleanupStack.truncate(oldSize);
  cgf.builder.setInsertionPointAfter(scope);

  // Promote any lifetime-extended cleanups onto the EH scope stack. The new
  // cir.cleanup.scope ops created here will wrap any code in the enclosing
  // scope, including reloads of any spilled values below, so the
  // lifetime-extended destructors run at the correct point.
  cleanups.forceLifetimeExtendedCleanups();

  // Reload spilled values now that the builder is after the closed scope.
  for (auto [addr, valPtr] : llvm::zip(tempAllocas, valuesToReload)) {
    if (!addr.isValid())
      continue;
    *valPtr = cgf.builder.createLoad(valPtr->getLoc(), addr);
  }
}

//===----------------------------------------------------------------------===//
// EHScopeStack
//===----------------------------------------------------------------------===//

void EHScopeStack::Cleanup::anchor() {}

EHScopeStack::stable_iterator
EHScopeStack::getInnermostActiveNormalCleanup() const {
  stable_iterator si = getInnermostNormalCleanup();
  stable_iterator se = stable_end();
  while (si != se) {
    EHCleanupScope &cleanup = llvm::cast<EHCleanupScope>(*find(si));
    if (cleanup.isActive())
      return si;
    si = cleanup.getEnclosingNormalCleanup();
  }
  return stable_end();
}

/// Push an entry of the given size onto this protected-scope stack.
char *EHScopeStack::allocate(size_t size) {
  size = llvm::alignTo(size, ScopeStackAlignment);
  if (!startOfBuffer) {
    unsigned capacity = llvm::PowerOf2Ceil(std::max<size_t>(size, 1024ul));
    startOfBuffer = std::make_unique<char[]>(capacity);
    startOfData = endOfBuffer = startOfBuffer.get() + capacity;
  } else if (static_cast<size_t>(startOfData - startOfBuffer.get()) < size) {
    unsigned currentCapacity = endOfBuffer - startOfBuffer.get();
    unsigned usedCapacity =
        currentCapacity - (startOfData - startOfBuffer.get());
    unsigned requiredCapacity = usedCapacity + size;
    // We know from the 'else if' condition that requiredCapacity is greater
    // than currentCapacity.
    unsigned newCapacity = llvm::PowerOf2Ceil(requiredCapacity);

    std::unique_ptr<char[]> newStartOfBuffer =
        std::make_unique<char[]>(newCapacity);
    char *newEndOfBuffer = newStartOfBuffer.get() + newCapacity;
    char *newStartOfData = newEndOfBuffer - usedCapacity;
    memcpy(newStartOfData, startOfData, usedCapacity);
    startOfBuffer.swap(newStartOfBuffer);
    endOfBuffer = newEndOfBuffer;
    startOfData = newStartOfData;
  }

  assert(startOfBuffer.get() + size <= startOfData);
  startOfData -= size;
  return startOfData;
}

void EHScopeStack::deallocate(size_t size) {
  startOfData += llvm::alignTo(size, ScopeStackAlignment);
}

void *EHScopeStack::pushCleanup(CleanupKind kind, size_t size) {
  char *buffer = allocate(EHCleanupScope::getSizeForCleanupSize(size));
  bool isNormalCleanup = kind & NormalCleanup;
  bool isEHCleanup = kind & EHCleanup;
  bool isLifetimeMarker = kind & LifetimeMarker;
  bool skipCleanupScope = false;

  cir::CleanupKind cleanupKind = cir::CleanupKind::All;
  if (isEHCleanup && cgf->getLangOpts().Exceptions) {
    cleanupKind =
        isNormalCleanup ? cir::CleanupKind::All : cir::CleanupKind::EH;
  } else {
    // Exceptions are disabled (or no EH flag was requested). Drop the EH
    // flag so the scope entry stays consistent with the op's cleanup kind.
    isEHCleanup = false;
    if (isNormalCleanup)
      cleanupKind = cir::CleanupKind::Normal;
    else
      skipCleanupScope = true;
  }

  // A loop condition variable's cleanups are represented by the loop op's
  // per-evaluation cleanup region rather than a nested cir.cleanup.scope.
  if (capturingLoopConditionCleanups)
    skipCleanupScope = true;

  cir::CleanupScopeOp cleanupScope = nullptr;
  if (!skipCleanupScope) {
    CIRGenBuilderTy &builder = cgf->getBuilder();
    mlir::Location loc = builder.getUnknownLoc();
    if (mlir::Block *block = builder.getInsertionBlock()) {
      auto insertionPoint = builder.getInsertionPoint();
      if (insertionPoint != block->begin())
        loc = std::prev(insertionPoint)->getLoc();
      for (mlir::Operation *parent = block->getParentOp();
           llvm::isa<mlir::UnknownLoc>(loc) && parent;
           parent = parent->getParentOp())
        loc = parent->getLoc();
    }
    cleanupScope = cir::CleanupScopeOp::create(
        builder, loc, cleanupKind,
        /*bodyBuilder=*/
        [&](mlir::OpBuilder &b, mlir::Location loc) {
          // Terminations will be handled in popCleanup
        },
        /*cleanupBuilder=*/
        [&](mlir::OpBuilder &b, mlir::Location loc) {
          // Terminations will be handled after emiting cleanup
        });

    builder.setInsertionPointToEnd(&cleanupScope.getBodyRegion().back());
  }

  // Per C++ [except.terminate], it is implementation-defined whether none,
  // some, or all cleanups are called before std::terminate. Thus, when
  // terminate is the current EH scope, we may skip adding any EH cleanup
  // scopes.
  if (innermostEHScope != stable_end() &&
      find(innermostEHScope)->getKind() == EHScope::Terminate)
    isEHCleanup = false;

  EHCleanupScope *scope = new (buffer)
      EHCleanupScope(isNormalCleanup, isEHCleanup, size, cleanupScope,
                     innermostNormalCleanup, innermostEHScope);

  if (isNormalCleanup)
    innermostNormalCleanup = stable_begin();

  if (isEHCleanup)
    innermostEHScope = stable_begin();

  if (isLifetimeMarker)
    cgf->cgm.errorNYI("push lifetime marker cleanup");

  // With Windows -EHa, Invoke llvm.seh.scope.begin() for EHCleanup
  if (cgf->getLangOpts().EHAsynch && isEHCleanup && !isLifetimeMarker &&
      cgf->getTarget().getCXXABI().isMicrosoft())
    cgf->cgm.errorNYI("push seh cleanup");

  return scope->getCleanupBuffer();
}

void EHScopeStack::popCleanup() {
  assert(!empty() && "popping exception stack when not empty");

  assert(isa<EHCleanupScope>(*begin()));
  EHCleanupScope &cleanup = cast<EHCleanupScope>(*begin());
  innermostNormalCleanup = cleanup.getEnclosingNormalCleanup();
  innermostEHScope = cleanup.getEnclosingEHScope();

  // Save everything needed from the scope before destroying it and releasing
  // its backing stack storage.
  cir::CleanupScopeOp cleanupScope = cleanup.getCleanupScopeOp();
  size_t allocatedSize = cleanup.getAllocatedSize();
  if (cleanupScope) {
    auto *block = &cleanupScope.getBodyRegion().back();
    if (!block->mightHaveTerminator()) {
      mlir::OpBuilder::InsertionGuard guard(cgf->getBuilder());
      cgf->getBuilder().setInsertionPointToEnd(block);
      cir::YieldOp::create(cgf->getBuilder(),
                           cgf->getBuilder().getUnknownLoc());
    }
    // If the insertion point was inside the cleanup scope we just closed, move
    // it to immediate after the scope.
    mlir::Block *insertBlock = cgf->getBuilder().getInsertionBlock();
    if (insertBlock &&
        cleanupScope.getBodyRegion().findAncestorBlockInRegion(*insertBlock))
      cgf->getBuilder().setInsertionPointAfter(cleanupScope);
  }

  // Destroy the cleanup before releasing its storage. No access through
  // `cleanup` is valid after deallocate().
  cleanup.destroy();
  deallocate(allocatedSize);
}

bool EHScopeStack::requiresCatchOrCleanup() const {
  for (stable_iterator si = getInnermostEHScope(); si != stable_end();) {
    if (auto *cleanup = dyn_cast<EHCleanupScope>(&*find(si))) {
      if (cleanup->isLifetimeMarker()) {
        // Skip lifetime markers and continue from the enclosing EH scope
        assert(!cir::MissingFeatures::emitLifetimeMarkers());
        continue;
      }
    }
    return true;
  }
  return false;
}

/// The given cleanup block is being deactivated. Configure a cleanup variable
/// if necessary.
static void setupCleanupBlockDeactivation(CIRGenFunction &cgf,
                                          EHScopeStack::stable_iterator c,
                                          mlir::Operation *dominatingIP) {
  EHCleanupScope &scope = cast<EHCleanupScope>(*cgf.ehStack.find(c));

  assert((scope.isNormalCleanup() || scope.isEHCleanup()) &&
         "cleanup block is neither normal nor EH?");

  scope.setTestFlagInNormalCleanup(scope.isNormalCleanup());
  scope.setTestFlagInEHCleanup(scope.isEHCleanup());

  CIRGenBuilderTy &builder = cgf.getBuilder();

  // If the cleanup block doesn't exist yet, create it and set its initial
  // value to `true`. If we are inside a conditional branch, the value must be
  // initialized before the conditional branch begins.
  Address var = scope.getActiveFlag();
  if (!var.isValid()) {
    mlir::Location loc = builder.getUnknownLoc();
    assert(dominatingIP && "no existing variable and no dominating IP!");

    if (cgf.isInConditionalBranch()) {
      // The cleanup only becomes active when this arm executes.
      var = cgf.createCleanupActiveFlag();
    } else {
      var = cgf.createTempAllocaWithoutCast(
          builder.getBoolTy(), CharUnits::One(), loc, "cleanup.isactive");
      mlir::OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPoint(dominatingIP);
      builder.createFlagStore(loc, true, var.getPointer());
    }
    scope.setActiveFlag(var);
  }

  // The code above sets the `isActive` flag to `true` as its initial state
  // at the point where the variable is created. The code below sets it to
  // `false` at the point where the cleanup is deactivated.
  mlir::Location loc = builder.getUnknownLoc();
  builder.createFlagStore(loc, false, var.getPointer());
}

/// Deactive a cleanup that was created in an active state.
void CIRGenFunction::deactivateCleanupBlock(EHScopeStack::stable_iterator c,
                                            mlir::Operation *dominatingIP,
                                            bool keepScope) {
  assert(c != ehStack.stable_end() && "deactivating bottom of stack?");
  EHCleanupScope &scope = cast<EHCleanupScope>(*ehStack.find(c));
  assert(scope.isActive() && "double deactivation");

  // If it's the top of the stack, just pop it, but do so only if it belongs
  // to the current RunCleanupsScope.
  if (!keepScope && c == ehStack.stable_begin() &&
      currentCleanupStackDepth.strictlyEncloses(c)) {
    popCleanupBlock(/*forDeactivation=*/true);
    return;
  }

  // Otherwise, follow the general case.
  setupCleanupBlockDeactivation(*this, c, dominatingIP);

  scope.setActive(false);
}

static void emitCleanupBody(CIRGenFunction &cgf, EHScopeStack::Cleanup *cleanup,
                            EHScopeStack::Cleanup::Flags flags,
                            Address activeFlag, mlir::Location loc) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  assert(cgf.haveInsertPoint() && "expected insertion point");

  if (activeFlag.isValid()) {
    mlir::Value isActive = builder.createFlagLoad(loc, activeFlag.getPointer());
    cir::IfOp::create(builder, loc, isActive,
                      /*withElseRegion=*/false,
                      /*thenBuilder=*/
                      [&](mlir::OpBuilder &, mlir::Location) {
                        cleanup->emit(cgf, flags);
                        assert(cgf.haveInsertPoint() &&
                               "cleanup ended with no insertion point?");
                        builder.createYield(loc);
                      });
    return;
  }

  cleanup->emit(cgf, flags);
  assert(cgf.haveInsertPoint() && "cleanup ended with no insertion point?");
}

static void emitCleanup(CIRGenFunction &cgf, cir::CleanupScopeOp cleanupScope,
                        EHScopeStack::Cleanup *cleanup,
                        EHScopeStack::Cleanup::Flags flags,
                        Address activeFlag) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Block &block = cleanupScope.getCleanupRegion().back();

  mlir::OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointToStart(&block);

  emitCleanupBody(cgf, cleanup, flags, activeFlag, cleanupScope.getLoc());

  mlir::Block &cleanupRegionLastBlock = cleanupScope.getCleanupRegion().back();
  if (cleanupRegionLastBlock.empty() ||
      !cleanupRegionLastBlock.back().hasTrait<mlir::OpTrait::IsTerminator>()) {
    mlir::OpBuilder::InsertionGuard guardCase(builder);
    builder.setInsertionPointToEnd(&cleanupRegionLastBlock);
    builder.createYield(cleanupScope.getLoc());
  }
}

/// Check whether a cleanup scope body contains any non-yield exits that branch
/// through the cleanup. These exits branch through the cleanup and require
/// the normal cleanup to be executed even when the cleanup has been
/// deactivated.
static bool bodyHasBranchThroughExits(mlir::Region &bodyRegion) {
  return bodyRegion
      .walk([&](mlir::Operation *op) {
        if (isa<cir::ReturnOp, cir::GotoOp>(op))
          return mlir::WalkResult::interrupt();
        return mlir::WalkResult::advance();
      })
      .wasInterrupted();
}

/// Pop a cleanup block from the stack.
///
/// \param forDeactivation - When true, this indicates that the cleanup block
/// is being popped because it was deactivated while at the top of the stack.
void CIRGenFunction::popCleanupBlock(bool forDeactivation) {
  assert(!ehStack.empty() && "cleanup stack is empty!");
  assert(isa<EHCleanupScope>(*ehStack.begin()) && "top not a cleanup!");
  EHCleanupScope &scope = cast<EHCleanupScope>(*ehStack.begin());

  // If we pushed an EH-only cleanup but exceptions are disabled, it will leave
  // an effectively empty cleanup on the EH stack. In that case, there is
  // nothing to do here except pop the cleanup.
  cir::CleanupScopeOp cleanupScope = scope.getCleanupScopeOp();
  if (!cleanupScope) {
    assert(!scope.isNormalCleanup() && !scope.isEHCleanup() &&
           "missing cir.cleanup.scope for active cleanup");
    ehStack.popCleanup();
    return;
  }

  bool requiresNormalCleanup = scope.isNormalCleanup();
  bool requiresEHCleanup = scope.isEHCleanup();

  // When we're popping a cleanup to deactivate it, we need to know if anything
  // in the cleanup scope body region branches through the cleanup handler
  // before the entire cleanup scope body has executed. If the cleanup scope
  // body falls through, we don't want to emit normal cleanup code. However,
  // if the cleanup body region contains early exits (return or goto), we do
  // need to execute the normal cleanup when the early exit is taken. To handle
  // that case, we guard the cleanup with an "active" flag so that it executes
  // conditionally and set the flag to false when the cleanup body falls
  // through. Classic codegen tracks this state with "hasBranches" and
  // "getFixupDepth" on the cleanup scope, but because CIR uses structured
  // control flow, we need to check for early exits and insert the active
  // flag handling here. Note that when a cleanup is deactivated while not at
  // the top of the stack, the active flag gets created in
  // setupCleanupBlockDeactivation.
  if (forDeactivation && requiresNormalCleanup) {
    if (bodyHasBranchThroughExits(cleanupScope.getBodyRegion())) {
      // The active flag shouldn't exist if the scope was at the top of the
      // stack when it was deactivated.
      assert(!scope.getActiveFlag().isValid() && "active flag already set");

      // Create the flag.
      mlir::Location loc = builder.getUnknownLoc();
      Address activeFlag = createTempAllocaWithoutCast(
          builder.getBoolTy(), CharUnits::One(), loc, "cleanup.isactive");

      // Initialize the flag to true before the cleanup scope (the point where
      // the cleanup becomes active).
      {
        mlir::OpBuilder::InsertionGuard guard(builder);
        builder.setInsertionPoint(cleanupScope);
        builder.createFlagStore(loc, true, activeFlag.getPointer());
      }

      // Set the flag to false at the end of the cleanup scope body region.
      assert(builder.getInsertionBlock() ==
                 &cleanupScope.getBodyRegion().back() &&
             "expected insertion point in cleanup body");
      builder.createFlagStore(loc, false, activeFlag.getPointer());

      scope.setActiveFlag(activeFlag);
      scope.setTestFlagInNormalCleanup(true);
    } else {
      // If the cleanup was pushed on the stack as normal+eh, downgrade it to
      // eh-only.
      if (requiresEHCleanup)
        cleanupScope.setCleanupKind(cir::CleanupKind::EH);
      requiresNormalCleanup = false;
    }
  }

  Address normalActiveFlag = scope.shouldTestFlagInNormalCleanup()
                                 ? scope.getActiveFlag()
                                 : Address::invalid();
  Address ehActiveFlag = scope.shouldTestFlagInEHCleanup()
                             ? scope.getActiveFlag()
                             : Address::invalid();

  // If we don't need the cleanup at all, we're done.
  if (!requiresNormalCleanup && !requiresEHCleanup) {
    // If we get here, the cleanup scope isn't needed. Rather than try to move
    // the contents of its body region out of the cleanup and erase it, we just
    // add a yield to the cleanup region to make it valid but no-op. It will be
    // erased during canonicalization.
    mlir::Block &cleanupBlock = cleanupScope.getCleanupRegion().back();
    if (!cleanupBlock.mightHaveTerminator()) {
      mlir::OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPointToEnd(&cleanupBlock);
      cir::YieldOp::create(builder, builder.getUnknownLoc());
    }
    ehStack.popCleanup();
    return;
  }

  // Copy the cleanup emission data out.  This uses either a stack
  // array or malloc'd memory, depending on the size, which is
  // behavior that SmallVector would provide, if we could use it
  // here. Unfortunately, if you ask for a SmallVector<char>, the
  // alignment isn't sufficient.
  auto *cleanupSource = reinterpret_cast<char *>(scope.getCleanupBuffer());
  alignas(EHScopeStack::ScopeStackAlignment) char
      cleanupBufferStack[8 * sizeof(void *)];
  std::unique_ptr<char[]> cleanupBufferHeap;
  size_t cleanupSize = scope.getCleanupSize();
  EHScopeStack::Cleanup *cleanup;

  // This is necessary because we are going to deallocate the cleanup
  // (in popCleanup) before we emit it.
  if (cleanupSize <= sizeof(cleanupBufferStack)) {
    memcpy(cleanupBufferStack, cleanupSource, cleanupSize);
    cleanup = reinterpret_cast<EHScopeStack::Cleanup *>(cleanupBufferStack);
  } else {
    cleanupBufferHeap.reset(new char[cleanupSize]);
    memcpy(cleanupBufferHeap.get(), cleanupSource, cleanupSize);
    cleanup =
        reinterpret_cast<EHScopeStack::Cleanup *>(cleanupBufferHeap.get());
  }

  EHScopeStack::Cleanup::Flags cleanupFlags;
  if (scope.isNormalCleanup())
    cleanupFlags.setIsNormalCleanupKind();
  if (scope.isEHCleanup())
    cleanupFlags.setIsEHCleanupKind();

  // Determine the active flag for the cleanup handler.
  Address cleanupActiveFlag = normalActiveFlag.isValid() ? normalActiveFlag
                              : ehActiveFlag.isValid()   ? ehActiveFlag
                                                         : Address::invalid();

  // In CIR, the cleanup code is emitted into the cleanup region of the
  // cir.cleanup.scope op. There is no CFG threading needed — the FlattenCFG
  // pass handles lowering the structured cleanup scope.
  scope.markEmitted();
  ehStack.popCleanup();
  emitCleanup(*this, cleanupScope, cleanup, cleanupFlags, cleanupActiveFlag);
}

void CIRGenFunction::initLoopConditionCleanupsWithFlag(
    EHScopeStack::stable_iterator depth, Address activeFlag) {
  assert(activeFlag.isValid() && "condition cleanup needs an active flag");

  for (auto it = ehStack.begin(), end = ehStack.find(depth); it != end; ++it) {
    EHCleanupScope &scope = cast<EHCleanupScope>(*it);
    assert(!scope.getCleanupScopeOp() &&
           "captured loop-condition cleanup owns a cleanup scope");
    assert(!scope.hasActiveFlag() && "cleanup already has an active flag");
    scope.setActiveFlag(activeFlag);
    scope.setTestFlagInNormalCleanup(scope.isNormalCleanup());
    scope.setTestFlagInEHCleanup(scope.isEHCleanup());
  }
}

void CIRGenFunction::emitLoopConditionCleanups(
    EHScopeStack::stable_iterator depth, mlir::Location loc) {
  while (ehStack.stable_begin() != depth) {
    assert(isa<EHCleanupScope>(*ehStack.begin()) && "top not a cleanup!");
    EHCleanupScope &scope = cast<EHCleanupScope>(*ehStack.begin());
    assert(!scope.getCleanupScopeOp() &&
           "captured loop-condition cleanup owns a cleanup scope");

    EHScopeStack::Cleanup::Flags cleanupFlags;
    if (scope.isNormalCleanup())
      cleanupFlags.setIsNormalCleanupKind();
    if (scope.isEHCleanup())
      cleanupFlags.setIsEHCleanupKind();

    Address activeFlag = scope.shouldTestFlagInNormalCleanup() ||
                                 scope.shouldTestFlagInEHCleanup()
                             ? scope.getActiveFlag()
                             : Address::invalid();

    auto *cleanupSource = reinterpret_cast<char *>(scope.getCleanupBuffer());
    alignas(EHScopeStack::ScopeStackAlignment) char
        cleanupBufferStack[8 * sizeof(void *)];
    std::unique_ptr<char[]> cleanupBufferHeap;
    size_t cleanupSize = scope.getCleanupSize();
    EHScopeStack::Cleanup *cleanup;
    if (cleanupSize <= sizeof(cleanupBufferStack)) {
      memcpy(cleanupBufferStack, cleanupSource, cleanupSize);
      cleanup = reinterpret_cast<EHScopeStack::Cleanup *>(cleanupBufferStack);
    } else {
      cleanupBufferHeap.reset(new char[cleanupSize]);
      memcpy(cleanupBufferHeap.get(), cleanupSource, cleanupSize);
      cleanup =
          reinterpret_cast<EHScopeStack::Cleanup *>(cleanupBufferHeap.get());
    }

    scope.markEmitted();
    ehStack.popCleanup();
    emitCleanupBody(*this, cleanup, cleanupFlags, activeFlag, loc);
  }
}

/// Pops cleanup blocks until the given savepoint is reached.
void CIRGenFunction::popCleanupBlocks(
    EHScopeStack::stable_iterator oldCleanupStackDepth,
    ArrayRef<mlir::Value *> valuesToReload) {
  // If the current stack depth is the same as the cleanup stack depth,
  // we won't be exiting any cleanup scopes, so we don't need to reload
  // any values.
  bool requiresCleanup = false;
  for (auto it = ehStack.begin(), ie = ehStack.find(oldCleanupStackDepth);
       it != ie; ++it) {
    if (isa<EHCleanupScope>(&*it)) {
      requiresCleanup = true;
      break;
    }
  }

  // If there are values that we need to keep live, spill them now before
  // we pop the cleanup blocks. These are passed as pointers to mlir::Value
  // because we're going to replace them with the reloaded value.
  SmallVector<Address> tempAllocas;
  if (requiresCleanup) {
    for (mlir::Value *valPtr : valuesToReload) {
      mlir::Value val = *valPtr;
      if (!val)
        continue;

      // TODO(cir): Check for static allocas.

      Address temp = createDefaultAlignTempAlloca(val.getType(), val.getLoc(),
                                                  "tmp.exprcleanup");
      tempAllocas.push_back(temp);
      builder.createStore(val.getLoc(), val, temp);
    }
  }

  // Pop cleanup blocks until we reach the base stack depth for the
  // current scope.
  while (ehStack.stable_begin() != oldCleanupStackDepth)
    popCleanupBlock();

  // Reload the values that we spilled, if necessary.
  if (requiresCleanup) {
    for (auto [addr, valPtr] : llvm::zip(tempAllocas, valuesToReload)) {
      mlir::Location loc = valPtr->getLoc();
      *valPtr = builder.createLoad(loc, addr);
    }
  }
}

/// Pops cleanup blocks until the given savepoint is reached, then add the
/// cleanups from the given savepoint in the lifetime-extended cleanups stack.
void CIRGenFunction::popCleanupBlocks(
    EHScopeStack::stable_iterator oldCleanupStackDepth,
    size_t oldLifetimeExtendedSize, ArrayRef<mlir::Value *> valuesToReload) {
  popCleanupBlocks(oldCleanupStackDepth, valuesToReload);

  // Promote deferred lifetime-extended cleanups onto the EH scope stack.
  for (const PendingCleanupEntry &cleanup : llvm::make_range(
           lifetimeExtendedCleanupStack.begin() + oldLifetimeExtendedSize,
           lifetimeExtendedCleanupStack.end()))
    pushPendingCleanupToEHStack(cleanup);
  lifetimeExtendedCleanupStack.truncate(oldLifetimeExtendedSize);
}
