//===--- CIRGenerator.cpp - Emit CIR from ASTs ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This builds an AST and converts it to CIR.
//
//===----------------------------------------------------------------------===//

#include "CIRGenModule.h"

#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Dialect/OpenACC/OpenACC.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Target/LLVMIR/Import.h"

#include "clang/AST/DeclGroup.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/CIR/CIRGenerator.h"
#include "clang/CIR/InitAllDialects.h"
#include "clang/Sema/Sema.h"
#include "llvm/IR/DataLayout.h"

using namespace cir;
using namespace clang;

void CIRGenerator::anchor() {}

CIRGenerator::CIRGenerator(clang::DiagnosticsEngine &diags,
                           llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs,
                           const CodeGenOptions &cgo)
    : diags(diags), fs(std::move(vfs)), codeGenOpts{cgo},
      handlingTopLevelDecls{0} {}
CIRGenerator::~CIRGenerator() {
  // There should normally not be any leftover inline method definitions.
  assert(deferredInlineMemberFuncDefs.empty() || diags.hasErrorOccurred());
}

static void setMLIRDataLayout(mlir::ModuleOp &mod, const llvm::DataLayout &dl) {
  mlir::MLIRContext *mlirContext = mod.getContext();
  mlir::DataLayoutSpecInterface dlSpec =
      mlir::translateDataLayout(dl, mlirContext);
  mod->setAttr(mlir::DLTIDialect::kDataLayoutAttrName, dlSpec);
}

void CIRGenerator::Initialize(ASTContext &astContext) {
  using namespace llvm;

  this->astContext = &astContext;

  mlirContext = std::make_unique<mlir::MLIRContext>();
  cir::registerAllDialects(*mlirContext);
  mlirContext->loadDialect<mlir::DLTIDialect, cir::CIRDialect>();
  mlirContext->getOrLoadDialect<mlir::acc::OpenACCDialect>();
  mlirContext->getOrLoadDialect<mlir::omp::OpenMPDialect>();

  cgm = std::make_unique<clang::CIRGen::CIRGenModule>(
      *mlirContext.get(), astContext, codeGenOpts, diags);
  mlir::ModuleOp mod = cgm->getModule();
  llvm::DataLayout layout =
      llvm::DataLayout(astContext.getTargetInfo().getDataLayoutString());
  setMLIRDataLayout(mod, layout);
  cgm->initializeDataLayout();
}

void CIRGenerator::InitializeSema(Sema &sema) { this->sema = &sema; }

void CIRGenerator::ForgetSema() { sema = nullptr; }

