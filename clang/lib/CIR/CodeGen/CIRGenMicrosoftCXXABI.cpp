//===--- CIRGenMicrosoftCXXABI.cpp - Emit CIR for Microsoft C++ ABI -------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CIRGenCXXABI.h"
#include "CIRGenFunction.h"

#include "clang/AST/Attr.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/GlobalDecl.h"

using namespace clang;
using namespace clang::CIRGen;

namespace {

class CIRGenMicrosoftCXXABI : public CIRGenCXXABI {
public:
  CIRGenMicrosoftCXXABI(CIRGenModule &cgm) : CIRGenCXXABI(cgm) {}

  void emitCXXStructor(GlobalDecl gd) override;
  mlir::Value getVirtualBaseClassOffset(mlir::Location loc,
                                        CIRGenFunction &cgf, Address thisAddr,
                                        const CXXRecordDecl *classDecl,
                                        const CXXRecordDecl *baseClassDecl)
      override;
  mlir::Value emitDynamicCast(CIRGenFunction &cgf, mlir::Location loc,
                              QualType srcRecordTy, QualType destRecordTy,
                              cir::PointerType destCIRTy, bool isRefCast,
                              Address src) override;
  AddedStructorArgCounts
  buildStructorSignature(GlobalDecl gd,
                         llvm::SmallVectorImpl<CanQualType> &argTys) override;
  AddedStructorArgs getImplicitConstructorArgs(CIRGenFunction &cgf,
                                               const CXXConstructorDecl *d,
                                               CXXCtorType type,
                                               bool forVirtualBase,
                                               bool delegating) override;
  void emitInstanceFunctionProlog(SourceLocation loc,
                                  CIRGenFunction &cgf) override;
  void emitRethrow(CIRGenFunction &cgf, bool isNoReturn) override;
  void emitThrow(CIRGenFunction &cgf, const CXXThrowExpr *e) override;
  void emitBadCastCall(CIRGenFunction &cgf, mlir::Location loc) override;
  void emitBeginCatch(CIRGenFunction &cgf,
                      const CXXCatchStmt *catchStmt) override;
  mlir::Attribute getAddrOfRTTIDescriptor(mlir::Location loc,
                                          QualType ty) override;
  CatchTypeInfo
  getAddrOfCXXCatchHandlerType(mlir::Location loc, QualType ty,
                               QualType catchHandlerType) override;
  mlir::Value getCXXDestructorImplicitParam(CIRGenFunction &cgf,
                                            const CXXDestructorDecl *dd,
                                            CXXDtorType type,
                                            bool forVirtualBase,
                                            bool delegating) override;
  void emitCXXConstructors(const CXXConstructorDecl *d) override;
  void emitCXXDestructors(const CXXDestructorDecl *d) override;
  void emitDestructorCall(CIRGenFunction &cgf, const CXXDestructorDecl *dd,
                          CXXDtorType type, bool forVirtualBase,
                          bool delegating, Address thisAddr,
                          QualType thisTy) override;
  void registerGlobalDtor(const VarDecl *vd, cir::FuncOp dtor,
                          mlir::Value addr) override;
  void emitGuardedInit(CIRGenFunction &cgf, const VarDecl &d,
                       cir::GlobalOp var, bool shouldPerformInit) override;
  void emitVirtualObjectDelete(CIRGenFunction &cgf, const CXXDeleteExpr *de,
                               Address ptr, QualType elementType,
                               const CXXDestructorDecl *dtor) override;
  size_t getSrcArgforCopyCtor(const CXXConstructorDecl *cd,
                              FunctionArgList &args) const override;
  bool isVirtualOffsetNeededForVTableField(CIRGenFunction &cgf,
                                           CIRGenFunction::VPtr vptr) override;
  void emitVTableDefinitions(CIRGenVTables &cgvt,
                             const CXXRecordDecl *rd) override;
  mlir::Value emitVirtualDestructorCall(CIRGenFunction &cgf,
                                        const CXXDestructorDecl *dtor,
                                        CXXDtorType dtorType, Address thisAddr,
                                        DeleteOrMemberCallExpr e) override;
  void emitVirtualInheritanceTables(const CXXRecordDecl *rd) override;
  bool useThunkForDtorVariant(const CXXDestructorDecl *dtor,
                              CXXDtorType dt) const override;
  cir::GlobalOp getAddrOfVTable(const CXXRecordDecl *rd,
                                CharUnits vptrOffset) override;
  CIRGenCallee getVirtualFunctionPointer(CIRGenFunction &cgf, GlobalDecl gd,
                                         Address thisAddr, mlir::Type ty,
                                         SourceLocation loc) override;
  mlir::Value getVTableAddressPoint(BaseSubobject base,
                                    const CXXRecordDecl *vtableClass) override;
  mlir::Value getVTableAddressPointInStructor(
      CIRGenFunction &cgf, const CXXRecordDecl *vtableClass, BaseSubobject base,
      const CXXRecordDecl *nearestVBase) override;
  void addImplicitStructorParams(CIRGenFunction &cgf, QualType &resTy,
                                 FunctionArgList &params) override;
  bool doStructorsInitializeVPtrs(const CXXRecordDecl *vtableClass) override;
  bool hasThisReturn(GlobalDecl gd) const override;
  bool hasMostDerivedReturn(GlobalDecl gd) const override;
  mlir::Value performThisAdjustment(CIRGenFunction &cgf, Address thisAddr,
                                    const CXXRecordDecl *unadjustedClass,
                                    const ThunkInfo &ti) override;
  mlir::Value performReturnAdjustment(CIRGenFunction &cgf, Address ret,
                                      const CXXRecordDecl *unadjustedClass,
                                      const ReturnAdjustment &ra) override;
  Address initializeArrayCookie(CIRGenFunction &cgf, Address newPtr,
                                mlir::Value numElements, const CXXNewExpr *e,
                                QualType elementType) override;

protected:
  CharUnits getArrayCookieSizeImpl(QualType elementType) override;
};

}

