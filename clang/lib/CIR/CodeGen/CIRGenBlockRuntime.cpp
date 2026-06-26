//===--- CIRGenBlockRuntime.cpp - ClangIR Blocks runtime ABI --------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CIRGenBlockRuntime.h"
#include "CIRGenFunction.h"
#include "CIRGenModule.h"
#include "CIRGenObjCRuntime.h"
#include "clang/AST/ASTContext.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "clang/CIR/Dialect/IR/CIRTypes.h"

using namespace clang;
using namespace clang::CIRGen;

CIRGenBlockRuntime::~CIRGenBlockRuntime() = default;

namespace {
enum DarwinBlockFlags : uint32_t {
  BLOCK_IS_NOESCAPE = 1u << 23,
  BLOCK_HAS_COPY_DISPOSE = 1u << 25,
  BLOCK_HAS_CXX_OBJ = 1u << 26,
  BLOCK_IS_GLOBAL = 1u << 28,
  BLOCK_USE_STRET = 1u << 29,
  BLOCK_HAS_SIGNATURE = 1u << 30,
};

class CIRGenDarwinBlockRuntime final : public CIRGenBlockRuntime {
public:
  using CIRGenBlockRuntime::CIRGenBlockRuntime;

  static constexpr unsigned HeaderFieldCount = 5;

  mlir::Type getOpaquePtrTy(CIRGenFunction &cgf) {
    return cgf.convertType(cgf.getContext().VoidPtrTy);
  }

  cir::RecordType getGenericBlockLiteralType(CIRGenFunction &cgf) {
    CIRGenBuilderTy &builder = cgf.getBuilder();
    mlir::Type voidPtr = getOpaquePtrTy(cgf);
    return builder.getAnonRecordTy({voidPtr, builder.getSInt32Ty(),
                                    builder.getSInt32Ty(), voidPtr, voidPtr});
  }

  mlir::Value getRuntimeGlobal(CIRGenFunction &cgf, mlir::Location loc,
                               StringRef name) {
    mlir::Type opaquePtrTy = getOpaquePtrTy(cgf);
    cir::GlobalOp global = cgm.createOrReplaceRuntimeVariable(
        loc, name, opaquePtrTy, cir::GlobalLinkageKind::ExternalLinkage,
        cgf.getPointerAlign());
    mlir::Value addr = cgf.getBuilder().createGetGlobal(loc, global);
    return cgf.getBuilder().createLoad(
        loc, Address(addr, opaquePtrTy, cgf.getPointerAlign()));
  }

  mlir::Value getNSConcreteStackBlock(CIRGenFunction &cgf, mlir::Location loc) {
    return getRuntimeGlobal(cgf, loc, "_NSConcreteStackBlock");
  }
  mlir::Value getNSConcreteGlobalBlock(CIRGenFunction &cgf, mlir::Location loc) {
    return getRuntimeGlobal(cgf, loc, "_NSConcreteGlobalBlock");
  }

