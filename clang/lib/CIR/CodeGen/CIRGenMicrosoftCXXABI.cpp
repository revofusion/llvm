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
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringSet.h"
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

  /// Classes whose vftable(s) have already been queued for (possibly
  /// deferred) emission via CIRGenModule::addDeferredVTable. Mirrors classic
  /// CodeGen's MicrosoftCXXABI::DeferredVFTables; without this, a vftable
  /// global created by getAddrOfVTable is only ever declared, never
  /// defined, since nothing else in CIR triggers emitVirtualInheritanceTables
  /// for the MS ABI's per-subobject vftables.
  llvm::SmallPtrSet<const CXXRecordDecl *, 4> deferredVFTables;

  /// Per-DeclContext counter used to number the thread-safe guard variables
  /// of internal-linkage local statics (mirrors classic CodeGen's
  /// MicrosoftCXXABI::ThreadSafeGuardNumMap). Externally-visible statics are
  /// numbered by Sema instead, via ASTContext::getStaticLocalNumber.
  llvm::DenseMap<const DeclContext *, unsigned> threadSafeGuardNumMap;

  /// Cache of a class's vbtables: the globals holding, for each subobject
  /// introducing a distinct vbptr, the offsets from that vbptr to each of
  /// its virtual bases (and to itself). Mirrors classic CodeGen's
  /// MicrosoftCXXABI::VBTableGlobals/VBTablesMap.
  struct VBTableGlobals {
    const VPtrInfoVector *vbTables = nullptr;
    SmallVector<cir::GlobalOp, 2> globals;
  };
  llvm::DenseMap<const CXXRecordDecl *, VBTableGlobals> vbTablesMap;

  /// Caching wrapper around MicrosoftVTableContext::enumerateVBTables(),
  /// also populating (and eagerly defining, where the linkage calls for it)
  /// each vbtable's own global.
  const VBTableGlobals &enumerateVBTables(const CXXRecordDecl *rd);
  cir::GlobalOp getAddrOfVBTable(const VPtrInfo &vbt, const CXXRecordDecl *rd,
                                 cir::GlobalLinkageKind linkage);
  void emitVBTableDefinition(const VPtrInfo &vbt, const CXXRecordDecl *rd,
                             cir::GlobalOp gv);

  /// Stores each of rd's vbtable pointers (one per vbptr-introducing
  /// subobject) at its byte offset from `this`. Called from within the
  /// is_most_derived-guarded region emitCtorCompleteObjectHandler creates,
  /// before any virtual base's constructor runs (a virtual base's own ctor
  /// can call back through paths that dereference the vbptr just stored,
  /// via getVirtualBaseClassOffset above).
  void emitVBPtrStores(CIRGenFunction &cgf, const CXXRecordDecl *rd);

