//==- CXXABILowering.cpp - lower C++ operations to target-specific ABI form -=//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "PassDetail.h"
#include "TargetLowering/LowerModule.h"

#include "mlir/IR/PatternMatch.h"
#include "mlir/Interfaces/DataLayoutInterfaces.h"
#include "mlir/Pass/Pass.h"
#include "mlir/Transforms/DialectConversion.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/CIR/Dialect/Builder/CIRBaseBuilder.h"
#include "clang/CIR/Dialect/IR/CIRAttrs.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "clang/CIR/Dialect/IR/CIROpsEnums.h"
#include "clang/CIR/Dialect/Passes.h"
#include "clang/CIR/MissingFeatures.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/StringExtras.h"

using namespace mlir;
using namespace cir;

namespace mlir {
#define GEN_PASS_DEF_CXXABILOWERING
#include "clang/CIR/Dialect/Passes.h.inc"
} // namespace mlir

namespace {

#define GET_ABI_LOWERING_PATTERNS
#include "clang/CIR/Dialect/IR/CIRLowering.inc"
#undef GET_ABI_LOWERING_PATTERNS

struct CXXABILoweringPass
    : public impl::CXXABILoweringBase<CXXABILoweringPass> {
  CXXABILoweringPass() = default;
  void runOnOperation() override;
};

/// A generic ABI lowering rewrite pattern. This conversion pattern matches any
/// CIR dialect operations with at least one operand or result of an
/// ABI-dependent type. This conversion pattern rewrites the matched operation
/// by replacing all its ABI-dependent operands and results with their
/// lowered counterparts.
class CIRGenericCXXABILoweringPattern : public mlir::ConversionPattern {
public:
  CIRGenericCXXABILoweringPattern(mlir::MLIRContext *context,
                                  const mlir::TypeConverter &typeConverter)
      : mlir::ConversionPattern(typeConverter, MatchAnyOpTypeTag(),
                                /*benefit=*/1, context) {}

