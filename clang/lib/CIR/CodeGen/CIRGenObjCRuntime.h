//===--- CIRGenObjCRuntime.h - ClangIR Objective-C runtime ABI --*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_CIR_CODEGEN_CIRGENOBJCRUNTIME_H
#define LLVM_CLANG_LIB_CIR_CODEGEN_CIRGENOBJCRUNTIME_H

#include "Address.h"
#include "CIRGenCall.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/ExprObjC.h"
#include "clang/AST/Type.h"
#include "mlir/IR/Value.h"
#include <memory>

namespace clang::CIRGen {
class CIRGenFunction;
class CIRGenModule;

/// Target/runtime-specific Objective-C lowering for CIRGen. Expression
/// visitors should delegate selectors, class refs, message sends, ARC and
/// metadata emission here instead of embedding Darwin runtime details.
class CIRGenObjCRuntime {
protected:
  CIRGenModule &cgm;

public:
  explicit CIRGenObjCRuntime(CIRGenModule &cgm) : cgm(cgm) {}
  virtual ~CIRGenObjCRuntime();

  virtual mlir::Value getSelector(CIRGenFunction &cgf, mlir::Location loc,
                                  Selector sel) = 0;
  virtual Address getAddrOfSelector(CIRGenFunction &cgf, mlir::Location loc,
                                    Selector sel);
  virtual mlir::Value getClass(CIRGenFunction &cgf, mlir::Location loc,
                               const ObjCInterfaceDecl *iface) = 0;
  virtual mlir::Value getProtocol(CIRGenFunction &cgf, mlir::Location loc,
                                  const ObjCProtocolDecl *proto);
  virtual mlir::Value generateConstantString(CIRGenFunction &cgf,
                                             const ObjCStringLiteral *expr) = 0;

  virtual RValue generateMessageSend(CIRGenFunction &cgf,
                                     const ObjCMessageExpr *expr,
                                     ReturnValueSlot returnValue) = 0;
  virtual RValue generateMessageSendSuper(CIRGenFunction &cgf,
                                          const ObjCMessageExpr *expr,
                                          ReturnValueSlot returnValue);

  virtual mlir::Value emitARCRetain(CIRGenFunction &cgf, mlir::Location loc,
                                    mlir::Value value, QualType type);
  virtual void emitARCRelease(CIRGenFunction &cgf, mlir::Location loc,
                              mlir::Value value, QualType type);
  virtual mlir::Value emitARCAutorelease(CIRGenFunction &cgf,
                                         mlir::Location loc, mlir::Value value,
                                         QualType type);
  virtual mlir::Value emitBlockCopy(CIRGenFunction &cgf, mlir::Location loc,
                                    mlir::Value block);
  virtual void emitBlockRelease(CIRGenFunction &cgf, mlir::Location loc,
                                mlir::Value block);

  virtual void generateProtocol(const ObjCProtocolDecl *decl);
  virtual void generateClass(const ObjCImplementationDecl *decl);
  virtual void generateCategory(const ObjCCategoryImplDecl *decl);
};

std::unique_ptr<CIRGenObjCRuntime> createCIRGenObjCRuntime(CIRGenModule &cgm);

} // namespace clang::CIRGen

#endif