public:
  CIRGenMicrosoftCXXABI(CIRGenModule &cgm) : CIRGenCXXABI(cgm) {}

  void emitCXXStructor(GlobalDecl gd) override;
  cir::IfOp emitCtorCompleteObjectHandler(CIRGenFunction &cgf,
                                          const CXXRecordDecl *rd) override;
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
  void adjustCallArgsForDestructorThunk(CIRGenFunction &cgf, GlobalDecl gd,
                                        CallArgList &callArgs) override;
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

  // Constructors with virtual bases load is_most_derived; deleting
  // destructors (scalar or vector) load should_call_delete. Both are
  // consumed the same way, via getStructorImplicitParamValue: the vbtable
  // guard (CIRGenMicrosoftCXXABI::emitCtorCompleteObjectHandler) and the
  // conditional-delete cleanup (CallDtorDeleteConditional, CIRGenClass.cpp)
  // respectively. Dtor_VectorDeleting's own array-delete handling
  // (emitConditionalArrayDtorCall) remains a separate, still-unimplemented
  // errorNYI in CIRGenFunction::emitDestructorBody -- loading the value here
  // doesn't by itself require that array-specific logic to exist.
  const auto *md = cast<CXXMethodDecl>(cgf.curGD.getDecl());
  const bool ctorNeedsImplicitParam =
      isa<CXXConstructorDecl>(md) && md->getParent()->getNumVBases();
  const bool dtorNeedsImplicitParam =
      isa<CXXDestructorDecl>(md) &&
      (cgf.curGD.getDtorType() == Dtor_Deleting ||
       cgf.curGD.getDtorType() == Dtor_VectorDeleting);
  if (ctorNeedsImplicitParam || dtorNeedsImplicitParam) {
    assert(getStructorImplicitParamDecl(cgf) &&
           "no implicit parameter for a constructor with virtual bases, or "
           "a deleting destructor?");
    setStructorImplicitParamValue(
        cgf, cgf.getBuilder().createLoad(
                 cgf.getLoc(loc),
                 cgf.getAddrOfLocalVar(getStructorImplicitParamDecl(cgf))));
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
    return added;
  }

  // All parameters are already in place except is_most_derived, which goes
  // after 'this' if the ctor is variadic and last if it's not -- see
  // getImplicitConstructorArgs/addImplicitStructorParams below, which must
  // agree on this exact position.
  const auto *cd = dyn_cast<CXXConstructorDecl>(gd.getDecl());
  if (!cd)
    return added;
  const CXXRecordDecl *classDecl = cd->getParent();
  if (classDecl->getNumVBases()) {
    const auto *fpt = cd->getType()->castAs<FunctionProtoType>();
    if (fpt->isVariadic()) {
      argTys.insert(argTys.begin() + 1, cgm.getASTContext().IntTy);
      ++added.prefix;
    } else {
      argTys.push_back(cgm.getASTContext().IntTy);
      ++added.suffix;
    }
  }
  return added;
}

CIRGenCXXABI::AddedStructorArgs
CIRGenMicrosoftCXXABI::getImplicitConstructorArgs(CIRGenFunction &cgf,
                                                  const CXXConstructorDecl *d,
                                                  CXXCtorType type,
                                                  bool forVirtualBase,
                                                  bool delegating) {
  assert(type == Ctor_Complete || type == Ctor_Base);

  // Check if we need a 'most_derived' parameter.
  if (!d->getParent()->getNumVBases())
    return AddedStructorArgs{};

  // The value depends only on `type` (and `delegating`) -- never on
  // `forVirtualBase`: a virtual base's own ctor is always invoked with
  // Ctor_Base (flag 0), correctly, because if that vbase itself has virtual
  // bases, those belong to the most-derived object's ctor, not to it.
  const auto *fpt = d->getType()->castAs<FunctionProtoType>();
  mlir::Value mostDerivedArg;
  if (delegating) {
    // Forward the caller's own flag: constructing a base subobject through a
    // delegating ctor must not re-decide whether it owns virtual bases.
    mostDerivedArg = getStructorImplicitParamValue(cgf);
  } else {
    mostDerivedArg = cgf.getBuilder().getSInt32(
        type == Ctor_Complete ? 1 : 0, cgf.getLoc(d->getSourceRange()));
  }
  QualType intTy = cgm.getASTContext().IntTy;
  if (fpt->isVariadic())
    return AddedStructorArgs::withPrefix({{mostDerivedArg, intTy}});
  return AddedStructorArgs::withSuffix({{mostDerivedArg, intTy}});
}

