//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This provides an abstract class for C++ code generation. Concrete subclasses
// of this implement code generation for specific C++ ABIs.
//
//===----------------------------------------------------------------------===//

#include "CIRGenCXXABI.h"
#include "CIRGenFunction.h"
#include "CIRGenTypes.h"

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/GlobalDecl.h"
#include "clang/UnifiedSymbolResolution/USRGeneration.h"
#include "llvm/ADT/SmallString.h"
#include <algorithm>
#include <unordered_set>
#include <vector>

using namespace clang;
using namespace clang::CIRGen;

namespace {

std::string usrForDecl(const NamedDecl *decl) {
  if (!decl)
    return {};
  llvm::SmallString<256> usr;
  if (clang::index::generateUSRForDecl(decl, usr))
    return {};
  return std::string(usr.str());
}

bool collectRootOverrideEvidence(
    const CXXMethodDecl *method,
    std::unordered_set<const CXXMethodDecl *> &active,
    std::vector<const CXXMethodDecl *> &roots) {
  const CXXMethodDecl *canonical = method->getCanonicalDecl();
  if (!active.insert(canonical).second)
    return false;
  bool hasOverrideEdge = false;
  for (const FunctionDecl *functionRedecl : method->redecls()) {
    const auto *redecl = dyn_cast<CXXMethodDecl>(functionRedecl);
    if (!redecl)
      continue;
    for (const CXXMethodDecl *overridden : redecl->overridden_methods()) {
      hasOverrideEdge = true;
      if (!collectRootOverrideEvidence(overridden, active, roots)) {
        active.erase(canonical);
        return false;
      }
    }
  }
  active.erase(canonical);
  if (!hasOverrideEdge)
    roots.push_back(canonical);
  return true;
}

} // namespace

CIRGenVirtualMethodIdentityAttrs clang::CIRGen::
    buildCIRGenVirtualMethodIdentityAttrs(CIRGenModule &cgm,
                                          mlir::MLIRContext &mlirContext,
                                          GlobalDecl dispatchDecl) {
  CIRGenVirtualMethodIdentityAttrs attrs;
  const auto *methodDecl =
      cast<CXXMethodDecl>(dispatchDecl.getDecl());
  attrs.method = mlir::FlatSymbolRefAttr::get(
      &mlirContext, cgm.getMangledName(dispatchDecl));
  std::string methodUSR = usrForDecl(methodDecl);
  if (methodUSR.empty())
    return attrs;
  attrs.methodUSR = mlir::StringAttr::get(&mlirContext, methodUSR);
  if (isa<CXXDestructorDecl>(methodDecl)) {
    llvm::StringRef variant;
    switch (dispatchDecl.getDtorType()) {
    case Dtor_Deleting:
      variant = "deleting";
      break;
    case Dtor_Complete:
      variant = "complete";
      break;
    case Dtor_Base:
      variant = "base";
      break;
    case Dtor_Comdat:
      variant = "comdat";
      break;
    case Dtor_Unified:
      variant = "unified";
      break;
    case Dtor_VectorDeleting:
      variant = "vector_deleting";
      break;
    }
    if (!variant.empty())
      attrs.methodABIVariant =
          mlir::StringAttr::get(&mlirContext, variant);
  }
  if (!methodDecl->isVirtual())
    return attrs;

  // Preserve the complete exact root set. One final overrider can own several
  // unrelated vtable slots, so choosing one root would lose callable identity.
  std::unordered_set<const CXXMethodDecl *> active;
  std::vector<const CXXMethodDecl *> roots;
  if (!collectRootOverrideEvidence(methodDecl, active, roots))
    return attrs;
  std::vector<std::pair<std::string, std::string>> rootIdentities;
  for (const CXXMethodDecl *root : roots) {
    std::string rootUSR = usrForDecl(root);
    std::string declaringClassUSR;
    if (auto identity = recordDeclIdentity(cgm, root->getParent()))
      declaringClassUSR = std::move(*identity);
    if (rootUSR.empty() || declaringClassUSR.empty())
      return attrs;
    rootIdentities.emplace_back(std::move(rootUSR),
                                std::move(declaringClassUSR));
  }
  std::sort(rootIdentities.begin(), rootIdentities.end());
  rootIdentities.erase(
      std::unique(rootIdentities.begin(), rootIdentities.end()),
      rootIdentities.end());
  if (rootIdentities.empty())
    return attrs;
  llvm::SmallVector<mlir::Attribute> alternatives;
  for (const auto &[rootUSR, declaringClassUSR] : rootIdentities) {
    mlir::NamedAttrList alternative;
    alternative.set("method_usr",
                    mlir::StringAttr::get(&mlirContext, rootUSR));
    alternative.set("declaring_class_usr",
                    mlir::StringAttr::get(&mlirContext, declaringClassUSR));
    alternatives.push_back(alternative.getDictionary(&mlirContext));
  }
  attrs.rootAlternatives = mlir::ArrayAttr::get(&mlirContext, alternatives);
  if (rootIdentities.size() == 1) {
    attrs.rootMethodUSR =
        mlir::StringAttr::get(&mlirContext, rootIdentities.front().first);
    attrs.declaringClassUSR =
        mlir::StringAttr::get(&mlirContext, rootIdentities.front().second);
  }
  return attrs;
}