  mlir::LogicalResult
  matchAndRewrite(mlir::Operation *op, llvm::ArrayRef<mlir::Value> operands,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    // Do not match on operations that have dedicated ABI lowering rewrite rules
    if (llvm::isa<cir::AllocaOp, cir::BaseDataMemberOp, cir::ConstantOp,
                  cir::CmpOp, cir::DerivedDataMemberOp, cir::FuncOp,
                  cir::ExtractMemberOp, cir::GetMemberOp,
                  cir::GetRuntimeMemberOp, cir::GlobalOp>(op))
      return mlir::failure();

    const mlir::TypeConverter *typeConverter = getTypeConverter();
    assert(typeConverter &&
           "CIRGenericCXXABILoweringPattern requires a type converter");
    assert(op->getNumRegions() == 0 &&
           "region-owning ops must lower ABI-dependent region signatures "
           "with a dedicated pattern");
    if (typeConverter->isLegal(op)) {
      // The operation does not have any CXXABI-dependent operands or results,
      // the match fails.
      return mlir::failure();
    }

    mlir::OperationState loweredOpState(op->getLoc(), op->getName());
    loweredOpState.addOperands(operands);
    loweredOpState.addAttributes(op->getAttrs());
    loweredOpState.addSuccessors(op->getSuccessors());

    // Lower all result types
    llvm::SmallVector<mlir::Type> loweredResultTypes;
    loweredResultTypes.reserve(op->getNumResults());
    for (mlir::Type result : op->getResultTypes())
      loweredResultTypes.push_back(typeConverter->convertType(result));
    loweredOpState.addTypes(loweredResultTypes);

    // Clone the operation with lowered operand types and result types
    mlir::Operation *loweredOp = rewriter.create(loweredOpState);

    rewriter.replaceOp(op, loweredOp);
    return mlir::success();
  }
};

static bool canLowerRecordLayout(cir::RecordType type);

static bool isLayoutSimpleForMemberPointerClass(mlir::Type ty) {
  if (mlir::isa<cir::RecordType>(ty))
    return false;
  if (auto pointer = mlir::dyn_cast<cir::PointerType>(ty))
    return !mlir::isa<cir::RecordType>(pointer.getPointee());
  if (auto array = mlir::dyn_cast<cir::ArrayType>(ty))
    return isLayoutSimpleForMemberPointerClass(array.getElementType());
  return !mlir::isa<cir::DataMemberType, cir::MethodType>(ty);
}

static bool isLayoutSimpleForMemberPointerClass(cir::RecordType type) {
  if (type.isIncomplete())
    return false;
  return llvm::all_of(type.getMembers(), [](mlir::Type member) {
    return isLayoutSimpleForMemberPointerClass(member);
  });
}

static bool canLowerDirectCXXABIType(mlir::Type ty) {
  if (auto dataMember = mlir::dyn_cast<cir::DataMemberType>(ty))
    return isLayoutSimpleForMemberPointerClass(dataMember.getClassTy()) &&
           !mlir::isa<cir::RecordType>(dataMember.getMemberTy());
  if (mlir::isa<cir::MethodType>(ty))
    return true;
  if (auto array = mlir::dyn_cast<cir::ArrayType>(ty))
    return canLowerDirectCXXABIType(array.getElementType());
  return false;
}

static bool canLowerAsRecordMember(mlir::Type ty) {
  if (auto record = mlir::dyn_cast<cir::RecordType>(ty))
    return canLowerRecordLayout(record);
  if (auto pointer = mlir::dyn_cast<cir::PointerType>(ty))
    return !mlir::isa<cir::RecordType>(pointer.getPointee());
  if (auto array = mlir::dyn_cast<cir::ArrayType>(ty))
    return canLowerAsRecordMember(array.getElementType());
  return true;
}

static bool memberNeedsCXXABILowering(mlir::Type ty) {
  if (mlir::isa<cir::MethodType>(ty))
    return false;
  if (mlir::isa<cir::DataMemberType>(ty))
    return canLowerDirectCXXABIType(ty);
  if (auto record = mlir::dyn_cast<cir::RecordType>(ty))
    return canLowerRecordLayout(record);
  if (auto array = mlir::dyn_cast<cir::ArrayType>(ty))
    return memberNeedsCXXABILowering(array.getElementType());
  return false;
}

static bool canLowerRecordLayout(cir::RecordType type) {
  if (type.isIncomplete())
    return false;

  bool needsLowering = false;
  for (mlir::Type member : type.getMembers()) {
    if (!canLowerAsRecordMember(member))
      return false;
    needsLowering |= memberNeedsCXXABILowering(member);
  }
  return needsLowering;
}

static mlir::StringAttr getLoweredRecordName(cir::RecordType type) {
  if (mlir::StringAttr name = type.getName())
    return mlir::StringAttr::get(type.getContext(),
                                 (name.getValue() + "$cxxabi").str());

  static thread_local llvm::DenseMap<mlir::Type, unsigned> anonRecordIds;
  static thread_local unsigned nextAnonRecordId;
  auto [it, inserted] = anonRecordIds.try_emplace(type, nextAnonRecordId);
  if (inserted)
    ++nextAnonRecordId;
  return mlir::StringAttr::get(type.getContext(),
                               "__cxxabi_anon_" + llvm::utostr(it->second));
}

static mlir::Attribute
lowerCXXABIAttribute(mlir::Attribute attr, mlir::Type loweredTy,
                     const mlir::TypeConverter &typeConverter,
                     const mlir::DataLayout &layout,
                     cir::LowerModule &lowerModule) {
  if (!attr)
    return {};

  if (auto dataMember = mlir::dyn_cast<cir::DataMemberAttr>(attr))
    return lowerModule.getCXXABI().lowerDataMemberConstant(
        dataMember, layout, typeConverter);

  if (auto method = mlir::dyn_cast<cir::MethodAttr>(attr))
    return lowerModule.getCXXABI().lowerMethodConstant(method, typeConverter);

  if (auto zero = mlir::dyn_cast<cir::ZeroAttr>(attr))
    return cir::ZeroAttr::get(loweredTy);

  if (auto ptr = mlir::dyn_cast<cir::ConstPtrAttr>(attr)) {
    auto loweredPtrTy = mlir::dyn_cast<cir::PointerType>(loweredTy);
    if (!loweredPtrTy)
      return {};
    return cir::ConstPtrAttr::get(loweredPtrTy, ptr.getValue());
  }

  if (auto array = mlir::dyn_cast<cir::ConstArrayAttr>(attr)) {
    auto loweredArrayTy = mlir::dyn_cast<cir::ArrayType>(loweredTy);
    if (!loweredArrayTy)
      return {};
    auto elements = mlir::dyn_cast<mlir::ArrayAttr>(array.getElts());
    if (!elements)
      return attr;

    llvm::SmallVector<mlir::Attribute> loweredElements;
    loweredElements.reserve(elements.size());
    for (mlir::Attribute element : elements) {
      mlir::Attribute loweredElement = lowerCXXABIAttribute(
          element, loweredArrayTy.getElementType(), typeConverter, layout,
          lowerModule);
      if (!loweredElement)
        return {};
      loweredElements.push_back(loweredElement);
    }

    return cir::ConstArrayAttr::get(
        loweredArrayTy,
        mlir::ArrayAttr::get(loweredTy.getContext(), loweredElements));
  }

  if (auto record = mlir::dyn_cast<cir::ConstRecordAttr>(attr)) {
    auto loweredRecordTy = mlir::dyn_cast<cir::RecordType>(loweredTy);
    if (!loweredRecordTy)
      return {};

    llvm::SmallVector<mlir::Attribute> loweredMembers;
    loweredMembers.reserve(record.getMembers().size());
    for (auto [index, member] : llvm::enumerate(record.getMembers())) {
      if (index >= loweredRecordTy.getMembers().size())
        return {};
      mlir::Attribute loweredMember = lowerCXXABIAttribute(
          member, loweredRecordTy.getMembers()[index], typeConverter, layout,
          lowerModule);
      if (!loweredMember)
        return {};
      loweredMembers.push_back(loweredMember);
    }

    return cir::ConstRecordAttr::get(
        loweredRecordTy,
        mlir::ArrayAttr::get(loweredTy.getContext(), loweredMembers));
  }

  return attr;
}

} // namespace