void CIRGenMicrosoftCXXABI::addImplicitStructorParams(CIRGenFunction &cgf,
                                                      QualType &resTy,
                                                      FunctionArgList &params) {
  const auto *md = cast<CXXMethodDecl>(cgf.curGD.getDecl());
  assert(isa<CXXConstructorDecl>(md) || isa<CXXDestructorDecl>(md));
  ASTContext &ctx = cgm.getASTContext();
  if (isa<CXXConstructorDecl>(md) && md->getParent()->getNumVBases()) {
    auto *isMostDerived = ImplicitParamDecl::Create(
        ctx, nullptr, cgf.curGD.getDecl()->getLocation(),
        &ctx.Idents.get("is_most_derived"), ctx.IntTy,
        ImplicitParamKind::Other);
    // Matches buildStructorSignature above: after 'this' if variadic, last
    // otherwise.
    const auto *fpt = md->getType()->castAs<FunctionProtoType>();
    if (fpt->isVariadic())
      params.insert(params.begin() + 1, isMostDerived);
    else
      params.push_back(isMostDerived);
    getStructorImplicitParamDecl(cgf) = isMostDerived;
  } else if (isa<CXXDestructorDecl>(md) &&
             (cgf.curGD.getDtorType() == Dtor_Deleting ||
              cgf.curGD.getDtorType() == Dtor_VectorDeleting)) {
    auto *shouldDelete = ImplicitParamDecl::Create(
        ctx, nullptr, cgf.curGD.getDecl()->getLocation(),
        &ctx.Idents.get("should_call_delete"), ctx.IntTy,
        ImplicitParamKind::Other);
    params.push_back(shouldDelete);
    getStructorImplicitParamDecl(cgf) = shouldDelete;
  }
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
    // Classic CodeGen's MicrosoftCXXABI::emitCXXStructor additionally
    // aliases Dtor_Complete to Dtor_Base (identical bodies when there are
    // no vbases, which the check above already guarantees here) and
    // aliases Dtor_VectorDeleting to Dtor_Deleting when the class doesn't
    // need a real vector deleting destructor -- both pure code-size
    // optimizations (an LLVM alias instead of a duplicate body), not
    // correctness requirements; CIRGenFunction::emitDestructorBody already
    // produces a correct (if not alias-deduplicated) body for every dtor
    // type reaching codegenCXXStructor below.
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
  return vptr.nearestVBase != nullptr;
}