namespace {

class CastEndpointRecordCollector
    : public RecursiveASTVisitor<CastEndpointRecordCollector> {
  llvm::DenseSet<const Type *> seen;

  void collect(QualType type, SourceLocation loc) {
    while (!type.isNull()) {
      type = type.getCanonicalType();
      if (type->isPointerType() || type->isReferenceType()) {
        type = type->getPointeeType();
        continue;
      }
      if (const auto *array = type->getAsArrayTypeUnsafe()) {
        type = array->getElementType();
        continue;
      }
      break;
    }
    if (!type.isNull() && seen.insert(type.getTypePtr()).second)
      endpoints.emplace_back(type, loc);
  }

public:
  llvm::SmallVector<std::pair<QualType, SourceLocation>, 8> endpoints;

  bool shouldVisitImplicitCode() const { return true; }
  bool shouldVisitTemplateInstantiations() const { return true; }

  bool VisitCastExpr(CastExpr *cast) {
    if (!cast)
      return true;
    collect(cast->getSubExpr()->getType(), cast->getExprLoc());
    collect(cast->getType(), cast->getExprLoc());
    return true;
  }
};

void prepareCastEndpointRecordSchemas(Sema *sema, Decl *decl) {
  if (!sema || !decl)
    return;
  CastEndpointRecordCollector collector;
  collector.TraverseDecl(decl);
  for (const auto &[type, loc] : collector.endpoints) {
    auto *specialization =
        dyn_cast_or_null<ClassTemplateSpecializationDecl>(
            type->getAsCXXRecordDecl());
    if (!specialization)
      continue;
    // The RecordType can retain an earlier declaration after Sema has assigned
    // the specialization to a later definition. Make the owning definition,
    // rather than the declaration embedded in the type, authoritative.
    specialization = specialization->getDefinitionOrSelf();
    if (specialization->isCompleteDefinition())
      continue;
    const TemplateSpecializationKind kind =
        specialization->getSpecializationKind();
    if (kind != TSK_Undeclared && kind != TSK_ImplicitInstantiation)
      continue;
    ClassTemplateDecl *primary = specialization->getSpecializedTemplate();
    if (!primary || !primary->getTemplatedDecl()->getDefinition())
      continue;
    // CIR cast endpoint schemas are direct producer facts. Complete only an
    // implicit specialization whose definition is structurally available;
    // genuinely opaque records remain valid incomplete endpoints.
    const QualType endpointType = type;
    const SourceLocation endpointLoc = loc;
    sema->runWithSufficientStackSpace(endpointLoc, [sema, endpointType,
                                                    endpointLoc] {
      sema->RequireCompleteType(endpointLoc, endpointType,
                                diag::err_incomplete_type);
    });
  }
}

class SelectedBodyVariableUseRestorer
    : public RecursiveASTVisitor<SelectedBodyVariableUseRestorer> {
  ASTContext &context;

public:
  explicit SelectedBodyVariableUseRestorer(ASTContext &context)
      : context(context) {}

  bool VisitDeclRefExpr(DeclRefExpr *expr) {
    if (auto *variable = dyn_cast<VarDecl>(expr->getDecl()))
      variable->markUsed(context);
    return true;
  }
};

void restoreSelectedBodyVariableUses(ASTContext &context,
                                     FunctionDecl *function) {
  if (Stmt *body = function->getBody()) {
    SelectedBodyVariableUseRestorer restorer(context);
    restorer.TraverseStmt(body);
  }
}

} // namespace

void CIRGenerator::prepareSelectedLocalClassMembers(TranslationUnitDecl *tu) {
  if (!sema || codeGenOpts.ClangIRSelectedDeclsFile.empty())
    return;

  class LocalClassCollector
      : public RecursiveASTVisitor<LocalClassCollector> {
    CIRGen::CIRGenModule &cgm;
    llvm::DenseSet<CXXRecordDecl *> visited;

    void collect(CXXRecordDecl *record) {
      if (!record || (!record->isLocalClass() && !record->isLambda()))
        return;
      CXXRecordDecl *definition = record->getDefinition();
      if (!definition || definition->isDependentContext() ||
          !visited.insert(definition).second)
        return;
      records.push_back(definition);
    }

  public:
    using Base = RecursiveASTVisitor<LocalClassCollector>;
    llvm::SmallVector<CXXRecordDecl *, 16> records;

    explicit LocalClassCollector(CIRGen::CIRGenModule &cgm) : cgm(cgm) {}
    bool shouldVisitImplicitCode() const { return true; }
    bool shouldVisitTemplateInstantiations() const { return true; }


    bool TraverseFunctionDecl(FunctionDecl *function) {
      if (!function || !cgm.shouldParseSelectedDeclBody(function))
        return true;
      return Base::TraverseFunctionDecl(function);
    }

    bool VisitCXXRecordDecl(CXXRecordDecl *record) {
      collect(record);
      return true;
    }

    bool VisitLambdaExpr(LambdaExpr *lambda) {
      collect(lambda ? lambda->getLambdaClass() : nullptr);
      return true;
    }
  };

  LocalClassCollector collector(*cgm);
  collector.TraverseDecl(tu);
  for (CXXRecordDecl *record : collector.records) {
    SourceLocation loc = record->getLocation();
    sema->runWithSufficientStackSpace(
        loc, [&] { sema->ForceDeclarationOfImplicitMembers(record); });
    // ForceDeclaration makes the implicit member declaration available, but
    // a skipped enclosing body does not ODR-use it and therefore does not ask
    // Sema for a definition. An exact selected structor symbol is that request:
    // define only the compiler-owned defaulted destructor it authenticates.
    if (auto *dtor = record->getDestructor();
        dtor && cgm->shouldParseSelectedDeclBody(dtor))
      defineSelectedDefaultedMethod(dtor);
  }
}