mlir::LogicalResult CIRAllocaOpABILowering::matchAndRewrite(
    cir::AllocaOp op, OpAdaptor adaptor,
    mlir::ConversionPatternRewriter &rewriter) const {
  mlir::Type allocaPtrTy = op.getType();
  mlir::Type allocaTy = op.getAllocaType();
  mlir::Type loweredAllocaPtrTy = getTypeConverter()->convertType(allocaPtrTy);
  mlir::Type loweredAllocaTy = getTypeConverter()->convertType(allocaTy);

  cir::AllocaOp loweredOp = cir::AllocaOp::create(
      rewriter, op.getLoc(), loweredAllocaPtrTy, loweredAllocaTy, op.getName(),
      op.getAlignmentAttr(), /*dynAllocSize=*/adaptor.getDynAllocSize());
  loweredOp.setInit(op.getInit());
  loweredOp.setConstant(op.getConstant());
  loweredOp.setAnnotationsAttr(op.getAnnotationsAttr());

  rewriter.replaceOp(op, loweredOp);
  return mlir::success();
}

mlir::LogicalResult CIRConstantOpABILowering::matchAndRewrite(
    cir::ConstantOp op, OpAdaptor adaptor,
    mlir::ConversionPatternRewriter &rewriter) const {
  mlir::Type loweredTy = getTypeConverter()->convertType(op.getType());

  if (mlir::isa<cir::DataMemberType>(op.getType())) {
    auto dataMember = mlir::cast<cir::DataMemberAttr>(op.getValue());
    mlir::DataLayout layout(op->getParentOfType<mlir::ModuleOp>());
    mlir::TypedAttr abiValue = lowerModule->getCXXABI().lowerDataMemberConstant(
        dataMember, layout, *getTypeConverter());
    rewriter.replaceOpWithNewOp<ConstantOp>(op, abiValue);
    return mlir::success();
  }

  if (mlir::isa<cir::MethodType>(op.getType())) {
    mlir::TypedAttr abiValue;
    if (auto method = mlir::dyn_cast<cir::MethodAttr>(op.getValue())) {
      abiValue =
          lowerModule->getCXXABI().lowerMethodConstant(method,
                                                       *getTypeConverter());
    } else if (mlir::isa<cir::ZeroAttr>(op.getValue())) {
      mlir::Type abiTy = getTypeConverter()->convertType(op.getType());
      abiValue = cir::ZeroAttr::get(abiTy);
    } else {
      return mlir::failure();
    }
    rewriter.replaceOpWithNewOp<ConstantOp>(op, abiValue);
    return mlir::success();
  }

  mlir::DataLayout layout(op->getParentOfType<mlir::ModuleOp>());
  mlir::Attribute loweredAttr =
      lowerCXXABIAttribute(op.getValue(), loweredTy, *getTypeConverter(),
                           layout, *lowerModule);
  if (!loweredTy || !loweredAttr || loweredTy == op.getType())
    return mlir::failure();
  rewriter.replaceOpWithNewOp<ConstantOp>(
      op, mlir::cast<mlir::TypedAttr>(loweredAttr));
  return mlir::success();
}