void CIRGenMicrosoftCXXABI::emitVTableDefinitions(CIRGenVTables &cgvt,
                                                  const CXXRecordDecl *rd) {
  MicrosoftVTableContext &vtContext = cgm.getMicrosoftVTableContext();
  const VPtrInfoVector &vfPtrs = vtContext.getVFPtrOffsets(rd);

  // Classic CodeGen's MicrosoftCXXABI::emitVTableDefinitions has no
  // blanket RTTIData check here at all: whether an entry needs RTTI is a
  // per-vfptr, per-layout question (any_of(VTLayout.vtable_components(),
  // isRTTIKind)), handled below by the existing per-component errorNYI.
  // A blanket check here was wrong: it fired even for classes (e.g. one
  // with only a virtual base and no vfptr anywhere) whose `vfPtrs` is
  // empty and would never reach that per-component check at all --
  // spuriously erroring on classes with nothing to emit.
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

const CIRGenMicrosoftCXXABI::VBTableGlobals &
CIRGenMicrosoftCXXABI::enumerateVBTables(const CXXRecordDecl *rd) {
  // At this layer, we can key the cache off of a single class, which is much
  // easier than caching each vbtable individually.
  auto [entry, added] = vbTablesMap.try_emplace(rd);
  VBTableGlobals &vbGlobals = entry->second;
  if (!added)
    return vbGlobals;

  MicrosoftVTableContext &context = cgm.getMicrosoftVTableContext();
  vbGlobals.vbTables = &context.enumerateVBTables(rd);

  // Cache the globals for all vbtables so we don't have to recompute the
  // mangled names.
  cir::GlobalLinkageKind linkage = cgm.getVTableLinkage(rd);
  for (const std::unique_ptr<VPtrInfo> &info : *vbGlobals.vbTables)
    vbGlobals.globals.push_back(getAddrOfVBTable(*info, rd, linkage));

  return vbGlobals;
}

cir::GlobalOp
CIRGenMicrosoftCXXABI::getAddrOfVBTable(const VPtrInfo &vbt,
                                       const CXXRecordDecl *rd,
                                       cir::GlobalLinkageKind linkage) {
  SmallString<256> name;
  llvm::raw_svector_ostream out(name);
  cast<MicrosoftMangleContext>(getMangleContext())
      .mangleCXXVBTable(rd, vbt.MangledPath, out);

  cir::ArrayType vbTableType = cir::ArrayType::get(
      cgm.getBuilder().getSInt32Ty(), 1 + vbt.ObjectWithVPtr->getNumVBases());

  CharUnits alignment =
      cgm.getASTContext().getTypeAlignInChars(cgm.getASTContext().IntTy);
  cir::GlobalOp gv = cgm.createOrReplaceCXXRuntimeVariable(
      cgm.getLoc(rd->getSourceRange()), name, vbTableType, linkage, alignment);
  // Classic CodeGen also sets unnamed_addr and a DLL storage class here;
  // neither is modeled in CIR yet.
  assert(!cir::MissingFeatures::opGlobalUnnamedAddr());
  assert(!cir::MissingFeatures::opGlobalDLLImportExport());

  if (linkage != cir::GlobalLinkageKind::ExternalLinkage)
    emitVBTableDefinition(vbt, rd, gv);

  return gv;
}

void CIRGenMicrosoftCXXABI::emitVBTableDefinition(const VPtrInfo &vbt,
                                                  const CXXRecordDecl *rd,
                                                  cir::GlobalOp gv) {
  const CXXRecordDecl *objectWithVPtr = vbt.ObjectWithVPtr;

  assert(rd->getNumVBases() && objectWithVPtr->getNumVBases() &&
         "should only emit vbtables for classes with vbtables");

  const ASTRecordLayout &baseLayout =
      cgm.getASTContext().getASTRecordLayout(vbt.IntroducingObject);
  const ASTRecordLayout &derivedLayout =
      cgm.getASTContext().getASTRecordLayout(rd);

  CIRGenBuilderTy &builder = cgm.getBuilder();
  cir::IntType sInt32Ty = builder.getSInt32Ty();
  SmallVector<mlir::Attribute> offsets(1 + objectWithVPtr->getNumVBases(),
                                       nullptr);

  // The offset from ObjectWithVPtr's vbptr to itself always leads.
  CharUnits vbPtrOffset = baseLayout.getVBPtrOffset();
  offsets[0] = cir::IntAttr::get(sInt32Ty, -vbPtrOffset.getQuantity());

  MicrosoftVTableContext &context = cgm.getMicrosoftVTableContext();
  for (const CXXBaseSpecifier &base : objectWithVPtr->vbases()) {
    const CXXRecordDecl *vbase = base.getType()->getAsCXXRecordDecl();
    CharUnits offset = derivedLayout.getVBaseClassOffset(vbase);
    assert(!offset.isNegative());

    // Make it relative to the subobject vbptr.
    CharUnits completeVBPtrOffset = vbt.NonVirtualOffset + vbPtrOffset;
    if (vbt.getVBaseWithVPtr())
      completeVBPtrOffset +=
          derivedLayout.getVBaseClassOffset(vbt.getVBaseWithVPtr());
    offset -= completeVBPtrOffset;

    unsigned vbIndex = context.getVBTableIndex(objectWithVPtr, vbase);
    assert(!offsets[vbIndex] && "the same vbindex seen twice?");
    offsets[vbIndex] = cir::IntAttr::get(sInt32Ty, offset.getQuantity());
  }

  auto vbTableType = mlir::cast<cir::ArrayType>(gv.getSymType());
  assert(offsets.size() == vbTableType.getSize());
  mlir::Attribute init = builder.getConstArray(
      mlir::ArrayAttr::get(builder.getContext(), offsets), vbTableType);
  cgm.setInitializer(gv, init);

  // Classic CodeGen downgrades a dllimport vbtable to available_externally
  // once it has a definition; not modeled in CIR yet.
  assert(!cir::MissingFeatures::opGlobalDLLImportExport());
}

void CIRGenMicrosoftCXXABI::emitVBPtrStores(CIRGenFunction &cgf,
                                            const CXXRecordDecl *rd) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Location loc = cgf.getLoc(rd->getSourceRange());
  Address thisAddr = cgf.loadCXXThisAddress();
  ASTContext &ctx = cgm.getASTContext();
  const ASTRecordLayout &layout = ctx.getASTRecordLayout(rd);

  const VBTableGlobals &vbGlobals = enumerateVBTables(rd);
  for (unsigned i = 0, e = vbGlobals.vbTables->size(); i != e; ++i) {
    const std::unique_ptr<VPtrInfo> &vbt = (*vbGlobals.vbTables)[i];
    cir::GlobalOp gv = vbGlobals.globals[i];

    const ASTRecordLayout &subobjectLayout =
        ctx.getASTRecordLayout(vbt->IntroducingObject);
    CharUnits offset = vbt->NonVirtualOffset + subobjectLayout.getVBPtrOffset();
    if (vbt->getVBaseWithVPtr())
      offset += layout.getVBaseClassOffset(vbt->getVBaseWithVPtr());

    // Same byte-addressing idiom as the reader, getVirtualBaseClassOffset
    // above: bitcast `this` to u8*, stride by the byte offset, bitcast the
    // resulting slot to u8** to store through it.
    mlir::Value thisBytePtr =
        builder.createBitcast(thisAddr.getPointer(), cgm.uInt8PtrTy);
    mlir::Value offsetValue =
        builder.getConstInt(loc, cgm.ptrDiffTy, offset.getQuantity());
    mlir::Value slotBytePtr = cir::PtrStrideOp::create(
        builder, loc, cgm.uInt8PtrTy, thisBytePtr, offsetValue);
    Address slotAddr(
        builder.createBitcast(slotBytePtr, builder.getPointerTo(cgm.uInt8PtrTy)),
        cgm.uInt8PtrTy, thisAddr.getAlignment().alignmentAtOffset(offset));

    mlir::Value tablePtr = builder.createGetGlobal(loc, gv);
    tablePtr = builder.createBitcast(tablePtr, cgm.uInt8PtrTy);
    builder.createStore(loc, tablePtr, slotAddr);
  }
}

