//===--- LowerMicrosoftCXXABI.cpp - Lower Microsoft C++ ABI CIR -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM
// Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "CIRCXXABI.h"
#include "LowerModule.h"

namespace cir {

namespace {

class LowerMicrosoftCXXABI : public CIRCXXABI {
public:
  LowerMicrosoftCXXABI(LowerModule &lm) : CIRCXXABI(lm) {}

  mlir::Type
  lowerDataMemberType(cir::DataMemberType type,
                      const mlir::TypeConverter &typeConverter) const override;
  mlir::Type
  lowerMethodType(cir::MethodType type,
                  const mlir::TypeConverter &typeConverter) const override;
  mlir::TypedAttr lowerDataMemberConstant(
      cir::DataMemberAttr attr, const mlir::DataLayout &layout,
      const mlir::TypeConverter &typeConverter) const override;
  mlir::TypedAttr
  lowerMethodConstant(cir::MethodAttr attr,
                      const mlir::TypeConverter &typeConverter) const override;
  mlir::Operation *
  lowerGetRuntimeMember(cir::GetRuntimeMemberOp op, mlir::Type loweredResultTy,
                        mlir::Value loweredAddr, mlir::Value loweredMember,
                        mlir::OpBuilder &builder) const override;
  mlir::Value lowerBaseDataMember(cir::BaseDataMemberOp op,
                                  mlir::Value loweredSrc,
                                  mlir::OpBuilder &builder) const override;
  mlir::Value lowerDerivedDataMember(cir::DerivedDataMemberOp op,
                                     mlir::Value loweredSrc,
                                     mlir::OpBuilder &builder) const override;
  mlir::Value lowerDataMemberCmp(cir::CmpOp op, mlir::Value loweredLhs,
                                 mlir::Value loweredRhs,
                                 mlir::OpBuilder &builder) const override;
};

}

std::unique_ptr<CIRCXXABI> createMicrosoftCXXABI(LowerModule &lm) {
  return std::make_unique<LowerMicrosoftCXXABI>(lm);
}

static cir::IntType getMSIntTy(LowerModule &lm) {
  const clang::TargetInfo &target = lm.getTarget();
  return cir::IntType::get(lm.getMLIRContext(), target.getIntWidth(),
                           true);
}

mlir::Type LowerMicrosoftCXXABI::lowerDataMemberType(
    cir::DataMemberType type, const mlir::TypeConverter &typeConverter) const {
  return getMSIntTy(lm);
}

mlir::Type LowerMicrosoftCXXABI::lowerMethodType(
    cir::MethodType type, const mlir::TypeConverter &typeConverter) const {
  return cir::PointerType::get(cir::VoidType::get(type.getContext()));
}

mlir::TypedAttr LowerMicrosoftCXXABI::lowerDataMemberConstant(
    cir::DataMemberAttr attr, const mlir::DataLayout &layout,
    const mlir::TypeConverter &typeConverter) const {
  mlir::Type abiTy = lowerDataMemberType(attr.getType(), typeConverter);
  return cir::PoisonAttr::get(abiTy);
}

mlir::TypedAttr LowerMicrosoftCXXABI::lowerMethodConstant(
    cir::MethodAttr attr, const mlir::TypeConverter &typeConverter) const {
  mlir::Type abiTy = lowerMethodType(attr.getType(), typeConverter);
  return cir::PoisonAttr::get(abiTy);
}

mlir::Operation *LowerMicrosoftCXXABI::lowerGetRuntimeMember(
    cir::GetRuntimeMemberOp op, mlir::Type loweredResultTy,
    mlir::Value loweredAddr, mlir::Value loweredMember,
    mlir::OpBuilder &builder) const {
  op.emitError("ClangIR lowering Not Yet Implemented: Microsoft C++ ABI "
               "pointer-to-member access");
  return cir::ConstantOp::create(builder, op.getLoc(),
                                 cir::PoisonAttr::get(loweredResultTy));
}

mlir::Value
LowerMicrosoftCXXABI::lowerBaseDataMember(cir::BaseDataMemberOp op,
                                          mlir::Value loweredSrc,
                                          mlir::OpBuilder &builder) const {
  op.emitError("ClangIR lowering Not Yet Implemented: Microsoft C++ ABI "
               "base pointer-to-member conversion");
  return cir::ConstantOp::create(builder, op.getLoc(),
                                 cir::PoisonAttr::get(loweredSrc.getType()));
}

mlir::Value
LowerMicrosoftCXXABI::lowerDerivedDataMember(cir::DerivedDataMemberOp op,
                                             mlir::Value loweredSrc,
                                             mlir::OpBuilder &builder) const {
  op.emitError("ClangIR lowering Not Yet Implemented: Microsoft C++ ABI "
               "derived pointer-to-member conversion");
  return cir::ConstantOp::create(builder, op.getLoc(),
                                 cir::PoisonAttr::get(loweredSrc.getType()));
}

mlir::Value LowerMicrosoftCXXABI::lowerDataMemberCmp(
    cir::CmpOp op, mlir::Value loweredLhs, mlir::Value loweredRhs,
    mlir::OpBuilder &builder) const {
  op.emitError("ClangIR lowering Not Yet Implemented: Microsoft C++ ABI "
               "pointer-to-member comparison");
  return cir::ConstantOp::create(
      builder, op.getLoc(),
      cir::PoisonAttr::get(cir::BoolType::get(op.getContext())));
}

}
