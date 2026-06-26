//===--- CIRGenBlockRuntime.h - ClangIR Blocks runtime ABI -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_LIB_CIR_CODEGEN_CIRGENBLOCKRUNTIME_H
#define LLVM_CLANG_LIB_CIR_CODEGEN_CIRGENBLOCKRUNTIME_H

#include "Address.h"
#include "CIRGenCall.h"
#include "CIRGenValue.h"
#include "clang/AST/CharUnits.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/Type.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include <memory>

namespace clang::CIRGen {
class CIRGenFunction;
class CIRGenModule;

enum class CIRGenBlockCaptureKind : uint8_t {
  Constant,
  ScalarOrPointer,
  CXXThis,
  ByRef,
  ObjCStrong,
  ObjCWeak,
  BlockObject,
  CXXNonTrivial
};

struct CIRGenBlockCaptureInfo {
  const VarDecl *variable = nullptr;
  CIRGenBlockCaptureKind kind = CIRGenBlockCaptureKind::ScalarOrPointer;
  QualType fieldType;
  mlir::Type cirType;
  CharUnits alignment;
  unsigned fieldIndex = 0;

  bool isByRef() const { return kind == CIRGenBlockCaptureKind::ByRef; }
  bool needsCopyDispose() const {
    return kind == CIRGenBlockCaptureKind::ObjCStrong ||
           kind == CIRGenBlockCaptureKind::ObjCWeak ||
           kind == CIRGenBlockCaptureKind::BlockObject ||
           kind == CIRGenBlockCaptureKind::CXXNonTrivial || isByRef();
  }
};

/// Per-block ABI layout and capture map. Runtime policy owns this; expression
/// visitors only ask for a block pointer value or captured-variable address.
struct CIRGenBlockInfo {
  const BlockExpr *blockExpr = nullptr;
  const BlockDecl *blockDecl = nullptr;
  std::string invokeName;
  cir::RecordType literalType;
  cir::RecordType descriptorType;
  CharUnits literalAlign;
  CharUnits literalSize;
  bool canBeGlobal = false;
  bool noEscape = false;
  bool needsCopyDispose = false;
  bool hasCXXObject = false;
  bool hasSignature = false;
  bool usesStret = false;
  bool hasCapturedVariableLayout = false;
  unsigned cxxThisFieldIndex = 0;
  llvm::SmallVector<CIRGenBlockCaptureInfo, 8> captures;
  llvm::DenseMap<const VarDecl *, unsigned> captureIndex;

  const CIRGenBlockCaptureInfo *getCapture(const VarDecl *var) const {
    auto it = captureIndex.find(var);
    if (it == captureIndex.end())
      return nullptr;
    return &captures[it->second];
  }
};

class CIRGenBlockRuntime {
protected:
  CIRGenModule &cgm;

public:
  explicit CIRGenBlockRuntime(CIRGenModule &cgm) : cgm(cgm) {}
  virtual ~CIRGenBlockRuntime();

  virtual mlir::Value emitBlockLiteral(CIRGenFunction &cgf,
                                       const BlockExpr *expr) = 0;
  virtual RValue emitBlockCallExpr(CIRGenFunction &cgf, const CallExpr *expr,
                                   ReturnValueSlot returnValue) = 0;
  virtual Address getAddrOfBlockDecl(CIRGenFunction &cgf,
                                     const VarDecl *variable) = 0;

  virtual CIRGenBlockInfo computeBlockInfo(CIRGenFunction &cgf,
                                           const BlockExpr *expr) = 0;
};

std::unique_ptr<CIRGenBlockRuntime> createCIRGenBlockRuntime(CIRGenModule &cgm);

} // namespace clang::CIRGen

#endif