void CIRGenMicrosoftCXXABI::emitVirtualInheritanceTables(
    const CXXRecordDecl *rd) {
  const VBTableGlobals &vbGlobals = enumerateVBTables(rd);
  for (unsigned i = 0, e = vbGlobals.vbTables->size(); i != e; ++i) {
    const std::unique_ptr<VPtrInfo> &vbt = (*vbGlobals.vbTables)[i];
    cir::GlobalOp gv = vbGlobals.globals[i];
    if (!gv.hasInitializer())
      emitVBTableDefinition(*vbt, rd, gv);
  }
}

cir::IfOp CIRGenMicrosoftCXXABI::emitCtorCompleteObjectHandler(
    CIRGenFunction &cgf, const CXXRecordDecl *rd) {
  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Location loc = cgf.getLoc(rd->getSourceRange());
  mlir::Value isMostDerived = getStructorImplicitParamValue(cgf);
  assert(isMostDerived &&
         "ctor for a class with virtual bases must have an implicit parameter");
  mlir::Value isCompleteObject = builder.createCompare(
      loc, cir::CmpOpKind::ne, isMostDerived,
      builder.getNullValue(isMostDerived.getType(), loc));

  mlir::OpBuilder::InsertPoint thenBody;
  cir::IfOp ifOp = cir::IfOp::create(
      builder, loc, isCompleteObject, /*withElseRegion=*/false,
      [&](mlir::OpBuilder &b, mlir::Location) {
        thenBody = b.saveInsertionPoint();
      });
  // Deliberately not scoped by an InsertionGuard: the builder must remain
  // positioned inside the then-region after this function returns, so the
  // caller (CIRGenFunction::emitCtorPrologue) can emit the virtual-base
  // initializers directly into it before terminating the region itself.
  builder.restoreInsertionPoint(thenBody);
  // Fill in the vbtable pointers here, before any virtual base's
  // constructor runs (a vbase ctor can call back through paths that
  // dereference the vbptr just stored, via getVirtualBaseClassOffset).
  emitVBPtrStores(cgf, rd);
  return ifOp;
}

