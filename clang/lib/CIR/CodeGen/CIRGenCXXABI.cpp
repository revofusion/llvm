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

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/GlobalDecl.h"
#include "clang/Index/USRGeneration.h"
#include "llvm/ADT/SmallString.h"

using namespace clang;
using namespace clang::CIRGen;

CIRGenCXXABI::~CIRGenCXXABI() {}

cir::IfOp CIRGenCXXABI::emitCtorCompleteObjectHandler(CIRGenFunction &cgf,
                                                      const CXXRecordDecl *rd) {
  // Reached only when !hasConstructorVariants(), i.e. only by ABIs (like MS)
  // that lack separate complete/base constructor functions and so need a
  // runtime check here; Itanium-style ABIs have distinct Ctor_Complete/
  // Ctor_Base functions instead and never call this.
  if (cgm.getTarget().getCXXABI().hasConstructorVariants())
    llvm_unreachable("shouldn't be called in this ABI");
  cgm.errorNYI(rd->getSourceRange(),
               "emitCtorCompleteObjectHandler: complete object detection");
  return {};
}

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
  return cgm.getCIRLinkageForDeclarator(dtor, linkage,
                                        /*isConstantVariable=*/false);
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

bool CIRGenCXXABI::isZeroInitializable(const MemberPointerType *) {
  return true;
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
  cgf.emitReturnOfRValue(*cgf.currSrcLoc, rv, resultType);
}

// plans/044-cfrontend-7346-operand-virtual-dispatch-verification.md round 19
// (CVE-2026-7346 identity-bridge slice): USR helpers for
// `buildCIRGenVirtualMethodIdentityAttrs` below. Deliberately independent of
// this decl's own mangled name (`CIRGenModule::getMangledName`): USR
// generation and Itanium mangling are two separate clang subsystems, and a
// downstream reader (an exporter/analysis walking the CIR module) needs the
// SAME USR identity space an ordinary-AST-based class-hierarchy/override
// census would independently compute for the identical decl.
namespace {

std::string usrForDecl(const NamedDecl *decl) {
  if (!decl)
    return {};
  llvm::SmallString<256> usr;
  if (clang::index::generateUSRForDecl(decl, usr))
    return {};
  return std::string(usr.str());
}

// Walk to the root declaration of a virtual method: the first declaration
// anywhere in the hierarchy, i.e. the one with no overridden methods of its
// own. Verifies, at every step, that there is exactly one overridden method
// to follow -- a genuine multiple/virtual-inheritance step exposing more
// than one is ambiguous and this fails closed (returns `nullptr`) rather
// than picking `*overridden_methods().begin()` and guessing. Mirrors the
// identical, independently-reviewed discipline in this exporter's own
// `class_hierarchy_census.cpp` (`root_overridden_method`), so the two
// producers of the "root method" concept in this same identity space agree
// structurally, not just by convention.
const CXXMethodDecl *rootOverriddenMethodOrNull(const CXXMethodDecl *method) {
  while (method->size_overridden_methods() > 0) {
    if (method->size_overridden_methods() > 1)
      return nullptr;
    method = *method->overridden_methods().begin();
  }
  return method;
}

} // namespace

namespace clang::CIRGen {

CIRGenVirtualMethodIdentityAttrs
buildCIRGenVirtualMethodIdentityAttrs(mlir::MLIRContext &mlirContext,
                                      llvm::StringRef mangledName,
                                      const CXXMethodDecl *methodDecl) {
  CIRGenVirtualMethodIdentityAttrs attrs;
  attrs.method = mlir::FlatSymbolRefAttr::get(&mlirContext, mangledName);
  if (!methodDecl)
    return attrs;
  std::string methodUSR = usrForDecl(methodDecl);
  if (methodUSR.empty())
    return attrs;
  attrs.methodUSR = mlir::StringAttr::get(&mlirContext, methodUSR);
  const CXXMethodDecl *root = rootOverriddenMethodOrNull(methodDecl);
  if (!root)
    return attrs; // ambiguous override chain -- fail closed, method_usr only
  std::string rootUSR = usrForDecl(root);
  std::string declaringClassUSR = usrForDecl(root->getParent());
  if (rootUSR.empty() || declaringClassUSR.empty())
    return attrs;
  attrs.rootMethodUSR = mlir::StringAttr::get(&mlirContext, rootUSR);
  attrs.declaringClassUSR =
      mlir::StringAttr::get(&mlirContext, declaringClassUSR);
  return attrs;
}

} // namespace clang::CIRGen
