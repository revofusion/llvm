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

// Conservative "might this guarded initializer throw" check, used to decide
// whether the (not yet implemented here) exception-safety wiring for guarded
// static initialization would be required. Mirrors the identically-named
// static helper in CIRGenItaniumCXXABI.cpp.
static bool functionMayThrow(const FunctionDecl *fd) {
  if (!fd)
    return true;

  const auto *fpt = fd->getType()->getAs<FunctionProtoType>();
  return !fpt || fpt->canThrow() != CT_Cannot;
}

static bool guardedInitMayThrow(const Stmt *s) {
  if (!s)
    return false;

  if (isa<CXXThrowExpr>(s))
    return true;

  if (const auto *e = dyn_cast<CallExpr>(s))
    if (functionMayThrow(e->getDirectCallee()))
      return true;

  if (const auto *e = dyn_cast<CXXConstructExpr>(s))
    if (functionMayThrow(e->getConstructor()))
      return true;

  if (const auto *e = dyn_cast<CXXNewExpr>(s))
    if (functionMayThrow(e->getOperatorNew()))
      return true;

  if (const auto *e = dyn_cast<CXXDynamicCastExpr>(s))
    if (e->getType()->isReferenceType())
      return true;

  if (isa<CXXTypeidExpr>(s))
    return true;

  for (const Stmt *child : s->children())
    if (guardedInitMayThrow(child))
      return true;

  return false;
}

class CIRGenMicrosoftCXXABI : public CIRGenCXXABI {
  using VFTableIdTy = std::pair<const CXXRecordDecl *, CharUnits>;
  llvm::DenseMap<VFTableIdTy, cir::GlobalOp> vtables;