void CIRGenMicrosoftCXXABI::emitInstanceFunctionProlog(SourceLocation loc,
                                                       CIRGenFunction &cgf) {
  if (cgf.curFuncDecl && cgf.curFuncDecl->hasAttr<NakedAttr>())
    cgf.cgm.errorNYI(cgf.curFuncDecl->getLocation(),
                     "Microsoft C++ ABI naked instance function prolog");

  setCXXABIThisValue(cgf, loadIncomingCXXThis(cgf));

  if (hasThisReturn(cgf.curGD) || hasMostDerivedReturn(cgf.curGD)) {
    mlir::Value thisValue = cgf.cxxabiThisValue;
    if (thisValue.getType() != cgf.returnValue.getElementType())
      thisValue = cgf.getBuilder().createBitcast(
          cgf.getLoc(loc), thisValue, cgf.returnValue.getElementType());
    cgf.getBuilder().createStore(cgf.getLoc(loc), thisValue, cgf.returnValue);
  }
}

CIRGenCXXABI::AddedStructorArgCounts
CIRGenMicrosoftCXXABI::buildStructorSignature(
    GlobalDecl gd, llvm::SmallVectorImpl<CanQualType> &argTys) {
  return AddedStructorArgCounts{};
}

CIRGenCXXABI::AddedStructorArgs
CIRGenMicrosoftCXXABI::getImplicitConstructorArgs(CIRGenFunction &cgf,
                                                  const CXXConstructorDecl *d,
                                                  CXXCtorType type,
                                                  bool forVirtualBase,
                                                  bool delegating) {
  return AddedStructorArgs{};
}

void CIRGenMicrosoftCXXABI::addImplicitStructorParams(CIRGenFunction &cgf,
                                                      QualType &resTy,
                                                      FunctionArgList &params) {
}

bool CIRGenMicrosoftCXXABI::hasThisReturn(GlobalDecl gd) const {
  return isa<CXXConstructorDecl>(gd.getDecl());
}

bool CIRGenMicrosoftCXXABI::hasMostDerivedReturn(GlobalDecl gd) const {
  return isa<CXXDestructorDecl>(gd.getDecl()) &&
         (gd.getDtorType() == Dtor_Deleting ||
          gd.getDtorType() == Dtor_VectorDeleting);
}