  CIRGenBlockInfo computeBlockInfo(CIRGenFunction &cgf,
                                   const BlockExpr *expr) override {
    CIRGenBlockInfo info;
    info.blockExpr = expr;
    info.blockDecl = expr->getBlockDecl();
    info.noEscape = info.blockDecl->doesNotEscape();
    // Do not set BLOCK_HAS_SIGNATURE until CIR can materialize the exact
    // ObjC @encode string into the descriptor initializer.
    info.hasSignature = false;

    CIRGenBuilderTy &builder = cgf.getBuilder();
    mlir::Type voidPtr = getOpaquePtrTy(cgf);
    SmallVector<mlir::Type, 8> members = {
        voidPtr, builder.getSInt32Ty(), builder.getSInt32Ty(), voidPtr, voidPtr};

    unsigned nextField = HeaderFieldCount;
    if (info.blockDecl->capturesCXXThis()) {
      CIRGenBlockCaptureInfo capture;
      capture.kind = CIRGenBlockCaptureKind::CXXThis;
      capture.fieldType = cgf.getContext().VoidPtrTy;
      capture.cirType = voidPtr;
      capture.alignment = cgf.getPointerAlign();
      capture.fieldIndex = nextField++;
      info.cxxThisFieldIndex = capture.fieldIndex;
      info.captures.push_back(capture);
      members.push_back(capture.cirType);
    }

    for (const BlockDecl::Capture &bc : info.blockDecl->captures()) {
      const VarDecl *var = bc.getVariable();
      const VarDecl *canonicalVar = var->getCanonicalDecl();
      CIRGenBlockCaptureInfo capture;
      capture.variable = var;
      if (bc.isByRef() || var->isEscapingByref())
        capture.kind = CIRGenBlockCaptureKind::ByRef;
      else if (var->getType().getObjCLifetime() == Qualifiers::OCL_Weak)
        capture.kind = CIRGenBlockCaptureKind::ObjCWeak;
      else if (var->getType().getObjCLifetime() == Qualifiers::OCL_Strong)
        capture.kind = CIRGenBlockCaptureKind::ObjCStrong;
      else if (var->getType()->isBlockPointerType())
        capture.kind = CIRGenBlockCaptureKind::BlockObject;
      else if (var->needsDestruction(cgf.getContext()) || bc.hasCopyExpr())
        capture.kind = CIRGenBlockCaptureKind::CXXNonTrivial;
      else
        capture.kind = CIRGenBlockCaptureKind::ScalarOrPointer;

      capture.fieldType = var->getType();
      capture.cirType = cgf.convertTypeForMem(capture.fieldType);
      capture.alignment = cgm.getNaturalTypeAlignment(capture.fieldType, nullptr);
      capture.fieldIndex = nextField++;
      info.captureIndex[canonicalVar] = info.captures.size();
      info.needsCopyDispose |= capture.needsCopyDispose();
      info.hasCXXObject |= capture.kind == CIRGenBlockCaptureKind::CXXNonTrivial;
      info.captures.push_back(capture);
      members.push_back(capture.cirType);
    }

    // Keep no-capture blocks as stack literals in phase 1. Turning this on
    // requires emitting a true global block object, not just setting
    // BLOCK_IS_GLOBAL on a stack alloca.
    info.canBeGlobal = false;
    info.literalType = builder.getAnonRecordTy(members);
    info.literalAlign = cgf.getPointerAlign();
    llvm::TypeSize literalSize =
        cgm.getDataLayout().getTypeAllocSize(info.literalType);
    info.literalSize = CharUnits::fromQuantity(
        literalSize.isScalable() ? 0 : literalSize.getFixedValue());
    mlir::Type uLongTy = cgf.convertType(cgf.getContext().UnsignedLongTy);
    info.descriptorType = builder.getAnonRecordTy(
        {uLongTy, uLongTy, voidPtr, voidPtr, voidPtr, voidPtr});
    return info;
  }

  bool checkPhase1Supported(CIRGenFunction &, const CIRGenBlockInfo &info) {
    for (const CIRGenBlockCaptureInfo &capture : info.captures) {
      switch (capture.kind) {
      case CIRGenBlockCaptureKind::Constant:
      case CIRGenBlockCaptureKind::ScalarOrPointer:
      case CIRGenBlockCaptureKind::CXXThis:
        break;
      case CIRGenBlockCaptureKind::ByRef:
        cgm.errorNYI(info.blockExpr->getSourceRange(),
                     "block byref capture lowering");
        return false;
      case CIRGenBlockCaptureKind::ObjCStrong:
      case CIRGenBlockCaptureKind::ObjCWeak:
      case CIRGenBlockCaptureKind::BlockObject:
        cgm.errorNYI(info.blockExpr->getSourceRange(),
                     "block capture requiring copy/dispose helper");
        return false;
      case CIRGenBlockCaptureKind::CXXNonTrivial:
        cgm.errorNYI(info.blockExpr->getSourceRange(),
                     "block capture of non-trivial C++ object");
        return false;
      }
    }
    return true;
  }

  uint32_t getInitialFlags(const CIRGenBlockInfo &info) {
    uint32_t flags = 0;
    if (info.hasSignature)
      flags |= BLOCK_HAS_SIGNATURE;
    if (info.needsCopyDispose)
      flags |= BLOCK_HAS_COPY_DISPOSE;
    if (info.hasCXXObject)
      flags |= BLOCK_HAS_CXX_OBJ;
    if (info.usesStret)
      flags |= BLOCK_USE_STRET;
    if (info.canBeGlobal || info.noEscape)
      flags |= BLOCK_IS_GLOBAL;
    if (info.noEscape)
      flags |= BLOCK_IS_NOESCAPE;
    return flags;
  }

  bool hasSupportedCaptures(const CIRGenBlockInfo &info) {
    if (info.captures.empty())
      return true;
    cgm.errorNYI(info.blockExpr->getSourceRange(), "block capture lowering");
    return false;
  }