mlir::LogicalResult CIRCmpOpABILowering::matchAndRewrite(
    cir::CmpOp op, OpAdaptor adaptor,
    mlir::ConversionPatternRewriter &rewriter) const {
  mlir::Type type = op.getLhs().getType();
  assert((mlir::isa<cir::DataMemberType>(type)) &&
         "input to cmp in ABI lowering must be a data member");

  assert(!cir::MissingFeatures::methodType());
  mlir::Value loweredResult = lowerModule->getCXXABI().lowerDataMemberCmp(
      op, adaptor.getLhs(), adaptor.getRhs(), rewriter);

  rewriter.replaceOp(op, loweredResult);
  return mlir::success();
}

mlir::LogicalResult CIRFuncOpABILowering::matchAndRewrite(
    cir::FuncOp op, OpAdaptor adaptor,
    mlir::ConversionPatternRewriter &rewriter) const {
  cir::FuncType opFuncType = op.getFunctionType();
  mlir::TypeConverter::SignatureConversion signatureConversion(
      opFuncType.getNumInputs());

  for (const auto &[i, argType] : llvm::enumerate(opFuncType.getInputs())) {
    mlir::Type loweredArgType = getTypeConverter()->convertType(argType);
    if (!loweredArgType)
      return mlir::failure();
    signatureConversion.addInputs(i, loweredArgType);
  }

  mlir::Type loweredResultType =
      getTypeConverter()->convertType(opFuncType.getReturnType());
  if (!loweredResultType)
    return mlir::failure();

  auto loweredFuncType =
      cir::FuncType::get(signatureConversion.getConvertedTypes(),
                         loweredResultType, /*isVarArg=*/opFuncType.isVarArg());

  // Create a new cir.func operation for the CXXABI-lowered function.
  cir::FuncOp loweredFuncOp = rewriter.cloneWithoutRegions(op);
  loweredFuncOp.setFunctionType(loweredFuncType);
  rewriter.inlineRegionBefore(op.getBody(), loweredFuncOp.getBody(),
                              loweredFuncOp.end());
  if (mlir::failed(rewriter.convertRegionTypes(
          &loweredFuncOp.getBody(), *getTypeConverter(), &signatureConversion)))
    return mlir::failure();

  rewriter.eraseOp(op);
  return mlir::success();
}

mlir::LogicalResult CIRGlobalOpABILowering::matchAndRewrite(
    cir::GlobalOp op, OpAdaptor adaptor,
    mlir::ConversionPatternRewriter &rewriter) const {
  mlir::Type ty = op.getSymType();
  mlir::Type loweredTy = getTypeConverter()->convertType(ty);
  if (!loweredTy)
    return mlir::failure();

  mlir::DataLayout layout(op->getParentOfType<mlir::ModuleOp>());

  mlir::Attribute loweredInit;
  if (mlir::isa<cir::DataMemberType>(ty)) {
    cir::DataMemberAttr init =
        mlir::cast_if_present<cir::DataMemberAttr>(op.getInitialValueAttr());
    loweredInit = lowerModule->getCXXABI().lowerDataMemberConstant(
        init, layout, *getTypeConverter());
  } else if (mlir::isa<cir::MethodType>(ty)) {
    if (auto init =
            mlir::dyn_cast_if_present<cir::MethodAttr>(op.getInitialValueAttr()))
      loweredInit =
          lowerModule->getCXXABI().lowerMethodConstant(init,
                                                       *getTypeConverter());
    else if (mlir::isa_and_nonnull<cir::ZeroAttr>(op.getInitialValueAttr()))
      loweredInit = cir::ZeroAttr::get(loweredTy);
    else
      return mlir::failure();
  } else {
    loweredInit = lowerCXXABIAttribute(op.getInitialValueAttr(), loweredTy,
                                       *getTypeConverter(), layout,
                                       *lowerModule);
    if (!loweredInit && op.getInitialValueAttr())
      return mlir::failure();
  }

  auto newOp = mlir::cast<cir::GlobalOp>(rewriter.clone(*op.getOperation()));
  newOp.setInitialValueAttr(loweredInit);
  newOp.setSymType(loweredTy);
  rewriter.replaceOp(op, newOp);
  return mlir::success();
}

