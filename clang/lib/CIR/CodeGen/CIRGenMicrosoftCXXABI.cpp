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
#include "clang/AST/Mangle.h"
#include "clang/AST/VTableBuilder.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/Support/raw_ostream.h"

using namespace clang;
using namespace clang::CIRGen;

namespace {

class CIRGenMicrosoftCXXABI : public CIRGenCXXABI {
  using VFTableIdTy = std::pair<const CXXRecordDecl *, CharUnits>;
  llvm::DenseMap<VFTableIdTy, cir::GlobalOp> vtables;

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
  AddedStructorArgCounts added;
  if (isa<CXXDestructorDecl>(gd.getDecl()) &&
      (gd.getDtorType() == Dtor_Deleting ||
       gd.getDtorType() == Dtor_VectorDeleting)) {
    argTys.push_back(cgm.getASTContext().IntTy);
    ++added.suffix;
  }
  return added;
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
  const auto *md = cast<CXXMethodDecl>(cgf.curGD.getDecl());
  if (!isa<CXXDestructorDecl>(md) ||
      (cgf.curGD.getDtorType() != Dtor_Deleting &&
       cgf.curGD.getDtorType() != Dtor_VectorDeleting))
    return;

  ASTContext &ctx = cgm.getASTContext();
  auto *shouldDelete = ImplicitParamDecl::Create(
      ctx, nullptr, cgf.curGD.getDecl()->getLocation(),
      &ctx.Idents.get("should_call_delete"), ctx.IntTy,
      ImplicitParamKind::Other);
  params.push_back(shouldDelete);
  getStructorImplicitParamDecl(cgf) = shouldDelete;
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
    if (gd.getCtorType() != Ctor_Complete)
      gd = GlobalDecl(cd, Ctor_Complete);
    if (cd->getParent()->getNumVBases() != 0) {
      cgm.errorNYI(cd->getSourceRange(),
                   "Microsoft C++ ABI constructor with virtual bases");
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
  ASTContext &ctx = cgm.getASTContext();
  CIRGenBuilderTy &builder = cgf.getBuilder();

  CharUnits vbptrOffset =
      ctx.getASTRecordLayout(classDecl).getVBPtrOffset();
  CharUnits intSize = ctx.getTypeSizeInChars(ctx.IntTy);
  CharUnits vbtableOffset =
      intSize *
      cgm.getMicrosoftVTableContext().getVBTableIndex(classDecl,
                                                      baseClassDecl);

  mlir::Value thisBytePtr =
      builder.createBitcast(thisAddr.getPointer(), cgm.uInt8PtrTy);
  mlir::Value vbptrOffsetValue =
      builder.getConstInt(loc, cgm.ptrDiffTy, vbptrOffset.getQuantity());
  mlir::Value vbptrBytePtr = cir::PtrStrideOp::create(
      builder, loc, cgm.uInt8PtrTy, thisBytePtr, vbptrOffsetValue);
  mlir::Value vbptrPtr =
      builder.createBitcast(vbptrBytePtr, builder.getPointerTo(cgm.uInt8PtrTy));
  mlir::Value vbtable = builder.createAlignedLoad(
      loc, cgm.uInt8PtrTy, vbptrPtr,
      thisAddr.getAlignment().alignmentAtOffset(vbptrOffset));

  mlir::Value vbtableBytePtr = builder.createBitcast(vbtable, cgm.uInt8PtrTy);
  mlir::Value vbtableOffsetValue =
      builder.getConstInt(loc, cgm.ptrDiffTy, vbtableOffset.getQuantity());
  mlir::Value entryBytePtr = cir::PtrStrideOp::create(
      builder, loc, cgm.uInt8PtrTy, vbtableBytePtr, vbtableOffsetValue);
  mlir::Value entryPtr = builder.createBitcast(
      entryBytePtr, builder.getPointerTo(builder.getSInt32Ty()));
  mlir::Value vbaseOffset = builder.createAlignedLoad(
      loc, builder.getSInt32Ty(), entryPtr, CharUnits::fromQuantity(4));
  vbaseOffset = builder.createIntCast(vbaseOffset, cgm.ptrDiffTy);

  return builder.createNSWAdd(loc, vbptrOffsetValue, vbaseOffset);
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
  SmallString<256> name;
  llvm::raw_svector_ostream out(name);
  getMangleContext().mangleCXXRTTI(ty, out);

  CharUnits align = cgm.getASTContext().toCharUnitsFromBits(
      cgm.getTarget().getPointerAlign(LangAS::Default));
  cir::GlobalOp global = cgm.createOrReplaceCXXRuntimeVariable(
      loc, name, cgm.uInt8Ty, cir::GlobalLinkageKind::ExternalLinkage, align);
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
  if (!cgm.getASTContext().getTargetInfo().callGlobalDeleteInDeletingDtor(
          cgm.getLangOpts())) {
    if (de->isGlobalDelete()) {
      cgm.errorNYI(de->getSourceRange(),
                   "Microsoft C++ ABI virtual object delete with separate "
                   "global delete");
      return;
    }
  }

  emitVirtualDestructorCall(cgf, dtor, Dtor_Deleting, ptr, de);
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
  MicrosoftVTableContext &vtContext = cgm.getMicrosoftVTableContext();
  const VPtrInfoVector &vfPtrs = vtContext.getVFPtrOffsets(rd);

  if (cgm.getLangOpts().RTTIData) {
    cgm.errorNYI(rd->getSourceRange(),
                 "Microsoft C++ ABI vtable RTTI data emission");
    return;
  }

  for (const std::unique_ptr<VPtrInfo> &info : vfPtrs) {
    cir::GlobalOp vtable = getAddrOfVTable(rd, info->FullOffsetInMDC);
    if (!vtable || vtable.hasInitializer())
      continue;

    const VTableLayout &vtLayout =
        vtContext.getVFTableLayout(rd, info->FullOffsetInMDC);

    for (const VTableComponent &component : vtLayout.vtable_components()) {
      if (component.isRTTIKind()) {
        cgm.errorNYI(rd->getSourceRange(),
                     "Microsoft C++ ABI vtable RTTI component");
        return;
      }
    }

    mlir::Attribute rtti =
        cgm.getBuilder().getConstNullPtrAttr(cgm.getBuilder().getUInt8PtrTy());
    cgvt.createVTableInitializer(vtable, vtLayout, rtti,
                                 cir::isLocalLinkage(vtable.getLinkage()));

    cir::GlobalLinkageKind linkage =
        rd->hasAttr<DLLImportAttr>()
            ? cir::GlobalLinkageKind::LinkOnceODRLinkage
            : cgm.getVTableLinkage(rd);
    vtable.setLinkage(linkage);
    if (cgm.supportsCOMDAT() && cir::isWeakForLinker(linkage))
      vtable.setComdat(true);
    cgm.setGVProperties(vtable, rd);
  }
}

mlir::Value CIRGenMicrosoftCXXABI::emitVirtualDestructorCall(
    CIRGenFunction &cgf, const CXXDestructorDecl *dtor, CXXDtorType dtorType,
    Address thisAddr, DeleteOrMemberCallExpr e) {
  auto *callExpr = dyn_cast<const CXXMemberCallExpr *>(e);
  auto *delExpr = dyn_cast<const CXXDeleteExpr *>(e);
  assert((callExpr != nullptr) ^ (delExpr != nullptr));
  assert(callExpr == nullptr || callExpr->arg_begin() == callExpr->arg_end());
  assert(dtorType == Dtor_VectorDeleting || dtorType == Dtor_Complete ||
         dtorType == Dtor_Deleting);

  ASTContext &ctx = cgm.getASTContext();
  bool vectorDeletingDtors =
      ctx.getTargetInfo().emitVectorDeletingDtors(ctx.getLangOpts());
  GlobalDecl gd(dtor,
                vectorDeletingDtors ? Dtor_VectorDeleting : Dtor_Deleting);
  const CIRGenFunctionInfo &fnInfo =
      cgm.getTypes().arrangeCXXStructorDeclaration(gd);
  cir::FuncType fnTy = cgm.getTypes().getFunctionType(fnInfo);
  CIRGenCallee callee =
      CIRGenCallee::forVirtual(callExpr, gd, thisAddr, fnTy);

  bool isDeleting = dtorType == Dtor_Deleting;
  bool isArrayDelete =
      delExpr && delExpr->isArrayForm() && vectorDeletingDtors;
  if (isArrayDelete) {
    cgm.errorNYI(delExpr->getSourceRange(),
                 "Microsoft C++ ABI vector deleting destructor call");
    return {};
  }

  bool isGlobalDelete =
      delExpr && delExpr->isGlobalDelete() &&
      ctx.getTargetInfo().callGlobalDeleteInDeletingDtor(ctx.getLangOpts());
  unsigned flags = (isDeleting ? 1 : 0) | (isGlobalDelete ? 4 : 0);
  mlir::Location loc =
      cgf.getLoc(callExpr ? callExpr->getExprLoc() : delExpr->getExprLoc());
  mlir::Value implicitParam = cgf.getBuilder().getSInt32(flags, loc);

  QualType thisTy = callExpr ? callExpr->getObjectType()
                             : delExpr->getDestroyedType();
  while (const ArrayType *arrayTy = ctx.getAsArrayType(thisTy))
    thisTy = arrayTy->getElementType();

  return cgf.emitCXXDestructorCall(gd, callee, thisAddr.getPointer(), thisTy,
                                   implicitParam, ctx.IntTy, callExpr)
      .getValue();
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
  VFTableIdTy id(rd, vptrOffset);
  auto cached = vtables.find(id);
  if (cached != vtables.end())
    return cached->second;

  MicrosoftVTableContext &vtContext = cgm.getMicrosoftVTableContext();
  const VPtrInfoVector &vfPtrs = vtContext.getVFPtrOffsets(rd);
  const std::unique_ptr<VPtrInfo> *vfPtr =
      llvm::find_if(vfPtrs, [&](const std::unique_ptr<VPtrInfo> &info) {
        return info->FullOffsetInMDC == vptrOffset;
      });
  if (vfPtr == vfPtrs.end()) {
    vtables[id] = {};
    return {};
  }

  if (cgm.getLangOpts().RTTIData) {
    cgm.errorNYI(rd->getSourceRange(),
                 "Microsoft C++ ABI vtable RTTI data reference");
    vtables[id] = {};
    return {};
  }

  SmallString<256> name;
  llvm::raw_svector_ostream out(name);
  cast<MicrosoftMangleContext>(getMangleContext())
      .mangleCXXVFTable(rd, (*vfPtr)->MangledPath, out);

  const VTableLayout &vtLayout =
      vtContext.getVFTableLayout(rd, (*vfPtr)->FullOffsetInMDC);
  cir::RecordType vtableType = cgm.getVTables().getVTableType(vtLayout);
  cir::GlobalLinkageKind linkage =
      rd->hasAttr<DLLImportAttr>()
          ? cir::GlobalLinkageKind::LinkOnceODRLinkage
          : cgm.getVTableLinkage(rd);
  llvm::Align align = cgm.getDataLayout().getABITypeAlign(vtableType);
  cir::GlobalOp vtable = cgm.createOrReplaceCXXRuntimeVariable(
      cgm.getLoc(rd->getSourceRange()), name, vtableType, linkage,
      CharUnits::fromQuantity(align));

  vtables[id] = vtable;
  return vtable;
}

CIRGenCallee CIRGenMicrosoftCXXABI::getVirtualFunctionPointer(
    CIRGenFunction &cgf, GlobalDecl gd, Address thisAddr, mlir::Type ty,
    SourceLocation loc) {
  CIRGenBuilderTy &builder = cgm.getBuilder();
  mlir::Location mlirLoc = cgf.getLoc(loc);
  auto *methodDecl = cast<CXXMethodDecl>(gd.getDecl());

  MicrosoftVTableContext &vtContext = cgm.getMicrosoftVTableContext();
  MethodVFTableLocation vfTableLoc = vtContext.getMethodVFTableLocation(gd);
  if (vfTableLoc.VBase || vfTableLoc.VBTableIndex != 0) {
    cgm.errorNYI(mlirLoc,
                 "Microsoft C++ ABI virtual function pointer with virtual base");
    return CIRGenCallee::forDirect(cgm.getAddrOfFunction(gd, ty), gd);
  }

  Address vptrThis = thisAddr;
  if (!vfTableLoc.VFPtrOffset.isZero()) {
    mlir::Value byteThis =
        builder.createBitcast(thisAddr.getPointer(), cgm.uInt8PtrTy);
    mlir::Value offset =
        builder.getSInt64(vfTableLoc.VFPtrOffset.getQuantity(), mlirLoc);
    mlir::Value adjusted = cir::PtrStrideOp::create(
        builder, mlirLoc, cgm.uInt8PtrTy, byteThis, offset);
    vptrThis = Address(
        adjusted, cgm.uInt8Ty,
        thisAddr.getAlignment().alignmentAtOffset(vfTableLoc.VFPtrOffset));
  }

  mlir::Value vtable = cgf.getVTablePtr(mlirLoc, vptrThis,
                                        methodDecl->getParent());
  cir::PointerType fnPtrTy = builder.getPointerTo(ty);
  auto vtableSlotPtr = cir::VTableGetVirtualFnAddrOp::create(
      builder, mlirLoc, builder.getPointerTo(fnPtrTy), vtable,
      vfTableLoc.Index);
  mlir::Value vfunc =
      builder.createAlignedLoad(mlirLoc, fnPtrTy, vtableSlotPtr,
                                cgf.getPointerAlign());
  return CIRGenCallee(gd, vfunc.getDefiningOp());
}

mlir::Value
CIRGenMicrosoftCXXABI::getVTableAddressPoint(BaseSubobject base,
                                             const CXXRecordDecl *vtableClass) {
  mlir::Location loc = cgm.getLoc(vtableClass->getSourceRange());
  cir::GlobalOp vtable = getAddrOfVTable(vtableClass, base.getBaseOffset());
  if (!vtable) {
    cgm.errorNYI(loc, "Microsoft C++ ABI missing vfptr table");
    return cgm.getBuilder().getNullValue(
        cir::VPtrType::get(cgm.getBuilder().getContext()), loc);
  }

  const VTableLayout &vtLayout = cgm.getMicrosoftVTableContext()
                                     .getVFTableLayout(vtableClass,
                                                       base.getBaseOffset());
  if (vtLayout.getNumVTables() != 1 ||
      vtLayout.getAddressPointIndices().empty()) {
    cgm.errorNYI(loc, "Microsoft C++ ABI complex vfptr table layout");
    return cgm.getBuilder().getNullValue(
        cir::VPtrType::get(cgm.getBuilder().getContext()), loc);
  }

  auto vtablePtrTy = cir::VPtrType::get(cgm.getBuilder().getContext());
  return cir::VTableAddrPointOp::create(
      cgm.getBuilder(), loc, vtablePtrTy,
      mlir::FlatSymbolRefAttr::get(vtable.getSymNameAttr()),
      cir::AddressPointAttr::get(cgm.getBuilder().getContext(), 0,
                                 vtLayout.getAddressPointIndices().front()));
}

mlir::Value CIRGenMicrosoftCXXABI::getVTableAddressPointInStructor(
    CIRGenFunction &cgf, const CXXRecordDecl *vtableClass, BaseSubobject base,
    const CXXRecordDecl *nearestVBase) {
  if (nearestVBase) {
    mlir::Location loc = cgf.getLoc(vtableClass->getSourceRange());
    cgm.errorNYI(loc,
                 "Microsoft C++ ABI virtual-base structor vtable address point");
    return cgf.getBuilder().getNullValue(
        cir::VPtrType::get(cgf.getBuilder().getContext()), loc);
  }
  return getVTableAddressPoint(base, vtableClass);
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
