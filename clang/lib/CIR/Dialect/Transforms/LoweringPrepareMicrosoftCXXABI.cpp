//===--- LoweringPrepareMicrosoftCXXABI.cpp - Microsoft C++ ABI prep ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "LoweringPrepareCXXABI.h"

namespace {

class LoweringPrepareMicrosoftCXXABI : public cir::LoweringPrepareCXXABI {
public:
  mlir::Value lowerDynamicCast(cir::CIRBaseBuilderTy &builder,
                               clang::ASTContext &astCtx,
                               cir::DynamicCastOp op) override {
    op.emitError("ClangIR lowering Not Yet Implemented: Microsoft C++ ABI "
                 "dynamic_cast lowering");
    return builder.getNullPtr(mlir::cast<cir::PointerType>(op.getType()),
                              op.getLoc());
  }
};

}

cir::LoweringPrepareCXXABI *
cir::LoweringPrepareCXXABI::createMicrosoftABI() {
  return new LoweringPrepareMicrosoftCXXABI();
}