void CIRGenMicrosoftCXXABI::adjustCallArgsForDestructorThunk(
    CIRGenFunction &cgf, GlobalDecl gd, CallArgList &callArgs) {
  assert((gd.getDtorType() == Dtor_VectorDeleting ||
          gd.getDtorType() == Dtor_Deleting) &&
         "Only vector deleting destructor thunks are available in this ABI");
  callArgs.add(RValue::get(getStructorImplicitParamValue(cgf)),
               cgf.getContext().IntTy);
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

  if (deferredVFTables.insert(rd).second) {
    // We haven't processed this record type before. Queue up this vtable
    // for possible deferred emission (see CIRGenModule::emitDeferred /
    // emitDeferredVTables), mirroring classic CodeGen's identical queuing
    // in MicrosoftCXXABI::getAddrOfVTable.
    cgm.addDeferredVTable(rd);

#ifndef NDEBUG
    // Create all the vftables at once in order to make sure each vftable
    // has a unique mangled name.
    llvm::StringSet<> observedMangledNames;
    for (const std::unique_ptr<VPtrInfo> &vfPtrInfo : vfPtrs) {
      SmallString<256> vfTableName;
      llvm::raw_svector_ostream nameStream(vfTableName);
      cast<MicrosoftMangleContext>(getMangleContext())
          .mangleCXXVFTable(rd, vfPtrInfo->MangledPath, nameStream);
      if (!observedMangledNames.insert(vfTableName.str()).second)
        llvm_unreachable("Already saw this mangling before?");
    }
#endif
  }

  const std::unique_ptr<VPtrInfo> *vfPtr =
      llvm::find_if(vfPtrs, [&](const std::unique_ptr<VPtrInfo> &info) {
        return info->FullOffsetInMDC == vptrOffset;
      });
  if (vfPtr == vfPtrs.end()) {
    vtables[id] = {};
    return {};
  }

  // Classic CodeGen emits the vftable symbol as a GlobalAlias into a private
  // backing global, pointing past the RTTI slot, when -frtti-data is in
  // effect (MicrosoftCXXABI::getAddrOfVTable, VTableAliasIsRequred). CIR has
  // no global-alias support yet; report the NYI but still create the
  // backing global below, preserving the contract getVTableAddressPoint's
  // caller relies on -- that a null return here means `rd` genuinely has no
  // vfptr at vptrOffset, never "there is one, but RTTI data isn't
  // implemented". emitVTableDefinitions has its own independent RTTIData
  // guard, so no wrong initializer is emitted for this global either way.
  if (cgm.getLangOpts().RTTIData)
    cgm.errorNYI(rd->getSourceRange(),
                 "Microsoft C++ ABI vtable RTTI data reference");

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
  // See the identical annotation in CIRGenItaniumCXXABI's
  // getVirtualFunctionPointer: `methodDecl` is already resolved above, so
  // this is a no-cost, purely additive annotation with no codegen meaning.
  auto identityAttrs = buildCIRGenVirtualMethodIdentityAttrs(
      cgm.getMLIRContext(), cgm.getMangledName(gd), methodDecl);
  auto vtableSlotPtr = cir::VTableGetVirtualFnAddrOp::create(
      builder, mlirLoc, builder.getPointerTo(fnPtrTy), vtable,
      vfTableLoc.Index, identityAttrs.method, identityAttrs.methodUSR,
      identityAttrs.rootMethodUSR, identityAttrs.declaringClassUSR);
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
    // Not an error: getAddrOfVTable already gracefully returns empty when
    // this exact subobject has no vfptr slot, and that is a real,
    // expected case (this function's only caller,
    // getVTableAddressPointInStructor, is in turn called on every
    // dynamic-class subobject enumerated by CIRGenFunction::
    // getVTablePointers, including ones whose only reason for being
    // "dynamic" is a virtual base -- which needs a vbptr, handled
    // separately by emitVBPtrStores, not a vfptr at all). Mirrors classic
    // CodeGen's MicrosoftCXXABI::getVTableAddressPoint, which returns
    // whatever its VFTablesMap lookup gives (defaulting to null for an
    // absent entry) with no error path; the caller there similarly just
    // asserts, when null, that this is exactly the
    // "has vbases and !hasOwnVFPtr()" case, matched here too as a
    // debug-only sanity check rather than a hard requirement.
    assert((base.getBase()->getNumVBases() &&
            !cgm.getASTContext()
                 .getASTRecordLayout(base.getBase())
                 .hasOwnVFPtr()) &&
           "getAddrOfVTable unexpectedly found no vfptr for this subobject");
    return {};
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
  // Classic CodeGen's equivalent (MicrosoftCXXABI::getVTableAddressPointInStructor)
  // does not special-case nearestVBase at all -- it just looks up the vfptr
  // address point for `base` within `vtableClass` unconditionally. Whether
  // that vfptr lives inside a virtual base (and therefore needs a runtime,
  // not static, offset to reach it) is handled entirely by
  // isVirtualOffsetNeededForVTableField/CIRGenFunction::initializeVTablePointer
  // (which already calls the existing, verified getVirtualBaseClassOffset for
  // exactly this case) -- not here.
  return getVTableAddressPoint(base, vtableClass);
}