void CIRGenMicrosoftCXXABI::emitCXXStructor(GlobalDecl gd) {
  const auto *md = cast<CXXMethodDecl>(gd.getDecl());
  if (const auto *cd = dyn_cast<CXXConstructorDecl>(md)) {
    if (cd->getParent()->getNumVBases() != 0) {
      cgm.errorNYI(cd->getSourceRange(),
                   "Microsoft C++ ABI constructor with virtual bases");
      return;
    }
    if (cd->getParent()->isDynamicClass()) {
      cgm.errorNYI(cd->getSourceRange(),
                   "Microsoft C++ ABI dynamic class constructor");
      return;
    }
    cir::FuncOp fn = cgm.codegenCXXStructor(gd);
    cgm.maybeSetTrivialComdat(*cd, fn);
    return;
  }
  if (const auto *dd = dyn_cast<CXXDestructorDecl>(md)) {
    if (dd->getParent()->getNumVBases() != 0) {
      cgm.errorNYI(dd->getSourceRange(),
                   "Microsoft C++ ABI destructor with virtual bases");
      return;
    }
    if (gd.getDtorType() != Dtor_Base) {
      cgm.errorNYI(dd->getSourceRange(),
                   "Microsoft C++ ABI non-base destructor variant");
      return;
    }
    cir::FuncOp fn = cgm.codegenCXXStructor(gd);
    cgm.maybeSetTrivialComdat(*dd, fn);
    return;
  }
  cgm.errorNYI(md->getSourceRange(), "Microsoft C++ ABI structor emission");
}

void CIRGenMicrosoftCXXABI::emitCXXConstructors(const CXXConstructorDecl *d) {
  cgm.emitGlobal(GlobalDecl(d, Ctor_Complete));
}

void CIRGenMicrosoftCXXABI::emitCXXDestructors(const CXXDestructorDecl *d) {
  cgm.emitGlobal(GlobalDecl(d, Dtor_Base));
}

mlir::Value CIRGenMicrosoftCXXABI::getVirtualBaseClassOffset(
    mlir::Location loc, CIRGenFunction &cgf, Address thisAddr,
    const CXXRecordDecl *classDecl, const CXXRecordDecl *baseClassDecl) {
  cgm.errorNYI(loc, "Microsoft C++ ABI virtual base offset");
  return cgf.getBuilder().getConstant(loc, cir::PoisonAttr::get(cgm.ptrDiffTy));
}

mlir::Value CIRGenMicrosoftCXXABI::emitDynamicCast(
    CIRGenFunction &cgf, mlir::Location loc, QualType srcRecordTy,
    QualType destRecordTy, cir::PointerType destCIRTy, bool isRefCast,
    Address src) {
  cgm.errorNYI(loc, "Microsoft C++ ABI dynamic_cast lowering");
  return cgf.getBuilder().getNullPtr(destCIRTy, loc);
}

void CIRGenMicrosoftCXXABI::emitRethrow(CIRGenFunction &cgf, bool isNoReturn) {
  cgm.errorNYI(*cgf.currSrcLoc, "Microsoft C++ ABI rethrow lowering");
}

void CIRGenMicrosoftCXXABI::emitThrow(CIRGenFunction &cgf,
                                      const CXXThrowExpr *e) {
  cgm.errorNYI(e->getSourceRange(), "Microsoft C++ ABI throw lowering");
}

void CIRGenMicrosoftCXXABI::emitBadCastCall(CIRGenFunction &cgf,
                                            mlir::Location loc) {
  cgm.errorNYI(loc, "Microsoft C++ ABI bad_cast lowering");
}

void CIRGenMicrosoftCXXABI::emitBeginCatch(CIRGenFunction &cgf,
                                           const CXXCatchStmt *catchStmt) {
  cgm.errorNYI(catchStmt->getSourceRange(),
               "Microsoft C++ ABI catch lowering");
}