void CIRGenerator::defineSelectedDefaultedMethod(CXXMethodDecl *method) {
  assert(sema && "selected defaulted method definition requires Sema");
  if (method->getParent()->isDependentContext() || !method->isDefaulted() ||
      method->isDeleted() || method->doesThisDeclarationHaveABody() ||
      !cgm->shouldParseSelectedDeclBody(method))
    return;

  SourceLocation loc = method->getLocation();
  sema->runWithSufficientStackSpace(loc, [&] {
    if (auto *ctor = dyn_cast<CXXConstructorDecl>(method)) {
      if (ctor->isDefaultConstructor())
        sema->DefineImplicitDefaultConstructor(loc, ctor);
      else if (ctor->isCopyConstructor())
        sema->DefineImplicitCopyConstructor(loc, ctor);
      else if (ctor->isMoveConstructor())
        sema->DefineImplicitMoveConstructor(loc, ctor);
    } else if (auto *dtor = dyn_cast<CXXDestructorDecl>(method)) {
      sema->DefineImplicitDestructor(loc, dtor);
    } else if (method->isCopyAssignmentOperator()) {
      sema->DefineImplicitCopyAssignment(loc, method);
    } else if (method->isMoveAssignmentOperator()) {
      sema->DefineImplicitMoveAssignment(loc, method);
    } else {
      auto comparisonKind = sema->getDefaultedComparisonKind(method);
      if (comparisonKind != Sema::DefaultedComparisonKind::None)
        sema->DefineDefaultedComparison(loc, method, comparisonKind);
    }
  });
}

void CIRGenerator::prepareSelectedMethods(
    llvm::MutableArrayRef<GlobalDecl> globals) {
  llvm::DenseSet<const FunctionDecl *> materialized;
  for (GlobalDecl &gd : globals) {
    auto *function =
        const_cast<FunctionDecl *>(dyn_cast<FunctionDecl>(gd.getDecl()));
    if (!function)
      continue;

    const bool firstMaterialization =
        materialized.insert(function->getCanonicalDecl()).second;
    if (firstMaterialization) {
      const FunctionDecl *pattern =
          function->getTemplateInstantiationPattern();
      if (!function->doesThisDeclarationHaveABody() &&
          !function->isDefaulted() &&
          cgm->shouldParseSelectedDeclBody(function) && pattern &&
          pattern->getDefinition()) {
        SourceLocation loc = function->getLocation();
        sema->runWithSufficientStackSpace(loc, [&] {
          sema->InstantiateFunctionDefinition(
              loc, function, /*Recursive=*/true,
              /*DefinitionRequired=*/true, /*AtEndOfTU=*/true);
        });
      }
      if (auto *method = dyn_cast<CXXMethodDecl>(function))
        defineSelectedDefaultedMethod(method);
    }

    // Sema can attach an instantiated body to a later redeclaration. Carry the
    // exact concrete definition into both selected endpoint preparation and
    // CIR emission instead of traversing the declaration that originally
    // authenticated the work item.
    if (const FunctionDecl *definition = function->getDefinition()) {
      gd = gd.getWithDecl(definition);
      function = const_cast<FunctionDecl *>(definition);
    }
    // A selected specialization can acquire a body from a skipped enclosing
    // function without replaying Sema's ordinary DeclRefExpr callbacks. The
    // body remains the authoritative source of variable uses, so restore those
    // exact facts before CIRGen enforces them.
    restoreSelectedBodyVariableUses(*astContext, function);

    if (firstMaterialization)
      prepareCastEndpointRecordSchemas(sema, function);
  }
}