mlir::LogicalResult CIRBaseDataMemberOpABILowering::matchAndRewrite(
    cir::BaseDataMemberOp op, OpAdaptor adaptor,
    mlir::ConversionPatternRewriter &rewriter) const {
  mlir::Value loweredResult = lowerModule->getCXXABI().lowerBaseDataMember(
      op, adaptor.getSrc(), rewriter);
  rewriter.replaceOp(op, loweredResult);
  return mlir::success();
}

mlir::LogicalResult CIRDerivedDataMemberOpABILowering::matchAndRewrite(
    cir::DerivedDataMemberOp op, OpAdaptor adaptor,
    mlir::ConversionPatternRewriter &rewriter) const {
  mlir::Value loweredResult = lowerModule->getCXXABI().lowerDerivedDataMember(
      op, adaptor.getSrc(), rewriter);
  rewriter.replaceOp(op, loweredResult);
  return mlir::success();
}

mlir::LogicalResult CIRGetRuntimeMemberOpABILowering::matchAndRewrite(
    cir::GetRuntimeMemberOp op, OpAdaptor adaptor,
    mlir::ConversionPatternRewriter &rewriter) const {
  mlir::Type resTy = getTypeConverter()->convertType(op.getType());
  mlir::Operation *newOp = lowerModule->getCXXABI().lowerGetRuntimeMember(
      op, resTy, adaptor.getAddr(), adaptor.getMember(), rewriter);
  rewriter.replaceOp(op, newOp);
  return mlir::success();
}

class CIRRecursiveConstantOpCXXABILoweringPattern
    : public mlir::OpConversionPattern<cir::ConstantOp> {
public:
  CIRRecursiveConstantOpCXXABILoweringPattern(
      const mlir::TypeConverter &typeConverter, mlir::MLIRContext *context,
      const mlir::DataLayout &layout, cir::LowerModule &lowerModule)
      : OpConversionPattern(typeConverter, context), layout(layout),
        lowerModule(lowerModule) {}

  mlir::LogicalResult
  matchAndRewrite(cir::ConstantOp op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    mlir::Type loweredTy = getTypeConverter()->convertType(op.getType());
    if (!loweredTy || loweredTy == op.getType())
      return mlir::failure();

    mlir::Attribute loweredAttr =
        lowerCXXABIAttribute(op.getValue(), loweredTy, *getTypeConverter(),
                             layout, lowerModule);
    auto typedAttr = mlir::dyn_cast_if_present<mlir::TypedAttr>(loweredAttr);
    if (!typedAttr)
      return mlir::failure();

    rewriter.replaceOpWithNewOp<cir::ConstantOp>(op, typedAttr);
    return mlir::success();
  }

private:
  const mlir::DataLayout &layout;
  cir::LowerModule &lowerModule;
};

class CIRGetMemberOpABILoweringPattern
    : public mlir::OpConversionPattern<cir::GetMemberOp> {
public:
  using OpConversionPattern::OpConversionPattern;

  mlir::LogicalResult
  matchAndRewrite(cir::GetMemberOp op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    auto addrPtrTy = mlir::dyn_cast<cir::PointerType>(adaptor.getAddr().getType());
    if (!addrPtrTy)
      return mlir::failure();
    auto recordTy = mlir::dyn_cast<cir::RecordType>(addrPtrTy.getPointee());
    if (!recordTy || recordTy.getMembers().size() <= op.getIndex())
      return mlir::failure();

    mlir::Type memberPtrTy = cir::PointerType::get(
        op.getType().getContext(), recordTy.getMembers()[op.getIndex()],
        op.getType().getAddrSpace());
    auto memberAddr = cir::GetMemberOp::create(
        rewriter, op.getLoc(), memberPtrTy, adaptor.getAddr(), op.getName(),
        op.getIndex());

    mlir::Type convertedPtrTy = getTypeConverter()->convertType(op.getType());
    if (convertedPtrTy && convertedPtrTy != memberPtrTy) {
      rewriter.replaceOpWithNewOp<cir::CastOp>(
          op, convertedPtrTy, cir::CastKind::bitcast, memberAddr);
      return mlir::success();
    }

    rewriter.replaceOp(op, memberAddr);
    return mlir::success();
  }
};