mlir::Attribute
CIRGenMicrosoftCXXABI::getAddrOfRTTIDescriptor(mlir::Location loc,
                                               QualType ty) {
  cgm.errorNYI(loc, "Microsoft C++ ABI RTTI descriptor emission");
  CharUnits align = cgm.getASTContext().toCharUnitsFromBits(
      cgm.getTarget().getPointerAlign(LangAS::Default));
  cir::GlobalOp global = cgm.createOrReplaceCXXRuntimeVariable(
      loc, "__CIR_NYI_MSVC_RTTI", cgm.uInt8Ty,
      cir::GlobalLinkageKind::ExternalLinkage, align);
  return cgm.getBuilder().getGlobalViewAttr(cgm.uInt8PtrTy, global);
}

CatchTypeInfo CIRGenMicrosoftCXXABI::getAddrOfCXXCatchHandlerType(
    mlir::Location loc, QualType ty, QualType catchHandlerType) {
  auto rtti = mlir::dyn_cast<cir::GlobalViewAttr>(
      getAddrOfRTTIDescriptor(loc, ty));
  return CatchTypeInfo{rtti, 0};
}

mlir::Value CIRGenMicrosoftCXXABI::getCXXDestructorImplicitParam(
    CIRGenFunction &cgf, const CXXDestructorDecl *dd, CXXDtorType type,
    bool forVirtualBase, bool delegating) {
  return {};
}

void CIRGenMicrosoftCXXABI::emitDestructorCall(
    CIRGenFunction &cgf, const CXXDestructorDecl *dd, CXXDtorType type,
    bool forVirtualBase, bool delegating, Address thisAddr, QualType thisTy) {
  if (forVirtualBase || dd->getParent()->getNumVBases() != 0) {
    cgm.errorNYI(dd->getSourceRange(),
                 "Microsoft C++ ABI virtual-base destructor call");
    return;
  }
  if (type == Dtor_Deleting || type == Dtor_VectorDeleting) {
    cgm.errorNYI(dd->getSourceRange(),
                 "Microsoft C++ ABI deleting destructor call");
    return;
  }

  GlobalDecl gd(dd, Dtor_Base);
  CIRGenCallee callee = CIRGenCallee::forDirect(cgm.getAddrOfCXXStructor(gd),
                                                gd);
  cgf.emitCXXDestructorCall(gd, callee, thisAddr.getPointer(), thisTy,
                            nullptr, QualType(), nullptr);
}

void CIRGenMicrosoftCXXABI::registerGlobalDtor(const VarDecl *vd,
                                               cir::FuncOp dtor,
                                               mlir::Value addr) {
  if (vd->isNoDestroy(cgm.getASTContext()))
    return;
  cgm.errorNYI(vd->getSourceRange(),
               "Microsoft C++ ABI global destructor registration");
}

void CIRGenMicrosoftCXXABI::emitGuardedInit(CIRGenFunction &cgf,
                                            const VarDecl &d,
                                            cir::GlobalOp var,
                                            bool shouldPerformInit) {
  cgm.errorNYI(d.getSourceRange(),
               "Microsoft C++ ABI guarded local static initialization");
}

void CIRGenMicrosoftCXXABI::emitVirtualObjectDelete(
    CIRGenFunction &cgf, const CXXDeleteExpr *de, Address ptr,
    QualType elementType, const CXXDestructorDecl *dtor) {
  cgm.errorNYI(de->getSourceRange(),
               "Microsoft C++ ABI virtual object delete");
}

size_t CIRGenMicrosoftCXXABI::getSrcArgforCopyCtor(
    const CXXConstructorDecl *cd, FunctionArgList &args) const {
  if (cd->getParent()->getNumVBases() > 0 &&
      cd->getType()->castAs<FunctionProtoType>()->isVariadic())
    return 2;
  return 1;
}

bool CIRGenMicrosoftCXXABI::isVirtualOffsetNeededForVTableField(
    CIRGenFunction &cgf, CIRGenFunction::VPtr vptr) {
  return false;
}

void CIRGenMicrosoftCXXABI::emitVTableDefinitions(CIRGenVTables &cgvt,
                                                  const CXXRecordDecl *rd) {
  cgm.errorNYI(rd->getSourceRange(),
               "Microsoft C++ ABI vtable definition emission");
}