bool CIRGenerator::verifyModule() const { return cgm->verifyModule(); }

mlir::ModuleOp CIRGenerator::getModule() const { return cgm->getModule(); }

bool CIRGenerator::HandleTopLevelDecl(DeclGroupRef group) {
  if (diags.hasUnrecoverableErrorOccurred())
    return true;

  HandlingTopLevelDeclRAII handlingDecl(*this);

  for (Decl *decl : group)
    cgm->emitTopLevelDecl(decl);

  return true;
}

void CIRGenerator::HandleTranslationUnit(ASTContext &astContext) {
  if (!diags.hasErrorOccurred() && cgm) {
    // Requiring a cast endpoint to be complete is a Sema instantiation action.
    // Delay ordinary-mode completion until explicit specializations have been
    // assigned their final owners at the end of the translation unit.
    if (sema && codeGenOpts.ClangIRSelectedDeclsFile.empty())
      prepareCastEndpointRecordSchemas(sema,
                                       astContext.getTranslationUnitDecl());
    prepareSelectedLocalClassMembers(astContext.getTranslationUnitDecl());
    // Materialize declarations discovered by ordinary AST callbacks before
    // selected-root closure scans specialization and destructor families.
    cgm->emitDeferred();
    // Discover selected roots once. Defining or emitting a root can instantiate
    // another specialization, so each stable root or exact dependency frontier
    // is prepared before emission without another translation-unit walk.
    cgm->emitSelectedMethods(astContext.getTranslationUnitDecl(),
                             [&](llvm::MutableArrayRef<GlobalDecl> methods) {
                               if (sema)
                                 prepareSelectedMethods(methods);
                             });
    cgm->emitSelectedVariables(astContext.getTranslationUnitDecl());
    cgm->emitSelectedDeclDependencyClosure(
        [&](llvm::MutableArrayRef<GlobalDecl> dependencies) {
          if (sema)
            prepareSelectedMethods(dependencies);
        });
    cgm->release();
  }

  // If there are errors before or when releasing the cgm, reset the module to
  // stop here before invoking the backend.
  assert(!cir::MissingFeatures::cleanupAfterErrorDiags());
}

void CIRGenerator::HandleInlineFunctionDefinition(FunctionDecl *d) {
  if (diags.hasErrorOccurred())
    return;

  if (!cgm->shouldParseSelectedDeclBody(d))
    return;

  assert(d->doesThisDeclarationHaveABody());

  // We may want to emit this definition. However, that decision might be
  // based on computing the linkage, and we have to defer that in case we are
  // inside of something that will chagne the method's final linkage, e.g.
  //   typedef struct {
  //     void bar();
  //     void foo() { bar(); }
  //   } A;
  deferredInlineMemberFuncDefs.push_back(d);

  // Provide some coverage mapping even for methods that aren't emitted.
  // Don't do this for templated classes though, as they may not be
  // instantiable.
  assert(!cir::MissingFeatures::coverageMapping());
}

void CIRGenerator::emitDeferredDecls() {
  if (deferredInlineMemberFuncDefs.empty())
    return;

  // Emit any deferred inline method definitions. Note that more deferred
  // methods may be added during this loop, since ASTConsumer callbacks can be
  // invoked if AST inspection results in declarations being added. Therefore,
  // we use an index to loop over the deferredInlineMemberFuncDefs rather than
  // a range.
  HandlingTopLevelDeclRAII handlingDecls(*this);
  for (unsigned i = 0; i != deferredInlineMemberFuncDefs.size(); ++i)
    cgm->emitTopLevelDecl(deferredInlineMemberFuncDefs[i]);
  deferredInlineMemberFuncDefs.clear();
}