  /// Per-DeclContext counter used to number the thread-safe guard variables
  /// of internal-linkage local statics (mirrors classic CodeGen's
  /// MicrosoftCXXABI::ThreadSafeGuardNumMap). Externally-visible statics are
  /// numbered by Sema instead, via ASTContext::getStaticLocalNumber.
  llvm::DenseMap<const DeclContext *, unsigned> threadSafeGuardNumMap;

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

// The idea here is creating a separate block for the throw with an
// `UnreachableOp` as the terminator. So, we branch from the current block
// to the throw block and create a block for the remaining operations.
// Mirrors the identically-named static helper in CIRGenItaniumCXXABI.cpp.
static void insertThrowAndSplit(mlir::OpBuilder &builder, mlir::Location loc,
                                mlir::Value exceptionPtr = {},
                                mlir::FlatSymbolRefAttr typeInfo = {},
                                mlir::FlatSymbolRefAttr dtor = {}) {
  mlir::Block *currentBlock = builder.getInsertionBlock();
  mlir::Region *region = currentBlock->getParent();

  if (currentBlock->empty()) {
    cir::ThrowOp::create(builder, loc, exceptionPtr, typeInfo, dtor);
    cir::UnreachableOp::create(builder, loc);
  } else {
    mlir::Block *throwBlock = builder.createBlock(region);

    cir::ThrowOp::create(builder, loc, exceptionPtr, typeInfo, dtor);
    cir::UnreachableOp::create(builder, loc);

    builder.setInsertionPointToEnd(currentBlock);
    cir::BrOp::create(builder, loc, throwBlock);
  }

  (void)builder.createBlock(region);
}

void CIRGenMicrosoftCXXABI::emitRethrow(CIRGenFunction &cgf, bool isNoReturn) {
  // A bare `throw;` (rethrowing the currently-active exception) is distinct
  // from `emitThrow` above but faces the identical scoping question: the
  // ABI-accurate MS lowering calls `_CxxThrowException(nullptr, nullptr)`
  // (see MicrosoftCXXABI::emitRethrow in clang/lib/CodeGen/MicrosoftCXXABI.cpp),
  // which is EH-runtime-ABI machinery belonging to a later LLVM-lowering pass
  // that `cir.throw` does not yet target for any ABI (its only existing
  // lowering, CIRToLLVMThrowOpLowering, targets the Itanium
  // __cxa_throw/__cxa_rethrow runtime regardless of source ABI). So, as with
  // `emitThrow`, we produce the same ABI-agnostic CIR shape
  // CIRGenItaniumCXXABI::emitRethrow produces: a no-operand `cir.throw`,
  // which is `cir.throw`'s existing spelling for "rethrow the active
  // exception" (lowers to a call to `__cxa_rethrow` with no arguments).
  if (isNoReturn) {
    CIRGenBuilderTy &builder = cgf.getBuilder();
    assert(cgf.currSrcLoc && "expected source location");
    mlir::Location loc = *cgf.currSrcLoc;
    insertThrowAndSplit(builder, loc);
  } else {
    cgm.errorNYI(*cgf.currSrcLoc,
                 "Microsoft C++ ABI rethrow lowering with isNoReturn false");
  }
}

void CIRGenMicrosoftCXXABI::emitThrow(CIRGenFunction &cgf,
                                      const CXXThrowExpr *e) {
  // A full, ABI-accurate MS C++ throw needs the runtime's ThrowInfo /
  // CatchableType / CatchableTypeArray descriptor structures and a call to
  // `_CxxThrowException` (see MicrosoftCXXABI::getThrowInfo and friends in
  // clang/lib/CodeGen/MicrosoftCXXABI.cpp) -- that is EH-runtime-ABI
  // machinery well beyond what ClangIR codegen (AST -> CIR) needs to model;
  // `cir.throw`'s only existing lowering (CIRToLLVMThrowOpLowering) targets
  // the Itanium __cxa_throw/__cxa_rethrow runtime regardless of source ABI,
  // and MS-specific LLVM lowering does not exist yet. That lowering-level
  // gap is out of scope here.
  //
  // What we do here is produce the same ABI-agnostic CIR shape
  // CIRGenItaniumCXXABI::emitThrow produces (allocate the exception object,
  // materialize the thrown expression into it, look up RTTI, emit
  // `cir.throw`), which is sufficient for ClangIR codegen to succeed. Unlike
  // the Itanium path (which bails out for a non-trivial destructor), we also
  // compute and attach the destructor operand `cir.throw` already supports,
  // since real MS ABI throw expressions commonly throw types with a
  // user-declared/virtual destructor (e.g. std::bad_array_new_length via the
  // MSVC STL's `_Throw_bad_array_new_length`, whose ~exception is virtual).
  CIRGenBuilderTy &builder = cgf.getBuilder();
  QualType clangThrowType = e->getSubExpr()->getType();
  cir::PointerType throwTy =
      builder.getPointerTo(cgf.convertType(clangThrowType));
  uint64_t typeSize =
      cgf.getContext().getTypeSizeInChars(clangThrowType).getQuantity();
  mlir::Location subExprLoc = cgf.getLoc(e->getSubExpr()->getSourceRange());

  // Defer computing allocation size to some later lowering pass.
  mlir::TypedValue<cir::PointerType> exceptionPtr =
      cir::AllocExceptionOp::create(builder, subExprLoc, throwTy,
                                    builder.getI64IntegerAttr(typeSize))
          .getAddr();

  // Build expression and store its result into exceptionPtr.
  CharUnits exnAlign = cgf.getContext().getExnObjectAlignment();
  cgf.emitAnyExprToExn(e->getSubExpr(), Address(exceptionPtr, exnAlign));

  // Get the RTTI symbol address.
  auto typeInfo = mlir::cast<cir::GlobalViewAttr>(
      cgm.getAddrOfRTTIDescriptor(subExprLoc, clangThrowType,
                                  /*forEH=*/true));
  assert(!typeInfo.getIndices() && "expected no indirection");

  // The address of the destructor, if the thrown type needs one run during
  // unwind. Mirrors the "use the base destructor variant in place of the
  // complete destructor variant if the class has no virtual bases" choice
  // CIRGenMicrosoftCXXABI::emitDestructorCall makes elsewhere in this file.
  mlir::FlatSymbolRefAttr dtor;
  if (const RecordType *recordTy = clangThrowType->getAs<RecordType>()) {
    auto *rec = cast<CXXRecordDecl>(recordTy->getDecl()->getDefinition());
    if (!rec->hasTrivialDestructor()) {
      const CXXDestructorDecl *dd = rec->getDestructor();
      CXXDtorType dtorType = Dtor_Complete;
      if (dd->getParent()->getNumVBases() == 0)
        dtorType = Dtor_Base;
      cir::FuncOp dtorFn = cgm.getAddrOfCXXStructor(GlobalDecl(dd, dtorType));
      dtor = mlir::FlatSymbolRefAttr::get(dtorFn.getSymNameAttr());
    }
  }

  // Now throw the exception.
  mlir::Location loc = cgf.getLoc(e->getSourceRange());
  insertThrowAndSplit(builder, loc, exceptionPtr, typeInfo.getSymbol(), dtor);
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
  if (forVirtualBase) {
    // A constructor or destructor destroying one of its own virtual bases on
    // an exception path needs to first check (via a "is-most-derived-class"
    // flag) whether it is actually responsible for that virtual base, since
    // only the most-derived object's constructor/destructor is. That check
    // (classic CodeGen's EmitDtorCompleteObjectHandler) is not modeled here
    // yet.
    cgm.errorNYI(dd->getSourceRange(),
                 "Microsoft C++ ABI virtual-base destructor call from a "
                 "constructor/destructor exception handler");
    return;
  }

  // Use the base destructor variant in place of the complete destructor
  // variant if the class has no virtual bases. This effectively implements
  // some of the -mconstructor-aliases optimization, but as part of the MS
  // C++ ABI. Mirrors classic CodeGen's MicrosoftCXXABI::EmitDestructorCall.
  if (type == Dtor_Complete && dd->getParent()->getNumVBases() == 0)
    type = Dtor_Base;

  if (type == Dtor_Deleting || type == Dtor_VectorDeleting) {
    cgm.errorNYI(dd->getSourceRange(),
                 "Microsoft C++ ABI deleting destructor call");
    return;
  }

  GlobalDecl gd(dd, type);
  CIRGenCallee callee = CIRGenCallee::forDirect(cgm.getAddrOfCXXStructor(gd),
                                                gd);

  // A direct-name call to a virtual destructor still needs the ABI's usual
  // "this" prologue adjustment for a non-virtual call to a virtual method
  // (getVirtualFunctionPrologueThisAdjustment in classic CodeGen). For a
  // destructor that adjustment is always zero here: the complete-object
  // destructor (kept above whenever the class has virtual bases) takes a
  // pointer to the complete object and needs no adjustment, and the base
  // destructor's slot in the deleting-destructor vftable entry that classic
  // CodeGen looks up for the adjustment can only carry a nonzero offset when
  // that vftable slot lives inside a virtual base subobject -- which cannot
  // happen for a class with no virtual bases (the only case demoted to
  // Dtor_Base above). So no "this" adjustment is needed on this call path.
  assert(!dd->isVirtual() ||
         (type == Dtor_Complete || dd->getParent()->getNumVBases() == 0));

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
  mlir::Location loc = cgf.getLoc(d.getSourceRange());
  CIRGenBuilderTy &builder = cgf.getBuilder();

  // MSVC only guards static locals this way; dynamically-initialized globals
  // with weak/linkonce linkage go through the ordinary global-init path
  // (classic CodeGen's CodeGenFunction::EmitCXXGlobalVarDeclInit dispatch for
  // a non-static-local VarDecl), which is not modeled through this entry
  // point yet.
  if (!d.isStaticLocal()) {
    cgm.errorNYI(d.getSourceRange(),
                 "Microsoft C++ ABI guarded initialization of a "
                 "non-static-local variable");
    return;
  }

  if (d.getTLSKind()) {
    cgm.errorNYI(d.getSourceRange(),
                 "Microsoft C++ ABI guarded local static initialization: "
                 "thread-local variable");
    return;
  }

  if (d.needsDestruction(cgm.getASTContext()) == QualType::DK_cxx_destructor) {
    cgm.errorNYI(d.getSourceRange(),
                 "Microsoft C++ ABI guarded local static initialization: "
                 "variable with a non-trivial destructor");
    return;
  }

  if (cgm.getLangOpts().Exceptions && shouldPerformInit &&
      guardedInitMayThrow(d.getInit())) {
    cgm.errorNYI(d.getSourceRange(),
                 "Microsoft C++ ABI guarded local static initialization: "
                 "potentially-throwing initializer");
    return;
  }

  // Thread-safe statics (the default since C++11, both for MSVC and for
  // Clang in MS ABI mode) use a per-variable guard following the algorithm
  // in N2325's appendix: the guard starts at a value less than or equal to
  // any completed initialization's epoch, and each thread compares it
  // against the shared `_Init_thread_epoch` counter to decide whether
  // initialization has already happened, without needing to take a lock in
  // the common (already-initialized) case. `-fno-threadsafe-statics`'s
  // simpler per-function bitmask guard scheme is not implemented here.
  if (!cgm.getLangOpts().ThreadsafeStatics) {
    cgm.errorNYI(d.getSourceRange(),
                 "Microsoft C++ ABI guarded local static initialization: "
                 "-fno-threadsafe-statics bitmask guard");
    return;
  }

  cir::IntType guardTy = cgm.sInt32Ty;
  CharUnits guardAlign = CharUnits::fromQuantity(4);

  unsigned guardNum;
  if (d.isExternallyVisible()) {
    // Externally visible variables are numbered in Sema so unreachable
    // VarDecls (e.g. in discarded template instantiations) are still
    // numbered consistently across translation units.
    guardNum = cgm.getASTContext().getStaticLocalNumber(&d);
    assert(guardNum > 0);
    --guardNum;
  } else {
    guardNum = threadSafeGuardNumMap[d.getDeclContext()]++;
  }

  SmallString<256> guardName;
  {
    llvm::raw_svector_ostream guardOut(guardName);
    cast<MicrosoftMangleContext>(getMangleContext())
        .mangleThreadSafeStaticGuardVariable(&d, guardNum, guardOut);
  }

  cir::GlobalOp guard = cgm.createOrReplaceCXXRuntimeVariable(
      loc, guardName, guardTy, var.getLinkage(), guardAlign);
  guard.setInitialValueAttr(builder.getZeroInitAttr(guardTy));

  mlir::Value guardPtr = builder.createGetGlobal(loc, guard);
  Address guardAddr(guardPtr, guardTy, guardAlign);

  // `_Init_thread_epoch` is a thread-local counter owned by the C runtime;
  // its TLS access model is fixed by the runtime's own definition and does
  // not follow the module's `-ftls-model=`, matching classic CodeGen.
  cir::GlobalOp epoch = cgm.createOrReplaceCXXRuntimeVariable(
      loc, "_Init_thread_epoch", guardTy,
      cir::GlobalLinkageKind::ExternalLinkage, guardAlign);
  epoch.setTlsModel(cir::TLS_Model::GeneralDynamic);
  mlir::Value epochPtr =
      builder.createGetGlobal(loc, epoch, /*threadLocal=*/true);
  Address epochAddr(epochPtr, guardTy, guardAlign);

  cir::FuncType initThreadFnTy =
      builder.getFuncType({guardPtr.getType()}, builder.getVoidTy());
  cir::FuncOp initThreadHeaderFn =
      cgm.createRuntimeFunction(initThreadFnTy, "_Init_thread_header");
  cir::FuncOp initThreadFooterFn =
      cgm.createRuntimeFunction(initThreadFnTy, "_Init_thread_footer");

  auto emitInit = [&] {
    if (!shouldPerformInit)
      return;

    mlir::Value varPtr =
        builder.createGetGlobal(loc, var, d.getTLSKind() != VarDecl::TLS_None);
    Address varAddr(varPtr, cgf.convertTypeForMem(d.getType()),
                    cgf.getContext().getDeclAlign(&d));
    cgf.emitAnyExprToMem(d.getInit(), varAddr, d.getType().getQualifiers(),
                         true);
  };

  // Pseudo code for the test:
  //   if (Guard > _Init_thread_epoch) {
  //     _Init_thread_header(&Guard);
  //     if (Guard == -1) {
  //       ... initialize the object ...
  //       _Init_thread_footer(&Guard);
  //     }
  //   }
  cir::LoadOp firstGuardLoad = builder.createLoad(loc, guardAddr);
  firstGuardLoad.setMemOrder(cir::MemOrder::Relaxed);
  cir::LoadOp initThreadEpoch = builder.createLoad(loc, epochAddr);
  mlir::Value isUninitialized = builder.createCompare(
      loc, cir::CmpOpKind::gt, firstGuardLoad.getResult(),
      initThreadEpoch.getResult());

  cir::IfOp::create(
      builder, loc, isUninitialized, false,
      [&](mlir::OpBuilder &, mlir::Location) {
        cgf.emitRuntimeCall(loc, initThreadHeaderFn, {guardPtr});

        cir::LoadOp secondGuardLoad = builder.createLoad(loc, guardAddr);
        secondGuardLoad.setMemOrder(cir::MemOrder::Relaxed);
        mlir::Value minusOne = builder.getSInt32(-1, loc);
        mlir::Value shouldDoInit = builder.createCompare(
            loc, cir::CmpOpKind::eq, secondGuardLoad.getResult(), minusOne);

        cir::IfOp::create(builder, loc, shouldDoInit, false,
                          [&](mlir::OpBuilder &, mlir::Location) {
                            emitInit();
                            cgf.emitRuntimeCall(loc, initThreadFooterFn,
                                                {guardPtr});
                            builder.createYield(loc);
                          });
        builder.createYield(loc);
      });
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