  cir::FuncOp emitInvokeFunction(CIRGenFunction &parent,
                                 const CIRGenBlockInfo &info) {
    const auto *blockTy = info.blockExpr->getType()->castAs<BlockPointerType>();
    const auto *fnTy = blockTy->getPointeeType()->castAs<FunctionProtoType>();
    ASTContext &ctx = cgm.getASTContext();

    FunctionArgList args;
    IdentifierInfo *selfName = &ctx.Idents.get(".block_descriptor");
    auto *selfDecl = ImplicitParamDecl::Create(
        ctx, const_cast<BlockDecl *>(info.blockDecl),
        info.blockExpr->getCaretLocation(), selfName, ctx.VoidPtrTy,
        ImplicitParamKind::ObjCSelf);
    args.push_back(selfDecl);
    for (ParmVarDecl *param : info.blockDecl->parameters())
      args.push_back(param);

    SmallVector<CanQualType, 8> argTypes;
    argTypes.push_back(ctx.getCanonicalParamType(ctx.VoidPtrTy));
    for (ParmVarDecl *param : info.blockDecl->parameters())
      argTypes.push_back(ctx.getCanonicalParamType(param->getType()));
    const CIRGenFunctionInfo &fnInfo = cgm.getTypes().arrangeCIRFunctionInfo(
        fnTy->getReturnType()->getCanonicalTypeUnqualified(), argTypes,
        RequiredArgs::All);
    cir::FuncType cirFnTy = cgm.getTypes().getFunctionType(fnInfo);

    std::string name = cgm.getUniqueGlobalName("__cir_block_invoke");
    cir::FuncOp fn = cgm.createCIRFunction(
        parent.getLoc(info.blockExpr->getCaretLocation()), name, cirFnTy,
        nullptr);
    fn.setLinkage(cir::GlobalLinkageKind::InternalLinkage);
    fn.addEntryBlock();

    mlir::OpBuilder::InsertionGuard guard(cgm.getBuilder());
    CIRGenFunction cgf(cgm, cgm.getBuilder(), /*suppressNewContext=*/true);
    CIRGenModule::CurCGFGuard curCGFGuard(cgm, cgf);
    cgf.curBlockInfo = &info;
    cgf.curGD = GlobalDecl(info.blockDecl);
    {
      CIRGenFunction::SymTableScopeTy varScope(cgf.symbolTable);
      CIRGenFunction::LexicalScope lexScope(
          cgf, parent.getLoc(info.blockExpr->getSourceRange()),
          &fn.getBlocks().front());
      cgf.startFunction(GlobalDecl(info.blockDecl), fnTy->getReturnType(), fn,
                        cirFnTy, args, info.blockExpr->getCaretLocation(),
                        info.blockExpr->getBody()->getBeginLoc());
      cgf.curFuncDecl = parent.curFuncDecl;
      cgf.curCodeDecl = info.blockDecl;

      Address selfAddr = cgf.getAddrOfLocalVar(selfDecl);
      cgf.blockPointer = cgf.getBuilder().createLoad(
          cgf.getLoc(info.blockExpr->getCaretLocation()), selfAddr);

      (void)cgf.emitFunctionBody(info.blockExpr->getBody());
      cgf.finishFunction(info.blockExpr->getBody()->getEndLoc());
    }
    cgf.curBlockInfo = nullptr;
    cgf.blockPointer = nullptr;
    return fn;
  }

  mlir::Value getFunctionPointer(CIRGenFunction &cgf, mlir::Location loc,
                                 cir::FuncOp fn) {
    mlir::Type fnPtrTy = cir::PointerType::get(fn.getFunctionType());
    return cir::GetGlobalOp::create(cgf.getBuilder(), loc, fnPtrTy,
                                    fn.getSymNameAttr());
  }

  cir::GlobalOp emitDescriptor(CIRGenFunction &cgf, const CIRGenBlockInfo &info,
                               mlir::Location loc) {
    CIRGenBuilderTy &builder = cgf.getBuilder();
    mlir::Type voidPtr = getOpaquePtrTy(cgf);
    mlir::Type uLongTy = cgf.convertType(cgf.getContext().UnsignedLongTy);
    auto zeroULong = cir::IntAttr::get(uLongTy, 0);
    auto sizeULong = cir::IntAttr::get(uLongTy, info.literalSize.getQuantity());
    auto nullPtr = cir::ZeroAttr::get(voidPtr);
    SmallVector<mlir::Attribute, 6> fields = {zeroULong, sizeULong, nullPtr,
                                              nullPtr, nullPtr, nullPtr};
    auto init = cir::ConstRecordAttr::get(info.descriptorType,
                                          builder.getArrayAttr(fields));
    cir::GlobalOp global = CIRGenModule::createGlobalOp(
        cgm, loc, cgm.getUniqueGlobalName("__block_descriptor"),
        info.descriptorType, /*isConstant=*/true);
    global.setLinkage(cir::GlobalLinkageKind::InternalLinkage);
    cgm.setInitializer(global, init);
    return global;
  }