mlir::Value CIRGenMicrosoftCXXABI::emitVirtualDestructorCall(
    CIRGenFunction &cgf, const CXXDestructorDecl *dtor, CXXDtorType dtorType,
    Address thisAddr, DeleteOrMemberCallExpr e) {
  cgm.errorNYI(dtor->getSourceRange(),
               "Microsoft C++ ABI virtual destructor call");
  return {};
}

void CIRGenMicrosoftCXXABI::emitVirtualInheritanceTables(
    const CXXRecordDecl *rd) {
  cgm.errorNYI(rd->getSourceRange(),
               "Microsoft C++ ABI virtual inheritance tables");
}

bool CIRGenMicrosoftCXXABI::useThunkForDtorVariant(
    const CXXDestructorDecl *dtor, CXXDtorType dt) const {
  return false;
}

cir::GlobalOp CIRGenMicrosoftCXXABI::getAddrOfVTable(const CXXRecordDecl *rd,
                                                     CharUnits vptrOffset) {
  cgm.errorNYI(rd->getSourceRange(), "Microsoft C++ ABI vtable reference");
  return {};
}

CIRGenCallee CIRGenMicrosoftCXXABI::getVirtualFunctionPointer(
    CIRGenFunction &cgf, GlobalDecl gd, Address thisAddr, mlir::Type ty,
    SourceLocation loc) {
  cgm.errorNYI(loc, "Microsoft C++ ABI virtual function pointer");
  return CIRGenCallee::forDirect(cgm.getAddrOfFunction(gd, ty), gd);
}

mlir::Value
CIRGenMicrosoftCXXABI::getVTableAddressPoint(BaseSubobject base,
                                             const CXXRecordDecl *vtableClass) {
  mlir::Location loc = cgm.getLoc(vtableClass->getSourceRange());
  cgm.errorNYI(loc, "Microsoft C++ ABI vtable address point");
  return cgm.getBuilder().getNullValue(
      cir::VPtrType::get(cgm.getBuilder().getContext()), loc);
}

mlir::Value CIRGenMicrosoftCXXABI::getVTableAddressPointInStructor(
    CIRGenFunction &cgf, const CXXRecordDecl *vtableClass, BaseSubobject base,
    const CXXRecordDecl *nearestVBase) {
  mlir::Location loc = cgf.getLoc(vtableClass->getSourceRange());
  cgm.errorNYI(loc, "Microsoft C++ ABI structor vtable address point");
  return cgf.getBuilder().getNullValue(
      cir::VPtrType::get(cgf.getBuilder().getContext()), loc);
}

bool CIRGenMicrosoftCXXABI::doStructorsInitializeVPtrs(
    const CXXRecordDecl *vtableClass) {
  return true;
}

mlir::Value CIRGenMicrosoftCXXABI::performThisAdjustment(
    CIRGenFunction &cgf, Address thisAddr, const CXXRecordDecl *unadjustedClass,
    const ThunkInfo &ti) {
  if (!ti.This.isEmpty())
    cgm.errorNYI(unadjustedClass->getSourceRange(),
                 "Microsoft C++ ABI this adjustment");
  return thisAddr.getPointer();
}

mlir::Value CIRGenMicrosoftCXXABI::performReturnAdjustment(
    CIRGenFunction &cgf, Address ret, const CXXRecordDecl *unadjustedClass,
    const ReturnAdjustment &ra) {
  if (!ra.isEmpty())
    cgm.errorNYI(unadjustedClass->getSourceRange(),
                 "Microsoft C++ ABI return adjustment");
  return ret.getPointer();
}

Address CIRGenMicrosoftCXXABI::initializeArrayCookie(
    CIRGenFunction &cgf, Address newPtr, mlir::Value numElements,
    const CXXNewExpr *e, QualType elementType) {
  cgm.errorNYI(e->getSourceRange(), "Microsoft C++ ABI array cookie");
  return newPtr;
}

CharUnits
CIRGenMicrosoftCXXABI::getArrayCookieSizeImpl(QualType elementType) {
  return cgm.getSizeSize();
}

CIRGenCXXABI *clang::CIRGen::CreateCIRGenMicrosoftCXXABI(CIRGenModule &cgm) {
  return new CIRGenMicrosoftCXXABI(cgm);
}