/// HandleTagDeclDefinition - This callback is invoked each time a TagDecl to
/// (e.g. struct, union, enum, class) is completed. This allows the client to
/// hack on the type, which can occur at any point in the file (because these
/// can be defined in declspecs).
void CIRGenerator::HandleTagDeclDefinition(TagDecl *d) {
  if (diags.hasErrorOccurred())
    return;

  if (!codeGenOpts.ClangIRSelectedDeclsFile.empty())
    return;

  // Don't allow re-entrant calls to CIRGen triggered by PCH deserialization to
  // emit deferred decls.
  HandlingTopLevelDeclRAII handlingDecl(*this, /*EmitDeferred=*/false);

  cgm->updateCompletedType(d);

  // For MSVC compatibility, treat declarations of static data members with
  // inline initializers as definitions.
  if (astContext->getTargetInfo().getCXXABI().isMicrosoft())
    cgm->errorNYI(d->getSourceRange(), "HandleTagDeclDefinition: MSABI");

  // For OpenMP emit declare reduction functions or declare mapper, if
  // required.
  if (astContext->getLangOpts().OpenMP) {
    for (Decl *member : d->decls()) {
      if (auto *drd = dyn_cast<OMPDeclareReductionDecl>(member)) {
        if (astContext->DeclMustBeEmitted(drd))
          cgm->errorNYI(d->getSourceRange(),
                        "HandleTagDeclDefinition: OMPDeclareReductionDecl");
      } else if (auto *dmd = dyn_cast<OMPDeclareMapperDecl>(member)) {
        if (astContext->DeclMustBeEmitted(dmd))
          cgm->errorNYI(d->getSourceRange(),
                        "HandleTagDeclDefinition: OMPDeclareMapperDecl");
      }
    }
  }
}

void CIRGenerator::HandleTagDeclRequiredDefinition(const TagDecl *D) {
  if (diags.hasErrorOccurred())
    return;

  assert(!cir::MissingFeatures::generateDebugInfo());
}

void CIRGenerator::HandleCXXStaticMemberVarInstantiation(VarDecl *D) {
  if (diags.hasErrorOccurred())
    return;

  if (!cgm->shouldEmitSelectedDeclRoot(GlobalDecl(D)))
    return;

  cgm->handleCXXStaticMemberVarInstantiation(D);
}

void CIRGenerator::HandleOpenACCRoutineReference(const FunctionDecl *FD,
                                                 const OpenACCRoutineDecl *RD) {
  llvm::StringRef mangledName = cgm->getMangledName(FD);
  cir::FuncOp entry =
      mlir::dyn_cast_if_present<cir::FuncOp>(cgm->getGlobalValue(mangledName));

  // if this wasn't generated, don't force it to be.
  if (!entry)
    return;
  cgm->emitOpenACCRoutineDecl(FD, entry, RD->getBeginLoc(), RD->clauses());
}

void CIRGenerator::CompleteTentativeDefinition(VarDecl *d) {
  if (diags.hasErrorOccurred())
    return;

  if (!cgm->shouldEmitSelectedDeclRoot(GlobalDecl(d)))
    return;

  cgm->emitTentativeDefinition(d);
}

void CIRGenerator::HandleVTable(CXXRecordDecl *rd) {
  if (diags.hasErrorOccurred())
    return;

  if (!codeGenOpts.ClangIRSelectedDeclsFile.empty())
    return;

  cgm->emitVTable(rd);
}

bool CIRGenerator::shouldSkipFunctionBody(Decl *d) {
  if (codeGenOpts.ClangIRSelectedDeclsFile.empty())
    return false;
  const auto *fd = d ? d->getAsFunction() : nullptr;
  if (fd && (fd->getDeclContext()->isDependentContext() ||
             fd->getTemplatedKind() != FunctionDecl::TK_NonTemplate))
    return false;
  return !fd || !cgm->shouldParseSelectedDeclBody(fd);
}
