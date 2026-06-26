//===--- CIRGenObjCRuntime.cpp - ClangIR Objective-C runtime ABI ----------===//

#include "CIRGenObjCRuntime.h"
#include "CIRGenFunction.h"
#include "CIRGenModule.h"
#include "clang/AST/ASTContext.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "clang/CIR/Dialect/IR/CIRTypes.h"
#include "llvm/ADT/SmallVector.h"

using namespace clang;
using namespace clang::CIRGen;

CIRGenObjCRuntime::~CIRGenObjCRuntime() = default;

static std::string getObjCRuntimeGlobalName(StringRef prefix, StringRef name) {
  std::string result = prefix.str();
  static constexpr char hex[] = "0123456789ABCDEF";
  for (unsigned char c : name) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '_') {
      result.push_back(c);
      continue;
    }
    result.push_back('_');
    result.push_back(hex[c >> 4]);
    result.push_back(hex[c & 0xf]);
  }
  return result;
}

static mlir::Value emitRuntimeGlobalValue(CIRGenFunction &cgf,
                                          mlir::Location loc,
                                          StringRef name) {
  mlir::Type opaquePtrTy = cgf.convertType(cgf.getContext().VoidPtrTy);
  cir::GlobalOp global = cgf.cgm.createOrReplaceRuntimeVariable(
      loc, name, opaquePtrTy, cir::GlobalLinkageKind::ExternalLinkage,
      cgf.getPointerAlign());
  mlir::Value addr = cgf.getBuilder().createGetGlobal(loc, global);
  return cgf.getBuilder().createLoad(
      loc, Address(addr, opaquePtrTy, cgf.getPointerAlign()));
}

Address CIRGenObjCRuntime::getAddrOfSelector(CIRGenFunction &cgf,
                                             mlir::Location loc,
                                             Selector) {
  cgm.errorNYI(loc, "ObjC @selector address runtime lowering");
  mlir::Type selTy = cgf.convertType(cgf.getContext().getObjCSelType());
  mlir::Type selPtrTy = cir::PointerType::get(selTy);
  return Address(cgf.getBuilder().getNullPtr(selPtrTy, loc), selTy,
                 cgf.getPointerAlign());
}

mlir::Value CIRGenObjCRuntime::getProtocol(CIRGenFunction &cgf,
                                           mlir::Location loc,
                                           const ObjCProtocolDecl *proto) {
  cgm.errorNYI(proto->getSourceRange(), "ObjC protocol reference runtime lowering");
  return cgf.getBuilder().getNullPtr(cgf.convertType(cgf.getContext().VoidPtrTy),
                                     loc);
}

RValue CIRGenObjCRuntime::generateMessageSendSuper(CIRGenFunction &cgf,
                                                   const ObjCMessageExpr *expr,
                                                   ReturnValueSlot) {
  cgm.errorNYI(expr->getSourceRange(), "ObjCMessageExpr: super message");
  return cgf.getUndefRValue(expr->getType());
}

mlir::Value CIRGenObjCRuntime::emitARCRetain(CIRGenFunction &, mlir::Location loc,
                                            mlir::Value value, QualType) {
  cgm.errorNYI(loc, "ObjC ARC retain runtime lowering");
  return value;
}

void CIRGenObjCRuntime::emitARCRelease(CIRGenFunction &, mlir::Location loc,
                                       mlir::Value, QualType) {
  cgm.errorNYI(loc, "ObjC ARC release runtime lowering");
}

mlir::Value CIRGenObjCRuntime::emitARCAutorelease(CIRGenFunction &,
                                                  mlir::Location loc,
                                                  mlir::Value value,
                                                  QualType) {
  cgm.errorNYI(loc, "ObjC ARC autorelease runtime lowering");
  return value;
}

mlir::Value CIRGenObjCRuntime::emitBlockCopy(CIRGenFunction &, mlir::Location loc,
                                            mlir::Value block) {
  cgm.errorNYI(loc, "ObjC ARC Block_copy runtime lowering");
  return block;
}