bool CIRGenMicrosoftCXXABI::doStructorsInitializeVPtrs(
    const CXXRecordDecl *vtableClass) {
  return true;
}

mlir::Value CIRGenMicrosoftCXXABI::performThisAdjustment(
    CIRGenFunction &cgf, Address thisAddr, const CXXRecordDecl *unadjustedClass,
    const ThunkInfo &ti) {
  const ThisAdjustment &ta = ti.This;
  if (ta.isEmpty())
    return thisAddr.getPointer();

  CIRGenBuilderTy &builder = cgf.getBuilder();
  mlir::Location loc = cgf.getLoc(unadjustedClass->getSourceRange());
  mlir::Value v;

  if (!ta.Virtual.isEmpty()) {
    // The overrider lives across a virtual-base boundary from the vfptr
    // that names this thunk (a "vtordisp" thunk): the real adjustment is a
    // *runtime* value ("vtordisp") read from an i32 slot at a fixed,
    // always-negative byte offset from `this`, negated and applied as the
    // this-pointer adjustment.
    assert(ta.Virtual.Microsoft.VtordispOffset < 0);
    if (ta.Virtual.Microsoft.VBPtrOffset) {
      // "vtordispex": the final overrider is defined in a *different*
      // virtual base than the one holding the vfptr, needing a further
      // vbtable lookup (MicrosoftCXXABI::GetVBaseOffsetFromVBPtr) on top
      // of the vtordisp adjustment above. Not implemented -- rarer than
      // the plain vtordisp case handled below.
      cgm.errorNYI(unadjustedClass->getSourceRange(),
                   "Microsoft C++ ABI vtordispex this adjustment");
      return thisAddr.getPointer();
    }

    mlir::Value thisU8 = builder.createBitcast(thisAddr.getPointer(),
                                               cgm.uInt8PtrTy);
    mlir::Value vtordispOffset = builder.getConstInt(
        loc, cgm.ptrDiffTy, ta.Virtual.Microsoft.VtordispOffset);
    mlir::Value vtordispPtr = cir::PtrStrideOp::create(
        builder, loc, cgm.uInt8PtrTy, thisU8, vtordispOffset);
    mlir::Value vtordisp = builder.createAlignedLoad(
        loc, cgm.sInt32Ty, vtordispPtr,
        cgm.getASTContext().getTypeAlignInChars(cgm.getASTContext().IntTy));
    mlir::Value vtordispNeg = builder.createNeg(vtordisp);
    mlir::Value vtordispNeg64 = builder.createIntCast(vtordispNeg, cgm.ptrDiffTy);
    v = cir::PtrStrideOp::create(builder, loc, cgm.uInt8PtrTy, thisU8,
                                 vtordispNeg64);
  } else {
    v = builder.createBitcast(thisAddr.getPointer(), cgm.uInt8PtrTy);
  }
  if (ta.NonVirtual) {
    mlir::Value offset =
        builder.getConstInt(loc, cgm.ptrDiffTy, ta.NonVirtual);
    v = cir::PtrStrideOp::create(builder, loc, cgm.uInt8PtrTy, v, offset);
  }
  return v;
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