class CIRExtractMemberOpABILoweringPattern
    : public mlir::OpConversionPattern<cir::ExtractMemberOp> {
public:
  using OpConversionPattern::OpConversionPattern;

  mlir::LogicalResult
  matchAndRewrite(cir::ExtractMemberOp op, OpAdaptor adaptor,
                  mlir::ConversionPatternRewriter &rewriter) const override {
    auto recordTy = mlir::dyn_cast<cir::RecordType>(adaptor.getRecord().getType());
    if (!recordTy || recordTy.getMembers().size() <= op.getIndex())
      return mlir::failure();

    rewriter.replaceOpWithNewOp<cir::ExtractMemberOp>(
        op, recordTy.getMembers()[op.getIndex()], adaptor.getRecord(),
        op.getIndex());
    return mlir::success();
  }
};

// Prepare the type converter for the CXXABI lowering pass.
// Even though this is a CIR-to-CIR pass, we are eliminating some CIR types.
static void prepareCXXABITypeConverter(mlir::TypeConverter &converter,
                                       mlir::DataLayout &dataLayout,
                                       cir::LowerModule &lowerModule) {
  converter.addConversion([&](mlir::Type type) -> mlir::Type { return type; });
  auto materializePointerBitcast = [](mlir::OpBuilder &builder,
                                      mlir::Type resultType,
                                      mlir::ValueRange inputs,
                                      mlir::Location loc) -> mlir::Value {
    if (inputs.size() != 1 || !mlir::isa<cir::PointerType>(resultType) ||
        !mlir::isa<cir::PointerType>(inputs.front().getType()))
      return {};
    return cir::CastOp::create(builder, loc, resultType, cir::CastKind::bitcast,
                               inputs.front());
  };
  converter.addSourceMaterialization(materializePointerBitcast);
  converter.addTargetMaterialization(materializePointerBitcast);

  // This is necessary in order to convert CIR pointer types that are pointing
  // to CIR types that we are lowering in this pass.
  converter.addConversion([&](cir::PointerType type) -> mlir::Type {
    mlir::Type loweredPointeeType = converter.convertType(type.getPointee());
    if (!loweredPointeeType)
      return {};
    return cir::PointerType::get(type.getContext(), loweredPointeeType,
                                 type.getAddrSpace());
  });
  converter.addConversion([&](cir::ArrayType type) -> mlir::Type {
    mlir::Type loweredElementType = converter.convertType(type.getElementType());
    if (!loweredElementType)
      return {};
    return cir::ArrayType::get(loweredElementType, type.getSize());
  });
  converter.addConversion([&](cir::RecordType type) -> mlir::Type {
    if (!canLowerRecordLayout(type))
      return type;

    static thread_local llvm::DenseMap<mlir::Type, cir::RecordType>
        activeNamedRecordConversions;
    mlir::StringAttr loweredName = getLoweredRecordName(type);
    auto activeIt = activeNamedRecordConversions.find(type);
    if (activeIt != activeNamedRecordConversions.end())
      return activeIt->second;

    cir::RecordType loweredType =
        cir::RecordType::get(type.getContext(), loweredName, type.getKind());
    activeNamedRecordConversions[type] = loweredType;
    llvm::scope_exit cleanup([&] { activeNamedRecordConversions.erase(type); });

    llvm::SmallVector<mlir::Type> loweredMembers;
    loweredMembers.reserve(type.getMembers().size());
    for (mlir::Type member : type.getMembers()) {
      mlir::Type loweredMember = converter.convertType(member);
      if (!loweredMember)
        return {};
      loweredMembers.push_back(loweredMember);
    }

    if (loweredType.isIncomplete())
      loweredType.complete(loweredMembers, type.getPacked(),
                           type.getPadded());
    return loweredType;
  });
  converter.addConversion([&](cir::DataMemberType type) -> mlir::Type {
    mlir::Type abiType =
        lowerModule.getCXXABI().lowerDataMemberType(type, converter);
    return converter.convertType(abiType);
  });
  converter.addConversion([&](cir::MethodType type) -> mlir::Type {
    mlir::Type abiType =
        lowerModule.getCXXABI().lowerMethodType(type, converter);
    return converter.convertType(abiType);
  });
  // This is necessary in order to convert CIR function types that have argument
  // or return types that use CIR types that we are lowering in this pass.
  converter.addConversion([&](cir::FuncType type) -> mlir::Type {
    llvm::SmallVector<mlir::Type> loweredInputTypes;
    loweredInputTypes.reserve(type.getNumInputs());
    if (mlir::failed(
            converter.convertTypes(type.getInputs(), loweredInputTypes)))
      return {};

    mlir::Type loweredReturnType = converter.convertType(type.getReturnType());
    if (!loweredReturnType)
      return {};

    return cir::FuncType::get(loweredInputTypes, loweredReturnType,
                              /*isVarArg=*/type.getVarArg());
  });
}