void CIRGenObjCRuntime::emitBlockRelease(CIRGenFunction &, mlir::Location loc,
                                         mlir::Value) {
  cgm.errorNYI(loc, "ObjC ARC Block_release runtime lowering");
}

void CIRGenObjCRuntime::generateProtocol(const ObjCProtocolDecl *decl) {
  cgm.errorNYI(decl->getSourceRange(), "ObjC protocol metadata emission");
}

void CIRGenObjCRuntime::generateClass(const ObjCImplementationDecl *decl) {
  cgm.errorNYI(decl->getSourceRange(), "ObjC class metadata emission");
}

void CIRGenObjCRuntime::generateCategory(const ObjCCategoryImplDecl *decl) {
  cgm.errorNYI(decl->getSourceRange(), "ObjC category metadata emission");
}

namespace {
class CIRGenDarwinObjCRuntime final : public CIRGenObjCRuntime {
public:
  using CIRGenObjCRuntime::CIRGenObjCRuntime;

  mlir::Value getSelector(CIRGenFunction &cgf, mlir::Location loc,
                          Selector sel) override {
    std::string name =
        getObjCRuntimeGlobalName("OBJC_SELECTOR_REFERENCES_", sel.getAsString());
    return emitRuntimeGlobalValue(cgf, loc, name);
  }

  mlir::Value getClass(CIRGenFunction &cgf, mlir::Location loc,
                       const ObjCInterfaceDecl *iface) override {
    std::string name = getObjCRuntimeGlobalName(
        "OBJC_CLASS_REFERENCES_", iface->getObjCRuntimeNameAsString());
    return emitRuntimeGlobalValue(cgf, loc, name);
  }

  mlir::Value generateConstantString(CIRGenFunction &cgf,
                                     const ObjCStringLiteral *expr) override {
    mlir::Location loc = cgf.getLoc(expr->getSourceRange());
    std::string name = getObjCRuntimeGlobalName(
        "OBJC_STRING_LITERAL_", expr->getString()->getBytes());
    mlir::Value value = emitRuntimeGlobalValue(cgf, loc, name);
    mlir::Type resultTy = cgf.convertType(expr->getType());
    if (value.getType() != resultTy) {
      if (mlir::isa<cir::PointerType>(value.getType()) &&
          mlir::isa<cir::PointerType>(resultTy))
        value = cgf.getBuilder().createBitcast(loc, value, resultTy);
      else {
        cgm.errorNYI(expr->getSourceRange(), "ObjCStringLiteral: result cast");
        return cgf.getBuilder().getNullPtr(
            cgf.convertType(cgf.getContext().VoidPtrTy), loc);
      }
    }
    return value;
  }

  cir::FuncOp getObjCMsgSendFn(CIRGenFunction &cgf) {
    mlir::Type opaquePtrTy = cgf.convertType(cgf.getContext().VoidPtrTy);
    auto fnTy = cir::FuncType::get({opaquePtrTy, opaquePtrTy}, opaquePtrTy, true);
    return cgm.createRuntimeFunction(fnTy, "objc_msgSend");
  }