CIRGenCXXABI::~CIRGenCXXABI() {}

CIRGenCXXABI::AddedStructorArgCounts CIRGenCXXABI::addImplicitConstructorArgs(
    CIRGenFunction &cgf, const CXXConstructorDecl *d, CXXCtorType type,
    bool forVirtualBase, bool delegating, CallArgList &args) {
  AddedStructorArgs addedArgs =
      getImplicitConstructorArgs(cgf, d, type, forVirtualBase, delegating);
  for (auto [idx, prefixArg] : llvm::enumerate(addedArgs.prefix))
    args.insert(args.begin() + 1 + idx,
                CallArg(RValue::get(prefixArg.value), prefixArg.type));
  for (const auto &arg : addedArgs.suffix)
    args.add(RValue::get(arg.value), arg.type);
  return AddedStructorArgCounts(addedArgs.prefix.size(),
                                addedArgs.suffix.size());
}

CatchTypeInfo CIRGenCXXABI::getCatchAllTypeInfo() {
  return CatchTypeInfo{{}, 0};
}

void CIRGenCXXABI::buildThisParam(CIRGenFunction &cgf,
                                  FunctionArgList &params) {
  const auto *md = cast<CXXMethodDecl>(cgf.curGD.getDecl());

  // FIXME: I'm not entirely sure I like using a fake decl just for code
  // generation. Maybe we can come up with a better way?
  auto *thisDecl =
      ImplicitParamDecl::Create(cgm.getASTContext(), nullptr, md->getLocation(),
                                &cgm.getASTContext().Idents.get("this"),
                                md->getThisType(), ImplicitParamKind::CXXThis);
  params.push_back(thisDecl);
  cgf.cxxabiThisDecl = thisDecl;

  // Classic codegen computes the alignment of thisDecl and saves it in
  // CodeGenFunction::CXXABIThisAlignment, but it is only used in emitTypeCheck
  // in CodeGenFunction::StartFunction().
  assert(!cir::MissingFeatures::cxxabiThisAlignment());
}

cir::GlobalLinkageKind CIRGenCXXABI::getCXXDestructorLinkage(
    GVALinkage linkage, const CXXDestructorDecl *dtor, CXXDtorType dt) const {
  // Delegate back to cgm by default.
  return cgm.getCIRLinkageForDeclarator(dtor, linkage);
}

mlir::Value CIRGenCXXABI::loadIncomingCXXThis(CIRGenFunction &cgf) {
  ImplicitParamDecl *vd = getThisDecl(cgf);
  Address addr = cgf.getAddrOfLocalVar(vd);
  return cir::LoadOp::create(cgf.getBuilder(), cgf.getLoc(vd->getLocation()),
                             addr.getElementType(), addr.getPointer());
}

void CIRGenCXXABI::setCXXABIThisValue(CIRGenFunction &cgf,
                                      mlir::Value thisPtr) {
  /// Initialize the 'this' slot.
  assert(getThisDecl(cgf) && "no 'this' variable for function");
  cgf.cxxabiThisValue = thisPtr;
}

CharUnits CIRGenCXXABI::getArrayCookieSize(const CXXNewExpr *e) {
  if (!requiresArrayCookie(e))
    return CharUnits::Zero();

  return getArrayCookieSizeImpl(e->getAllocatedType());
}

bool CIRGenCXXABI::requiresArrayCookie(const CXXNewExpr *e) {
  // If the class's usual deallocation function takes two arguments,
  // it needs a cookie.
  if (e->doesUsualArrayDeleteWantSize())
    return true;

  return e->getAllocatedType().isDestructedType();
}

void CIRGenCXXABI::emitReturnFromThunk(CIRGenFunction &cgf, RValue rv,
                                       QualType resultType) {
  assert(!cgf.hasAggregateEvaluationKind(resultType) &&
         "cannot handle aggregates");
  mlir::Location loc = cgf.getBuilder().getUnknownLoc();
  cgf.emitReturnOfRValue(loc, rv, resultType);
}