static void
populateCXXABIConversionTarget(mlir::ConversionTarget &target,
                               const mlir::TypeConverter &typeConverter) {
  target.addLegalOp<mlir::ModuleOp>();

  // The ABI lowering pass is interested in CIR operations with operands or
  // results of CXXABI-dependent types, or CIR operations with regions whose
  // block arguments are of CXXABI-dependent types.
  target.addDynamicallyLegalDialect<cir::CIRDialect>(
      [&typeConverter](mlir::Operation *op) {
        return typeConverter.isLegal(op);
      });

  // Some CIR ops needs special checking for legality
  target.addDynamicallyLegalOp<cir::FuncOp>([&typeConverter](cir::FuncOp op) {
    return typeConverter.isLegal(op.getFunctionType());
  });
  target.addDynamicallyLegalOp<cir::GlobalOp>(
      [&typeConverter](cir::GlobalOp op) {
        return typeConverter.isLegal(op.getSymType());
      });
  target.addDynamicallyLegalOp<cir::GetMemberOp>([](cir::GetMemberOp op) {
    auto addrPtrTy = mlir::dyn_cast<cir::PointerType>(op.getAddr().getType());
    if (!addrPtrTy)
      return false;
    auto recordTy = mlir::dyn_cast<cir::RecordType>(addrPtrTy.getPointee());
    return recordTy && recordTy.getMembers().size() > op.getIndex() &&
           recordTy.getMembers()[op.getIndex()] == op.getType().getPointee();
  });
  target.addDynamicallyLegalOp<cir::ExtractMemberOp>(
      [](cir::ExtractMemberOp op) {
        auto recordTy = mlir::dyn_cast<cir::RecordType>(op.getRecord().getType());
        return recordTy && recordTy.getMembers().size() > op.getIndex() &&
               recordTy.getMembers()[op.getIndex()] == op.getType();
      });
}

//===----------------------------------------------------------------------===//
// The Pass
//===----------------------------------------------------------------------===//

void CXXABILoweringPass::runOnOperation() {
  auto module = mlir::cast<mlir::ModuleOp>(getOperation());
  mlir::MLIRContext *ctx = module.getContext();

  // If the triple is not present, e.g. CIR modules parsed from text, we
  // cannot init LowerModule properly.
  assert(!cir::MissingFeatures::makeTripleAlwaysPresent());
  // If no target triple is available, skip the ABI lowering pass.
  if (!module->hasAttr(cir::CIRDialect::getTripleAttrName()))
    return;

  mlir::PatternRewriter rewriter(ctx);
  std::unique_ptr<cir::LowerModule> lowerModule =
      cir::createLowerModule(module, rewriter);

  mlir::DataLayout dataLayout(module);
  mlir::TypeConverter typeConverter;
  prepareCXXABITypeConverter(typeConverter, dataLayout, *lowerModule);

  mlir::RewritePatternSet patterns(ctx);
  patterns.add<CIRGenericCXXABILoweringPattern>(patterns.getContext(),
                                                typeConverter);
  patterns.add<CIRGetMemberOpABILoweringPattern,
               CIRExtractMemberOpABILoweringPattern>(typeConverter,
                                                     patterns.getContext());
  patterns.add<CIRRecursiveConstantOpCXXABILoweringPattern>(
      typeConverter, patterns.getContext(), dataLayout, *lowerModule);
  patterns.add<
#define GET_ABI_LOWERING_PATTERNS_LIST
#include "clang/CIR/Dialect/IR/CIRLowering.inc"
#undef GET_ABI_LOWERING_PATTERNS_LIST
      >(patterns.getContext(), typeConverter, dataLayout, *lowerModule);

  mlir::ConversionTarget target(*ctx);
  populateCXXABIConversionTarget(target, typeConverter);

  if (failed(mlir::applyPartialConversion(module, target, std::move(patterns))))
    signalPassFailure();
}

std::unique_ptr<Pass> mlir::createCXXABILoweringPass() {
  return std::make_unique<CXXABILoweringPass>();
}