  RValue generateMessageSend(CIRGenFunction &cgf,
                             const ObjCMessageExpr *expr,
                             ReturnValueSlot) override {
    if (expr->getReceiverKind() == ObjCMessageExpr::SuperInstance ||
        expr->getReceiverKind() == ObjCMessageExpr::SuperClass)
      return generateMessageSendSuper(cgf, expr, ReturnValueSlot());

    const ObjCMethodDecl *method = expr->getMethodDecl();
    if (method && method->isDirectMethod()) {
      cgm.errorNYI(expr->getSourceRange(), "ObjCMessageExpr: direct method");
      return cgf.getUndefRValue(expr->getType());
    }

    QualType resultType = method ? method->getReturnType() : expr->getType();
    if (!resultType->isVoidType() && !resultType->isPointerType() &&
        !resultType->isObjCObjectPointerType() &&
        !resultType->isBlockPointerType()) {
      cgm.errorNYI(expr->getSourceRange(),
                   "ObjCMessageExpr: non-pointer scalar return");
      return cgf.getUndefRValue(expr->getType());
    }

    mlir::Location loc = cgf.getLoc(expr->getSourceRange());
    QualType opaquePtrASTTy = cgf.getContext().VoidPtrTy;
    mlir::Type opaquePtrTy = cgf.convertType(opaquePtrASTTy);

    mlir::Value receiver;
    switch (expr->getReceiverKind()) {
    case ObjCMessageExpr::Instance:
      receiver = cgf.emitScalarExpr(expr->getInstanceReceiver());
      break;
    case ObjCMessageExpr::Class: {
      QualType receiverType = expr->getClassReceiver();
      const ObjCInterfaceDecl *iface =
          receiverType->castAs<ObjCObjectType>()->getInterface();
      if (!iface) {
        cgm.errorNYI(expr->getSourceRange(),
                     "ObjCMessageExpr: class receiver without interface");
        return cgf.getUndefRValue(expr->getType());
      }
      receiver = getClass(cgf, loc, iface);
      break;
    }
    case ObjCMessageExpr::SuperInstance:
    case ObjCMessageExpr::SuperClass:
      llvm_unreachable("super messages handled above");
    }

    if (!receiver) {
      cgm.errorNYI(expr->getSourceRange(), "ObjCMessageExpr: receiver without value");
      receiver = cgf.getBuilder().getNullPtr(opaquePtrTy, loc);
    }
    if (receiver.getType() != opaquePtrTy)
      receiver = cgf.getBuilder().createBitcast(loc, receiver, opaquePtrTy);

    mlir::Value selector = getSelector(cgf, loc, expr->getSelector());
    if (selector.getType() != opaquePtrTy)
      selector = cgf.getBuilder().createBitcast(loc, selector, opaquePtrTy);

    CallArgList args;
    args.add(RValue::get(receiver), opaquePtrASTTy);
    args.add(RValue::get(selector), opaquePtrASTTy);
    for (const Expr *arg : expr->arguments())
      cgf.emitCallArg(args, arg, arg->getType());

    SmallVector<CanQualType, 8> argTypes;
    for (const CallArg &arg : args)
      argTypes.push_back(cgf.getContext().getCanonicalParamType(arg.ty));
    const CIRGenFunctionInfo &fnInfo = cgm.getTypes().arrangeCIRFunctionInfo(
        cgf.getContext().getCanonicalParamType(opaquePtrASTTy), argTypes,
        RequiredArgs(2));

    RValue callResult = cgf.emitCall(
        fnInfo, CIRGenCallee::forDirect(getObjCMsgSendFn(cgf)),
        ReturnValueSlot(), args, nullptr, loc);
    if (expr->getType()->isVoidType())
      return RValue::get(nullptr);

    mlir::Type resultCIRTy = cgf.convertType(expr->getType());
    mlir::Value result = callResult.getValue();
    if (result.getType() != resultCIRTy) {
      if (mlir::isa<cir::PointerType>(result.getType()) &&
          mlir::isa<cir::PointerType>(resultCIRTy))
        result = cgf.getBuilder().createBitcast(loc, result, resultCIRTy);
      else {
        cgm.errorNYI(expr->getSourceRange(), "ObjCMessageExpr: result cast");
        return cgf.getUndefRValue(expr->getType());
      }
    }
    return RValue::get(result);
  }
};
} // namespace

std::unique_ptr<CIRGenObjCRuntime>
clang::CIRGen::createCIRGenObjCRuntime(CIRGenModule &cgm) {
  return std::make_unique<CIRGenDarwinObjCRuntime>(cgm);
}