  void storeField(CIRGenFunction &cgf, mlir::Location loc, Address blockAddr,
                  unsigned fieldIndex, mlir::Value value, mlir::Type fieldTy,
                  StringRef name) {
    mlir::Value addr = cgf.getBuilder().createGetMember(
        loc, cir::PointerType::get(fieldTy), blockAddr.getPointer(), name,
        fieldIndex);
    cgf.getBuilder().createStore(loc, value,
                                 Address(addr, fieldTy, blockAddr.getAlignment()));
  }

  mlir::Value emitBlockLiteral(CIRGenFunction &cgf,
                               const BlockExpr *expr) override {
    mlir::Location loc = cgf.getLoc(expr->getSourceRange());
    if (cgf.getLangOpts().OpenCL) {
      cgm.errorNYI(expr->getSourceRange(), "OpenCL block literal lowering");
      return cgf.getBuilder().getNullPtr(cgf.convertType(expr->getType()), loc);
    }

    CIRGenBlockInfo info = computeBlockInfo(cgf, expr);
    if (!checkPhase1Supported(cgf, info) || !hasSupportedCaptures(info))
      return cgf.getBuilder().getNullPtr(cgf.convertType(expr->getType()), loc);
    if (!expr->getFunctionType()->getReturnType()->isVoidType()) {
      cgm.errorNYI(expr->getSourceRange(), "non-void block literal lowering");
      return cgf.getBuilder().getNullPtr(cgf.convertType(expr->getType()), loc);
    }

    cir::FuncOp invokeFn = emitInvokeFunction(cgf, info);
    cir::GlobalOp descriptor = emitDescriptor(cgf, info, loc);
    Address block = cgf.createTempAlloca(info.literalType, info.literalAlign,
                                         loc, "block");

    CIRGenBuilderTy &builder = cgf.getBuilder();
    mlir::Type voidPtr = getOpaquePtrTy(cgf);
    mlir::Value isa = getNSConcreteStackBlock(cgf, loc);
    mlir::Value flags =
        builder.getConstInt(loc, builder.getSInt32Ty(), getInitialFlags(info));
    mlir::Value reserved = builder.getConstInt(loc, builder.getSInt32Ty(), 0);
    mlir::Value invoke = getFunctionPointer(cgf, loc, invokeFn);
    invoke = builder.createBitcast(loc, invoke, voidPtr);
    mlir::Value descAddr = builder.createGetGlobal(loc, descriptor);
    descAddr = builder.createBitcast(loc, descAddr, voidPtr);

    storeField(cgf, loc, block, 0, isa, voidPtr, "block.isa");
    storeField(cgf, loc, block, 1, flags, builder.getSInt32Ty(), "block.flags");
    storeField(cgf, loc, block, 2, reserved, builder.getSInt32Ty(),
               "block.reserved");
    storeField(cgf, loc, block, 3, invoke, voidPtr, "block.invoke");
    storeField(cgf, loc, block, 4, descAddr, voidPtr, "block.descriptor");

    mlir::Type resultTy = cgf.convertType(expr->getType());
    return builder.createBitcast(loc, block.getPointer(), resultTy);
  }

  RValue emitBlockCallExpr(CIRGenFunction &cgf, const CallExpr *expr,
                           ReturnValueSlot returnValue) override {
    cgm.errorNYI(expr->getSourceRange(), "block indirect invoke lowering");
    return RValue::get(nullptr);
  }

  Address getAddrOfBlockDecl(CIRGenFunction &cgf,
                             const VarDecl *variable) override {
    if (!cgf.curBlockInfo || !cgf.blockPointer) {
      cgm.errorNYI(variable->getSourceRange(), "block capture outside invoke");
      return Address::invalid();
    }
    const CIRGenBlockCaptureInfo *capture = cgf.curBlockInfo->getCapture(variable);
    if (!capture) {
      cgm.errorNYI(variable->getSourceRange(), "block capture lookup failure");
      return Address::invalid();
    }
    if (capture->isByRef()) {
      cgm.errorNYI(variable->getSourceRange(), "block byref capture address");
      return Address::invalid();
    }
    mlir::Location loc = cgf.getLoc(variable->getSourceRange());
    mlir::Value block = cgf.blockPointer;
    auto blockPtrTy = cir::PointerType::get(cgf.curBlockInfo->literalType);
    if (block.getType() != blockPtrTy)
      block = cgf.getBuilder().createBitcast(loc, block, blockPtrTy);
    mlir::Value addr = cgf.getBuilder().createGetMember(
        loc, cir::PointerType::get(capture->cirType), block, "block.capture",
        capture->fieldIndex);
    return Address(addr, capture->cirType, capture->alignment);
  }
};
} // namespace

std::unique_ptr<CIRGenBlockRuntime>
clang::CIRGen::createCIRGenBlockRuntime(CIRGenModule &cgm) {
  return std::make_unique<CIRGenDarwinBlockRuntime>(cgm);
}
