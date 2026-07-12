//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This contains code to emit Expr nodes as CIR code.
//
//===----------------------------------------------------------------------===//

#include "Address.h"
#include "CIRGenBlockRuntime.h"
#include "CIRGenConstantEmitter.h"
#include "CIRGenCXXABI.h"
#include "CIRGenFunction.h"
#include "CIRGenModule.h"
#include "CIRGenObjCRuntime.h"
#include "CIRGenValue.h"
#include "TargetInfo.h"
#include "mlir/IR/BuiltinAttributes.h"
#include "mlir/IR/Value.h"
#include "clang/AST/APValue.h"
#include "clang/AST/Attr.h"
#include "clang/AST/CharUnits.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/ExprObjC.h"
#include "clang/Basic/AddressSpaces.h"
#include "clang/Basic/TargetInfo.h"
#include "clang/CIR/Dialect/IR/CIRAttrs.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "clang/CIR/Dialect/IR/CIRTypes.h"
#include "clang/CIR/MissingFeatures.h"
#include "llvm/Support/raw_ostream.h"
#include <optional>
#include <string>

using namespace clang;
using namespace clang::CIRGen;
using namespace cir;

/// Get the address of a field within a record. The resulting address doesn't
/// necessarily have the right type.
Address CIRGenFunction::emitAddrOfFieldStorage(Address base,
                                               const FieldDecl *field,
                                               llvm::StringRef fieldName,
                                               unsigned fieldIndex) {
  mlir::Location loc = getLoc(field->getLocation());
  if (!base.isValid()) {
    cgm.errorNYI(field->getSourceRange(),
                 "emitAddrOfFieldStorage: unavailable base address");
    return Address::invalid();
  }

  // Empty fields have no storage in the record layout (they were skipped by
  // CIRRecordLowering::accumulateFields). Their address is computed directly
  // from the field's byte offset, mirroring classic CodeGen's
  // emitAddrOfZeroSizeField.
  if (isEmptyFieldForLayout(getContext(), field)) {
    CharUnits offset = getContext().toCharUnitsFromBits(
        getContext().getFieldOffset(field));
    // The result must have the field's pointer type (CIR pointers are typed),
    // unlike classic CodeGen where the opaque base pointer can be returned
    // as-is. Bitcast the (possibly byte-adjusted) base to the field type.
    mlir::Type fieldType = convertType(field->getType());
    mlir::Type fieldPtrType = cir::PointerType::get(fieldType);

    if (offset.isZero())
      return Address(builder.createBitcast(base.getPointer(), fieldPtrType),
                     fieldType, base.getAlignment());

    mlir::Type charPtrType = cgm.uInt8PtrTy;
    mlir::Value charPtr = builder.createBitcast(base.getPointer(), charPtrType);
    mlir::Value byteOffset =
        builder.getConstInt(loc, cgm.ptrDiffTy, offset.getQuantity());
    mlir::Value adjusted =
        cir::PtrStrideOp::create(builder, loc, charPtrType, charPtr, byteOffset);
    mlir::Value result = builder.createBitcast(adjusted, fieldPtrType);
    return Address(result, fieldType,
                   base.getAlignment().alignmentAtOffset(offset));
  }

  mlir::Type fieldType = convertType(field->getType());
  mlir::Type memberType = fieldType;
  auto baseRecordTy = mlir::dyn_cast<cir::RecordType>(base.getElementType());
  if (!baseRecordTy) {
    cgm.errorNYI(field->getSourceRange(),
                 "emitAddrOfFieldStorage: non-record base address");
    return Address::invalid();
  }
  if (fieldIndex >= baseRecordTy.getMembers().size()) {
    cgm.errorNYI(field->getSourceRange(),
                 "emitAddrOfFieldStorage: field index out of bounds");
    return Address::invalid();
  }
  if (!field->getParent()->isUnion()) {
    memberType = baseRecordTy.getMembers()[fieldIndex];
  }
  auto memberPtr = cir::PointerType::get(memberType);
  // For most cases fieldName is the same as field->getName() but for lambdas,
  // which do not currently carry the name, so it can be passed down from the
  // CaptureStmt.
  cir::GetMemberOp memberAddr = builder.createGetMember(
      loc, memberPtr, base.getPointer(), fieldName, fieldIndex);

  // Retrieve layout information, compute alignment and return the final
  // address.
  const RecordDecl *rec = field->getParent();
  const CIRGenRecordLayout &layout = cgm.getTypes().getCIRGenRecordLayout(rec);
  unsigned idx = layout.getCIRFieldNo(field);
  if (idx >= baseRecordTy.getMembers().size()) {
    cgm.errorNYI(field->getSourceRange(),
                 "emitAddrOfFieldStorage: layout index out of bounds");
    return Address::invalid();
  }
  CharUnits offset = CharUnits::fromQuantity(
      baseRecordTy.getElementOffset(cgm.getDataLayout().layout, idx));
  Address addr(memberAddr, memberType,
               base.getAlignment().alignmentAtOffset(offset));
  return builder.createElementBitCast(loc, addr, fieldType);
}

/// Given an expression of pointer type, try to
/// derive a more accurate bound on the alignment of the pointer.
Address CIRGenFunction::emitPointerWithAlignment(const Expr *expr,
                                                 LValueBaseInfo *baseInfo) {
  // We allow this with ObjC object pointers because of fragile ABIs.
  assert(expr->getType()->isPointerType() ||
         expr->getType()->isObjCObjectPointerType());
  expr = expr->IgnoreParens();

  // Casts:
  if (auto const *ce = dyn_cast<CastExpr>(expr)) {
    if (const auto *ece = dyn_cast<ExplicitCastExpr>(ce))
      cgm.emitExplicitCastExprType(ece);

    switch (ce->getCastKind()) {
    // Non-converting casts (but not C's implicit conversion from void*).
    case CK_BitCast:
    case CK_NoOp:
    case CK_AddressSpaceConversion: {
      if (const auto *ptrTy =
              ce->getSubExpr()->getType()->getAs<PointerType>()) {
        if (ptrTy->getPointeeType()->isVoidType())
          break;

        LValueBaseInfo innerBaseInfo;
        assert(!cir::MissingFeatures::opTBAA());
        Address addr =
            emitPointerWithAlignment(ce->getSubExpr(), &innerBaseInfo);
        if (baseInfo)
          *baseInfo = innerBaseInfo;

        if (isa<ExplicitCastExpr>(ce)) {
          LValueBaseInfo targetTypeBaseInfo;

          const QualType pointeeType = expr->getType()->getPointeeType();
          const CharUnits align =
              cgm.getNaturalTypeAlignment(pointeeType, &targetTypeBaseInfo);

          // If the source l-value is opaque, honor the alignment of the
          // casted-to type.
          if (innerBaseInfo.getAlignmentSource() != AlignmentSource::Decl) {
            if (baseInfo)
              baseInfo->mergeForCast(targetTypeBaseInfo);
            addr = Address(addr.getPointer(), addr.getElementType(), align);
          }
        }

        assert(!cir::MissingFeatures::sanitizers());

        const mlir::Type eltTy =
            convertTypeForMem(expr->getType()->getPointeeType());
        addr = getBuilder().createElementBitCast(getLoc(expr->getSourceRange()),
                                                 addr, eltTy);
        assert(!cir::MissingFeatures::addressSpace());

        return addr;
      }
      break;
    }

    // Array-to-pointer decay. TODO(cir): BaseInfo and TBAAInfo.
    case CK_ArrayToPointerDecay:
      return emitArrayToPointerDecay(ce->getSubExpr(), baseInfo);

    case CK_UncheckedDerivedToBase:
    case CK_DerivedToBase: {
      assert(!cir::MissingFeatures::opTBAA());
      assert(!cir::MissingFeatures::addressIsKnownNonNull());
      Address addr = emitPointerWithAlignment(ce->getSubExpr(), baseInfo);
      const CXXRecordDecl *derived =
          ce->getSubExpr()->getType()->getPointeeCXXRecordDecl();
      return getAddressOfBaseClass(addr, derived, ce->path(),
                                   shouldNullCheckClassCastValue(ce),
                                   ce->getExprLoc());
    }

    case CK_LValueBitCast: {
      LValue lv = emitCastLValue(ce);
      if (baseInfo)
        *baseInfo = lv.getBaseInfo();
      return lv.getAddress();
    }

    case CK_AnyPointerToBlockPointerCast:
    case CK_BaseToDerived:
    case CK_BaseToDerivedMemberPointer:
    case CK_BlockPointerToObjCPointerCast:
    case CK_BuiltinFnToFnPtr:
    case CK_CPointerToObjCPointerCast:
    case CK_DerivedToBaseMemberPointer:
    case CK_Dynamic:
    case CK_FunctionToPointerDecay:
    case CK_IntegralToPointer:
    case CK_AtomicToNonAtomic:
    case CK_LValueToRValue:
    case CK_LValueToRValueBitCast:
    case CK_NullToMemberPointer:
    case CK_NullToPointer:
    case CK_ReinterpretMemberPointer:
    case CK_UserDefinedConversion:
      // Common pointer conversions, nothing to do here.
      // TODO: Is there any reason to treat base-to-derived conversions
      // specially?
      break;

    case CK_ARCConsumeObject:
    case CK_ARCExtendBlockObject:
    case CK_ARCProduceObject:
    case CK_ARCReclaimReturnedObject:
    case CK_BooleanToSignedIntegral:
    case CK_ConstructorConversion:
    case CK_CopyAndAutoreleaseBlockObject:
    case CK_Dependent:
    case CK_FixedPointCast:
    case CK_FixedPointToBoolean:
    case CK_FixedPointToFloating:
    case CK_FixedPointToIntegral:
    case CK_FloatingCast:
    case CK_FloatingComplexCast:
    case CK_FloatingComplexToBoolean:
    case CK_FloatingComplexToIntegralComplex:
    case CK_FloatingComplexToReal:
    case CK_FloatingRealToComplex:
    case CK_FloatingToBoolean:
    case CK_FloatingToFixedPoint:
    case CK_FloatingToIntegral:
    case CK_HLSLAggregateSplatCast:
    case CK_HLSLArrayRValue:
    case CK_HLSLElementwiseCast:
    case CK_HLSLVectorTruncation:
    case CK_HLSLMatrixTruncation:
    case CK_IntToOCLSampler:
    case CK_IntegralCast:
    case CK_IntegralComplexCast:
    case CK_IntegralComplexToBoolean:
    case CK_IntegralComplexToFloatingComplex:
    case CK_IntegralComplexToReal:
    case CK_IntegralRealToComplex:
    case CK_IntegralToBoolean:
    case CK_IntegralToFixedPoint:
    case CK_IntegralToFloating:
    case CK_MatrixCast:
    case CK_MemberPointerToBoolean:
    case CK_NonAtomicToAtomic:
    case CK_ObjCObjectLValueCast:
    case CK_PointerToBoolean:
    case CK_PointerToIntegral:
    case CK_ToUnion:
    case CK_ToVoid:
    case CK_VectorSplat:
    case CK_ZeroToOCLOpaqueType:
      cgm.errorNYI(expr->getSourceRange(),
                   std::string("emitPointerWithAlignment: unexpected cast ") +
                       ce->getCastKindName());
      break;
    }
  }

  // Unary &
  if (const UnaryOperator *uo = dyn_cast<UnaryOperator>(expr)) {
    // TODO(cir): maybe we should use cir.unary for pointers here instead.
    if (uo->getOpcode() == UO_AddrOf) {
      LValue lv = emitLValue(uo->getSubExpr());
      if (baseInfo)
        *baseInfo = lv.getBaseInfo();
      assert(!cir::MissingFeatures::opTBAA());
      return lv.getAddress();
    }
  }

  // std::addressof and variants.
  if (auto const *call = dyn_cast<CallExpr>(expr)) {
    switch (call->getBuiltinCallee()) {
    default:
      break;
    case Builtin::BIaddressof:
    case Builtin::BI__addressof:
    case Builtin::BI__builtin_addressof: {
      LValue lv = emitLValue(call->getArg(0));
      if (baseInfo)
        *baseInfo = lv.getBaseInfo();
      assert(!cir::MissingFeatures::opTBAA());
      return lv.getAddress();
    }
    }
  }

  // Otherwise, use the alignment of the type.
  mlir::Value pointer = emitScalarExpr(expr);
  if (!pointer)
    pointer = createDummyValue(getLoc(expr->getSourceRange()), expr->getType());
  return makeNaturalAddressForPointer(pointer, expr->getType()->getPointeeType(),
                                      CharUnits(), /*forPointeeType=*/true,
                                      baseInfo);
}

void CIRGenFunction::emitStoreThroughLValue(RValue src, LValue dst,
                                            bool isInit) {
  if (!dst.isSimple()) {
    if (dst.isVectorElt()) {
      // Read/modify/write the vector, inserting the new element
      const mlir::Location loc = dst.getVectorPointer().getLoc();
      const mlir::Value vector =
          builder.createLoad(loc, dst.getVectorAddress());
      const mlir::Value newVector = cir::VecInsertOp::create(
          builder, loc, vector, src.getValue(), dst.getVectorIdx());
      builder.createStore(loc, newVector, dst.getVectorAddress());
      return;
    }

    assert(dst.isBitField() && "Unknown LValue type");
    emitStoreThroughBitfieldLValue(src, dst);
    return;

    cgm.errorNYI(dst.getPointer().getLoc(),
                 "emitStoreThroughLValue: non-simple lvalue");
    return;
  }

  assert(!cir::MissingFeatures::opLoadStoreObjC());

  assert(src.isScalar() && "Can't emit an aggregate store with this method");
  emitStoreOfScalar(src.getValue(), dst, isInit);
}

static LValue emitGlobalVarDeclLValue(CIRGenFunction &cgf, const Expr *e,
                                      const VarDecl *vd) {
  QualType t = e->getType();

  // If it's thread_local, emit a call to its wrapper function instead.
  if (vd->getTLSKind() == VarDecl::TLS_Dynamic &&
      cgf.cgm.getCXXABI().usesThreadWrapperFunction(vd))
    return cgf.cgm.getCXXABI().emitThreadLocalVarDeclLValue(cgf, vd, t);

  // Check if the variable is marked as declare target with link clause in
  // device codegen.
  if (cgf.getLangOpts().OpenMP)
    cgf.cgm.errorNYI(e->getSourceRange(), "emitGlobalVarDeclLValue: OpenMP");

  // Traditional LLVM codegen handles thread local separately, CIR handles
  // as part of getAddrOfGlobalVar.
  mlir::Value v = cgf.cgm.getAddrOfGlobalVar(vd);

  assert(!cir::MissingFeatures::addressSpace());
  mlir::Type realVarTy = cgf.convertTypeForMem(vd->getType());
  cir::PointerType realPtrTy = cgf.getBuilder().getPointerTo(realVarTy);
  if (realPtrTy != v.getType())
    v = cgf.getBuilder().createBitcast(v.getLoc(), v, realPtrTy);

  CharUnits alignment = cgf.getContext().getDeclAlign(vd);
  Address addr(v, realVarTy, alignment);
  LValue lv;
  if (vd->getType()->isReferenceType())
    cgf.cgm.errorNYI(e->getSourceRange(),
                     "emitGlobalVarDeclLValue: reference type");
  else
    lv = cgf.makeAddrLValue(addr, t, AlignmentSource::Decl);
  assert(!cir::MissingFeatures::setObjCGCLValueClass());
  return lv;
}

static bool canStoreThroughRepresentationalRecordBitcast(CIRGenModule &cgm,
                                                         mlir::Type valueType,
                                                         mlir::Type destType) {
  if (!mlir::isa<cir::RecordType>(valueType) ||
      !mlir::isa<cir::RecordType>(destType))
    return false;
  if (!cir::isSized(valueType) || !cir::isSized(destType))
    return false;

  cir::CIRDataLayout layout = cgm.getDataLayout();
  llvm::TypeSize valueSize = layout.getTypeAllocSize(valueType);
  llvm::TypeSize destSize = layout.getTypeAllocSize(destType);
  if (valueSize.isScalable() || destSize.isScalable() || valueSize != destSize)
    return false;

  return layout.getABITypeAlign(valueType) == layout.getABITypeAlign(destType);
}

static bool isBytePaddingType(mlir::Type type) {
  if (auto intType = mlir::dyn_cast<cir::IntType>(type))
    return !intType.isSigned() && intType.getWidth() == 8;
  if (auto arrayType = mlir::dyn_cast<cir::ArrayType>(type))
    return isBytePaddingType(arrayType.getElementType());
  return false;
}

static bool areAllZeroPaddingTypes(cir::RecordType recordType,
                                   unsigned exceptIndex) {
  for (unsigned index = 0; index < recordType.getNumElements(); ++index) {
    if (index == exceptIndex)
      continue;
    if (!isBytePaddingType(recordType.getElementType(index)))
      return false;
  }
  return true;
}

static std::string getTypeString(mlir::Type type) {
  std::string result;
  llvm::raw_string_ostream os(result);
  type.print(os);
  os.flush();
  return result;
}

static std::optional<mlir::TypedAttr>
retargetStoreConstant(CIRGenModule &cgm, mlir::TypedAttr attr,
                      mlir::Type destType) {
  if (attr.getType() == destType)
    return attr;

  CIRGenBuilderTy &builder = cgm.getBuilder();
  if (builder.isNullValue(attr))
    return builder.getZeroInitAttr(destType);

  if (auto intAttr = mlir::dyn_cast<cir::IntAttr>(attr)) {
    if (auto destInt = mlir::dyn_cast<cir::IntType>(destType);
        destInt && intAttr.getBitWidth() == destInt.getWidth())
      return cir::IntAttr::get(destType, intAttr.getValue());
  }

  auto destRecord = mlir::dyn_cast<cir::RecordType>(destType);
  auto sourceRecordAttr = mlir::dyn_cast<cir::ConstRecordAttr>(attr);
  if (!destRecord || !sourceRecordAttr)
    return std::nullopt;

  auto buildRecord = [&](llvm::ArrayRef<mlir::Attribute> members) {
    return cir::ConstRecordAttr::get(
        destRecord, mlir::ArrayAttr::get(builder.getContext(), members));
  };

  for (unsigned index = 0; index < destRecord.getNumElements(); ++index) {
    mlir::Type destMemberType = destRecord.getElementType(index);
    if (!mlir::isa<cir::RecordType>(destMemberType) ||
        !areAllZeroPaddingTypes(destRecord, index))
      continue;

    std::optional<mlir::TypedAttr> nested =
        retargetStoreConstant(cgm, attr, destMemberType);
    if (!nested)
      continue;

    SmallVector<mlir::Attribute, 8> members;
    members.reserve(destRecord.getNumElements());
    for (unsigned memberIndex = 0; memberIndex < destRecord.getNumElements();
         ++memberIndex) {
      if (memberIndex == index)
        members.push_back(*nested);
      else
        members.push_back(
            builder.getZeroInitAttr(destRecord.getElementType(memberIndex)));
    }
    return buildRecord(members);
  }

  SmallVector<mlir::Attribute, 8> members;
  members.reserve(destRecord.getNumElements());
  unsigned sourceIndex = 0;
  for (unsigned destIndex = 0; destIndex < destRecord.getNumElements();
       ++destIndex) {
    mlir::Type destMemberType = destRecord.getElementType(destIndex);
    if (destRecord.getPadded() && isBytePaddingType(destMemberType) &&
        sourceIndex < sourceRecordAttr.getMembers().size()) {
      auto sourceMember = mlir::dyn_cast<mlir::TypedAttr>(
          sourceRecordAttr.getMembers()[sourceIndex]);
      bool sourceFitsLaterMember = false;
      if (sourceMember && sourceMember.getType() != destMemberType) {
        for (unsigned next = destIndex + 1; next < destRecord.getNumElements();
             ++next) {
          mlir::Type nextType = destRecord.getElementType(next);
          if (destRecord.getPadded() && isBytePaddingType(nextType))
            continue;
          sourceFitsLaterMember =
              retargetStoreConstant(cgm, sourceMember, nextType).has_value();
          break;
        }
      }
      if (sourceFitsLaterMember) {
        members.push_back(builder.getZeroInitAttr(destMemberType));
        continue;
      }
    }

    if (sourceIndex < sourceRecordAttr.getMembers().size()) {
      auto sourceMember = mlir::dyn_cast<mlir::TypedAttr>(
          sourceRecordAttr.getMembers()[sourceIndex]);
      if (sourceMember) {
        std::optional<mlir::TypedAttr> retargeted =
            retargetStoreConstant(cgm, sourceMember, destMemberType);
        if (retargeted) {
          members.push_back(*retargeted);
          ++sourceIndex;
          continue;
        }
      }
    }

    if (!isBytePaddingType(destMemberType))
      return std::nullopt;
    members.push_back(builder.getZeroInitAttr(destMemberType));
  }

  if (sourceIndex != sourceRecordAttr.getMembers().size())
    return std::nullopt;
  return buildRecord(members);
}

static mlir::Value retargetStoreConstantValue(CIRGenFunction &cgf,
                                              mlir::Location loc,
                                              mlir::Value value,
                                              mlir::Type destType) {
  cir::ConstantOp constOp = value.getDefiningOp<cir::ConstantOp>();
  if (!constOp)
    return {};

  auto typedAttr = mlir::dyn_cast<mlir::TypedAttr>(constOp.getValue());
  if (!typedAttr)
    return {};

  std::optional<mlir::TypedAttr> retargeted =
      retargetStoreConstant(cgf.cgm, typedAttr, destType);
  if (!retargeted)
    return {};
  return cir::ConstantOp::create(cgf.getBuilder(), loc, *retargeted);
}

void CIRGenFunction::emitStoreOfScalar(mlir::Value value, Address addr,
                                       bool isVolatile, QualType ty,
                                       LValueBaseInfo baseInfo, bool isInit,
                                       bool isNontemporal) {
  mlir::Location loc = currSrcLoc ? *currSrcLoc : builder.getUnknownLoc();
  if (!value) {
    cgm.errorNYI(loc, "emitStoreOfScalar: value unavailable");
    return;
  }
  if (!addr.isValid()) {
    cgm.errorNYI(loc, "emitStoreOfScalar: destination unavailable");
    return;
  }

  if (const auto *clangVecTy = ty->getAs<clang::VectorType>()) {
    // Classic LLVM codegen packs boolean vectors into an `iN` storage type
    // in memory (see CGExpr.cpp's EmitToMemory/EmitFromMemory) because
    // plain LLVM IR does not otherwise guarantee a compact bit-packed
    // layout for `<N x i1>`. ClangIR does not need an analogous repacking
    // step here: exactly like scalar `cir.bool` (see emitToMemory and
    // emitLoadOfScalar's own comment on this same point), a vector of
    // `cir.bool` has an identical memory and value representation --
    // `CIRGenTypes::convertTypeForMem` is a pure passthrough to
    // `convertType` for every type, including `ExtVectorType`, so `addr`'s
    // element type here is always exactly `value`'s type already. The
    // ordinary `cir.store` below is therefore correct as-is; any actual
    // `iN`-packed layout is a CIR-to-LLVM lowering concern (DirectToLLVM's
    // own type conversion), not something CIRGen needs to pre-pack.
    if (!clangVecTy->isExtVectorBoolType()) {
      // Handle vectors of size 3 like size 4 for better performance.
      const mlir::Type elementType = addr.getElementType();
      const auto vecTy = cast<cir::VectorType>(elementType);

      // TODO(CIR): Use `ABIInfo::getOptimalVectorMemoryType` once it upstreamed
      assert(!cir::MissingFeatures::cirgenABIInfo());
      if (vecTy.getSize() == 3 && !getLangOpts().PreserveVec3Type)
        cgm.errorNYI(addr.getPointer().getLoc(),
                     "emitStoreOfScalar Vec3 & PreserveVec3Type disabled");
    }
  }

  value = emitToMemory(value, ty);
  if (!value) {
    cgm.errorNYI(loc, "emitStoreOfScalar: memory value unavailable");
    return;
  }

  assert(!cir::MissingFeatures::opLoadStoreTbaa());
  LValue atomicLValue = LValue::makeAddr(addr, ty, baseInfo);
  if (ty->isAtomicType() ||
      (!isInit && isLValueSuitableForInlineAtomic(atomicLValue))) {
    emitAtomicStore(RValue::get(value), atomicLValue, isInit);
    return;
  }

  // Update the alloca with more info on initialization.
  assert(addr.getPointer() && "expected pointer to exist");
  auto srcAlloca = addr.getDefiningOp<cir::AllocaOp>();
  if (currVarDecl && srcAlloca) {
    const VarDecl *vd = currVarDecl;
    assert(vd && "VarDecl expected");
    if (vd->hasInit())
      srcAlloca.setInitAttr(mlir::UnitAttr::get(&getMLIRContext()));
  }

  if (value.getType() != addr.getElementType()) {
    if (mlir::Value retargeted =
            retargetStoreConstantValue(*this, loc, value, addr.getElementType())) {
      value = retargeted;
    } else if (canStoreThroughRepresentationalRecordBitcast(
                   cgm, value.getType(), addr.getElementType())) {
      addr = addr.withElementType(builder, value.getType());
    } else {
      std::string message =
          "emitStoreOfScalar: value type does not match destination type: " +
          getTypeString(value.getType()) + " -> " +
          getTypeString(addr.getElementType());
      cgm.errorNYI(loc, message);
      return;
    }
  }

  cir::StoreOp store = builder.createStore(loc, value, addr, isVolatile);
  if (isNontemporal)
    store.getOperation()->setAttr("nontemporal",
                                  mlir::UnitAttr::get(&getMLIRContext()));

  assert(!cir::MissingFeatures::opTBAA());
}

// TODO: Replace this with a proper TargetInfo function call.
/// Helper method to check if the underlying ABI is AAPCS
static bool isAAPCS(const TargetInfo &targetInfo) {
  return targetInfo.getABI().starts_with("aapcs");
}

mlir::Value CIRGenFunction::emitStoreThroughBitfieldLValue(RValue src,
                                                           LValue dst) {

  const CIRGenBitFieldInfo &info = dst.getBitFieldInfo();
  mlir::Type resLTy = convertTypeForMem(dst.getType());
  bool resultIsBool = mlir::isa<cir::BoolType>(resLTy);
  mlir::Type bitfieldResultTy = resLTy;
  if (resultIsBool)
    bitfieldResultTy = builder.getUIntNTy(info.size ? info.size : 1);

  if (!mlir::isa<cir::IntType>(bitfieldResultTy)) {
    cgm.errorNYI(src.getValue() ? src.getValue().getLoc()
                                : builder.getUnknownLoc(),
                 "emitStoreThroughBitfieldLValue: non-integer result type");
    return nullptr;
  }

  mlir::Value srcValue = src.getValue();
  if (!srcValue) {
    cgm.errorNYI(builder.getUnknownLoc(),
                 "emitStoreThroughBitfieldLValue: value unavailable");
    return nullptr;
  }
  if (resultIsBool && mlir::isa<cir::BoolType>(srcValue.getType()))
    srcValue = builder.createBoolToInt(srcValue, bitfieldResultTy);
  else if (mlir::isa<cir::IntType>(srcValue.getType()) &&
           srcValue.getType() != bitfieldResultTy)
    srcValue = builder.createIntCast(srcValue, bitfieldResultTy);

  mlir::Value bitFieldPtr = dst.getBitFieldPointer();
  if (!bitFieldPtr || !mlir::isa<cir::PointerType>(bitFieldPtr.getType())) {
    cgm.errorNYI(srcValue.getLoc(),
                 "emitStoreThroughBitfieldLValue: unavailable bitfield address");
    return nullptr;
  }

  Address ptr(bitFieldPtr,
              mlir::cast<cir::PointerType>(bitFieldPtr.getType()).getPointee(),
              dst.getAlignment());
  if (ptr.getElementType() != info.storageType)
    ptr = builder.createElementBitCast(srcValue.getLoc(), ptr, info.storageType);

  bool useVoaltile = cgm.getCodeGenOpts().AAPCSBitfieldWidth &&
                     dst.isVolatileQualified() &&
                     info.volatileStorageSize != 0 && isAAPCS(cgm.getTarget());

  mlir::Value result = builder.createSetBitfield(
      bitFieldPtr.getLoc(), bitfieldResultTy, ptr, ptr.getElementType(),
      srcValue, info, dst.isVolatileQualified(), useVoaltile);
  if (!resultIsBool)
    return result;

  mlir::Value zero = builder.getNullValue(bitfieldResultTy, result.getLoc());
  return builder.createCompare(result.getLoc(), cir::CmpOpKind::ne, result,
                               zero);
}

RValue CIRGenFunction::emitLoadOfBitfieldLValue(LValue lv, SourceLocation loc) {
  const CIRGenBitFieldInfo &info = lv.getBitFieldInfo();

  // Get the output type.
  mlir::Type resLTy = convertType(lv.getType());
  bool resultIsBool = mlir::isa<cir::BoolType>(resLTy);
  mlir::Type bitfieldResultTy = resLTy;
  if (resultIsBool)
    bitfieldResultTy = builder.getUIntNTy(info.size ? info.size : 1);

  if (!mlir::isa<cir::IntType>(bitfieldResultTy)) {
    cgm.errorNYI(loc, "emitLoadOfBitfieldLValue: non-integer result type");
    return RValue::get(nullptr);
  }
  mlir::Value bitFieldPtr = lv.getBitFieldPointer();
  if (!bitFieldPtr || !mlir::isa<cir::PointerType>(bitFieldPtr.getType())) {
    cgm.errorNYI(loc, "emitLoadOfBitfieldLValue: unavailable bitfield address");
    return RValue::get(nullptr);
  }

  Address ptr(bitFieldPtr,
              mlir::cast<cir::PointerType>(bitFieldPtr.getType()).getPointee(),
              lv.getAlignment());
  if (ptr.getElementType() != info.storageType)
    ptr = builder.createElementBitCast(getLoc(loc), ptr, info.storageType);

  bool useVoaltile = lv.isVolatileQualified() && info.volatileOffset != 0 &&
                     isAAPCS(cgm.getTarget());

  mlir::Value field =
      builder.createGetBitfield(getLoc(loc), bitfieldResultTy, ptr,
                                ptr.getElementType(), info, lv.isVolatile(),
                                useVoaltile);
  if (resultIsBool) {
    mlir::Value zero = builder.getNullValue(bitfieldResultTy, field.getLoc());
    field =
        builder.createCompare(field.getLoc(), cir::CmpOpKind::ne, field, zero);
  }
  assert(!cir::MissingFeatures::opLoadEmitScalarRangeCheck() && "NYI");
  return RValue::get(field);
}

Address CIRGenFunction::getAddrOfBitFieldStorage(LValue base,
                                                 const FieldDecl *field,
                                                 mlir::Type fieldType,
                                                 unsigned index) {
  mlir::Location loc = getLoc(field->getLocation());
  mlir::Value basePtr = base.getPointer();
  if (!basePtr || !mlir::isa<cir::PointerType>(basePtr.getType())) {
    cgm.errorNYI(field->getSourceRange(),
                 "getAddrOfBitFieldStorage: unavailable base address");
    return Address::invalid();
  }

  mlir::Type baseElemTy =
      mlir::cast<cir::PointerType>(basePtr.getType()).getPointee();
  auto rec = mlir::dyn_cast<cir::RecordType>(baseElemTy);
  if (!rec) {
    cgm.errorNYI(field->getSourceRange(),
                 "getAddrOfBitFieldStorage: non-record base address");
    return Address::invalid();
  }

  cir::PointerType fieldPtr = cir::PointerType::get(fieldType);
  cir::GetMemberOp sea = getBuilder().createGetMember(
      loc, fieldPtr, basePtr, field->getName(),
      rec.isUnion() ? field->getFieldIndex() : index);
  CharUnits offset = CharUnits::fromQuantity(
      rec.getElementOffset(cgm.getDataLayout().layout, index));
  return Address(sea, base.getAlignment().alignmentAtOffset(offset));
}

LValue CIRGenFunction::emitLValueForBitField(LValue base,
                                             const FieldDecl *field) {
  LValueBaseInfo baseInfo = base.getBaseInfo();
  const CIRGenRecordLayout &layout =
      cgm.getTypes().getCIRGenRecordLayout(field->getParent());
  const CIRGenBitFieldInfo &info = layout.getBitFieldInfo(field);

  assert(!cir::MissingFeatures::preservedAccessIndexRegion());

  unsigned idx = layout.getCIRFieldNo(field);
  Address addr = getAddrOfBitFieldStorage(base, field, info.storageType, idx);
  if (!addr.isValid())
    return LValue();

  mlir::Location loc = getLoc(field->getLocation());
  if (addr.getElementType() != info.storageType)
    addr = builder.createElementBitCast(loc, addr, info.storageType);

  QualType fieldType =
      field->getType().withCVRQualifiers(base.getVRQualifiers());
  // TODO(cir): Support TBAA for bit fields.
  assert(!cir::MissingFeatures::opTBAA());
  LValueBaseInfo fieldBaseInfo(baseInfo.getAlignmentSource());
  return LValue::makeBitfield(addr, info, fieldType, fieldBaseInfo);
}

LValue CIRGenFunction::emitLValueForField(LValue base, const FieldDecl *field) {
  if (base.getType().isNull()) {
    cgm.errorNYI(field->getSourceRange(),
                 "emitLValueForField: unavailable base lvalue");
    return LValue();
  }
  LValueBaseInfo baseInfo = base.getBaseInfo();

  if (field->isBitField())
    return emitLValueForBitField(base, field);

  QualType fieldType = field->getType();
  const RecordDecl *rec = field->getParent();
  AlignmentSource baseAlignSource = baseInfo.getAlignmentSource();
  LValueBaseInfo fieldBaseInfo(getFieldAlignmentSource(baseAlignSource));
  assert(!cir::MissingFeatures::opTBAA());

  Address addr = base.getAddress();
  if (auto *classDecl = dyn_cast<CXXRecordDecl>(rec)) {
    if (cgm.getCodeGenOpts().StrictVTablePointers &&
        classDecl->isDynamicClass()) {
      cgm.errorNYI(field->getSourceRange(),
                   "emitLValueForField: strict vtable for dynamic class");
    }
  }

  unsigned recordCVR = base.getVRQualifiers();

  llvm::StringRef fieldName = field->getName();
  unsigned fieldIndex;
  if (cgm.lambdaFieldToName.count(field))
    fieldName = cgm.lambdaFieldToName[field];

  if (rec->isUnion())
    fieldIndex = field->getFieldIndex();
  else if (isEmptyFieldForLayout(getContext(), field)) {
    // Empty fields have no storage in the record layout, so there is no field
    // index to look up; emitAddrOfFieldStorage computes the address directly
    // from the byte offset.
    fieldIndex = 0;
  } else {
    const CIRGenRecordLayout &layout =
        cgm.getTypes().getCIRGenRecordLayout(field->getParent());
    fieldIndex = layout.getCIRFieldNo(field);
  }

  addr = emitAddrOfFieldStorage(addr, field, fieldName, fieldIndex);
  if (!addr.isValid())
    return LValue();
  assert(!cir::MissingFeatures::preservedAccessIndexRegion());

  // If this is a reference field, load the reference right now.
  if (fieldType->isReferenceType()) {
    assert(!cir::MissingFeatures::opTBAA());
    LValue refLVal = makeAddrLValue(addr, fieldType, fieldBaseInfo);
    if (recordCVR & Qualifiers::Volatile)
      refLVal.getQuals().addVolatile();
    addr = emitLoadOfReference(refLVal, getLoc(field->getSourceRange()),
                               &fieldBaseInfo);

    // Qualifiers on the struct don't apply to the referencee.
    recordCVR = 0;
    fieldType = fieldType->getPointeeType();
  }

  LValue lv = makeAddrLValue(addr, fieldType, fieldBaseInfo);
  lv.getQuals().addCVRQualifiers(recordCVR);

  // __weak attribute on a field is ignored.
  if (lv.getQuals().getObjCGCAttr() == Qualifiers::Weak) {
    cgm.errorNYI(field->getSourceRange(),
                 "emitLValueForField: __weak attribute");
    return LValue();
  }

  return lv;
}

LValue CIRGenFunction::emitLValueForFieldInitialization(
    LValue base, const clang::FieldDecl *field, llvm::StringRef fieldName) {
  if (base.getType().isNull()) {
    cgm.errorNYI(field->getSourceRange(),
                 "emitLValueForFieldInitialization: unavailable base lvalue");
    return LValue();
  }
  QualType fieldType = field->getType();

  if (!fieldType->isReferenceType())
    return emitLValueForField(base, field);

  const CIRGenRecordLayout &layout =
      cgm.getTypes().getCIRGenRecordLayout(field->getParent());
  unsigned fieldIndex = layout.getCIRFieldNo(field);

  Address v =
      emitAddrOfFieldStorage(base.getAddress(), field, fieldName, fieldIndex);
  if (!v.isValid())
    return LValue();

  // Make sure that the address is pointing to the right type.
  mlir::Type memTy = convertTypeForMem(fieldType);
  v = builder.createElementBitCast(getLoc(field->getSourceRange()), v, memTy);

  // TODO: Generate TBAA information that describes this access as a structure
  // member access and not just an access to an object of the field's type. This
  // should be similar to what we do in EmitLValueForField().
  LValueBaseInfo baseInfo = base.getBaseInfo();
  AlignmentSource fieldAlignSource = baseInfo.getAlignmentSource();
  LValueBaseInfo fieldBaseInfo(getFieldAlignmentSource(fieldAlignSource));
  assert(!cir::MissingFeatures::opTBAA());
  return makeAddrLValue(v, fieldType, fieldBaseInfo);
}

mlir::Value CIRGenFunction::emitToMemory(mlir::Value value, QualType ty) {
  // Bool has a different representation in memory than in registers,
  // but in ClangIR, it is simply represented as a cir.bool value.
  // This function is here as a placeholder for possible future changes.
  return value;
}

void CIRGenFunction::emitStoreOfScalar(mlir::Value value, LValue lvalue,
                                       bool isInit) {
  if (lvalue.getType()->isConstantMatrixType()) {
    assert(0 && "NYI: emitStoreOfScalar constant matrix type");
    return;
  }

  emitStoreOfScalar(value, lvalue.getAddress(), lvalue.isVolatile(),
                    lvalue.getType(), lvalue.getBaseInfo(), isInit,
                    /*isNontemporal=*/false);
}

mlir::Value CIRGenFunction::emitLoadOfScalar(Address addr, bool isVolatile,
                                             QualType ty, SourceLocation loc,
                                             LValueBaseInfo baseInfo) {
  // Traditional LLVM codegen handles thread local separately, CIR handles
  // as part of getAddrOfGlobalVar (GetGlobalOp).
  mlir::Type eltTy = addr.getElementType();

  if (const auto *clangVecTy = ty->getAs<clang::VectorType>()) {
    // See the symmetric comment in emitStoreOfScalar: ClangIR keeps the
    // memory and value representations of a vector of `cir.bool` identical
    // (`convertTypeForMem` is a passthrough to `convertType`), so `eltTy`
    // here is already exactly `convertType(ty)` -- an ordinary `cir.load`
    // below, with no truncation/unpacking step, is correct as-is.
    if (!clangVecTy->isExtVectorBoolType()) {
      const auto vecTy = cast<cir::VectorType>(eltTy);

      // Handle vectors of size 3 like size 4 for better performance.
      assert(!cir::MissingFeatures::cirgenABIInfo());
      if (vecTy.getSize() == 3 && !getLangOpts().PreserveVec3Type)
        cgm.errorNYI(addr.getPointer().getLoc(),
                     "emitLoadOfScalar Vec3 & PreserveVec3Type disabled");
    }
  }

  assert(!cir::MissingFeatures::opLoadStoreTbaa());
  LValue atomicLValue = LValue::makeAddr(addr, ty, baseInfo);
  if (ty->isAtomicType() || isLValueSuitableForInlineAtomic(atomicLValue)) {
    cir::LoadOp loadOp = builder.createLoad(getLoc(loc), addr, isVolatile);
    loadOp.setMemOrder(cir::MemOrder::SequentiallyConsistent);
    return loadOp;
  }

  if (mlir::isa<cir::VoidType>(eltTy))
    cgm.errorNYI(loc, "emitLoadOfScalar: void type");

  assert(!cir::MissingFeatures::opLoadEmitScalarRangeCheck());

  mlir::Value loadOp = builder.createLoad(getLoc(loc), addr, isVolatile);

  // Convert the in-memory representation to the in-register value.  In
  // classic LLVM codegen a type with boolean representation (but which is not
  // the builtin `bool` type, e.g. an enumeration with a fixed boolean
  // underlying type) is stored as a wider integer in memory and truncated to
  // its i1 value on load.  In ClangIR, `bool` is modeled directly as
  // `cir.bool` and its memory and value representations are identical, so no
  // truncation is required here -- the loaded value already has the correct
  // value type.  See emitToMemory for the symmetric store-side handling.
  if (!ty->isBooleanType() && ty->hasBooleanRepresentation()) {
    assert(loadOp.getType() == convertType(ty) &&
           "boolean-representation load type mismatch");
  }

  return loadOp;
}

mlir::Value CIRGenFunction::emitLoadOfScalar(LValue lvalue,
                                             SourceLocation loc) {
  assert(!cir::MissingFeatures::opLoadStoreNontemporal());
  assert(!cir::MissingFeatures::opLoadStoreTbaa());
  return emitLoadOfScalar(lvalue.getAddress(), lvalue.isVolatile(),
                          lvalue.getType(), loc, lvalue.getBaseInfo());
}

/// Given an expression that represents a value lvalue, this
/// method emits the address of the lvalue, then loads the result as an rvalue,
/// returning the rvalue.
RValue CIRGenFunction::emitLoadOfLValue(LValue lv, SourceLocation loc) {
  if (lv.getType().isNull()) {
    cgm.errorNYI(loc, "emitLoadOfLValue: unavailable lvalue");
    return RValue::get(nullptr);
  }

  assert(!lv.getType()->isFunctionType());
  assert(!(lv.getType()->isConstantMatrixType()) && "not implemented");

  if (lv.isBitField())
    return emitLoadOfBitfieldLValue(lv, loc);

  if (lv.isSimple())
    return RValue::get(emitLoadOfScalar(lv, loc));

  if (lv.isVectorElt()) {
    const mlir::Value load =
        builder.createLoad(getLoc(loc), lv.getVectorAddress());
    return RValue::get(cir::VecExtractOp::create(builder, getLoc(loc), load,
                                                 lv.getVectorIdx()));
  }

  if (lv.isExtVectorElt())
    return emitLoadOfExtVectorElementLValue(lv);

  cgm.errorNYI(loc, "emitLoadOfLValue");
  return RValue::get(nullptr);
}

int64_t CIRGenFunction::getAccessedFieldNo(unsigned int idx,
                                           const mlir::ArrayAttr elts) {
  auto elt = mlir::cast<mlir::IntegerAttr>(elts[idx]);
  return elt.getInt();
}

// If this is a reference to a subset of the elements of a vector, create an
// appropriate shufflevector.
RValue CIRGenFunction::emitLoadOfExtVectorElementLValue(LValue lv) {
  mlir::Location loc = lv.getExtVectorPointer().getLoc();
  mlir::Value vec = builder.createLoad(loc, lv.getExtVectorAddress());

  // HLSL allows treating scalars as one-element vectors. Converting the scalar
  // IR value to a vector here allows the rest of codegen to behave as normal.
  if (getLangOpts().HLSL && !mlir::isa<cir::VectorType>(vec.getType())) {
    cgm.errorNYI(loc, "emitLoadOfExtVectorElementLValue: HLSL");
    return {};
  }

  const mlir::ArrayAttr elts = lv.getExtVectorElts();

  // If the result of the expression is a non-vector type, we must be extracting
  // a single element. Just codegen as an extractelement.
  const auto *exprVecTy = lv.getType()->getAs<clang::VectorType>();
  if (!exprVecTy) {
    int64_t indexValue = getAccessedFieldNo(0, elts);
    cir::ConstantOp index =
        builder.getConstInt(loc, builder.getSInt64Ty(), indexValue);
    return RValue::get(cir::VecExtractOp::create(builder, loc, vec, index));
  }

  // Always use shuffle vector to try to retain the original program structure
  SmallVector<int64_t> mask;
  for (auto i : llvm::seq<unsigned>(0, exprVecTy->getNumElements()))
    mask.push_back(getAccessedFieldNo(i, elts));

  cir::VecShuffleOp resultVec = builder.createVecShuffle(loc, vec, mask);
  if (lv.getType()->isExtVectorBoolType()) {
    cgm.errorNYI(loc, "emitLoadOfExtVectorElementLValue: ExtVectorBoolType");
    return {};
  }

  return RValue::get(resultVec);
}

LValue
CIRGenFunction::emitPointerToDataMemberBinaryExpr(const BinaryOperator *e) {
  assert((e->getOpcode() == BO_PtrMemD || e->getOpcode() == BO_PtrMemI) &&
         "unexpected binary operator opcode");

  Address baseAddr = Address::invalid();
  if (e->getOpcode() == BO_PtrMemD)
    baseAddr = emitLValue(e->getLHS()).getAddress();
  else
    baseAddr = emitPointerWithAlignment(e->getLHS());

  const auto *memberPtrTy = e->getRHS()->getType()->castAs<MemberPointerType>();

  mlir::Value memberPtr = emitScalarExpr(e->getRHS());

  LValueBaseInfo baseInfo;
  assert(!cir::MissingFeatures::opTBAA());
  Address memberAddr = emitCXXMemberDataPointerAddress(e, baseAddr, memberPtr,
                                                       memberPtrTy, &baseInfo);

  return makeAddrLValue(memberAddr, memberPtrTy->getPointeeType(), baseInfo);
}

/// Generates lvalue for partial ext_vector access.
Address CIRGenFunction::emitExtVectorElementLValue(LValue lv,
                                                   mlir::Location loc) {
  Address vectorAddress = lv.getExtVectorAddress();
  QualType elementTy = lv.getType()->castAs<VectorType>()->getElementType();
  mlir::Type vectorElementTy = cgm.getTypes().convertType(elementTy);
  Address castToPointerElement =
      vectorAddress.withElementType(builder, vectorElementTy);

  mlir::ArrayAttr extVecElts = lv.getExtVectorElts();
  unsigned idx = getAccessedFieldNo(0, extVecElts);
  mlir::Value idxValue =
      builder.getConstInt(loc, mlir::cast<cir::IntType>(ptrDiffTy), idx);

  mlir::Value elementValue = builder.getArrayElement(
      loc, loc, castToPointerElement.getPointer(), vectorElementTy, idxValue,
      /*shouldDecay=*/false);

  const CharUnits eltSize = getContext().getTypeSizeInChars(elementTy);
  const CharUnits alignment =
      castToPointerElement.getAlignment().alignmentAtOffset(idx * eltSize);
  return Address(elementValue, vectorElementTy, alignment);
}

static cir::FuncOp emitFunctionDeclPointer(CIRGenModule &cgm, GlobalDecl gd) {
  assert(!cir::MissingFeatures::weakRefReference());
  return cgm.getAddrOfFunction(gd);
}

static LValue emitCapturedFieldLValue(CIRGenFunction &cgf, const FieldDecl *fd,
                                      mlir::Value thisValue) {
  return cgf.emitLValueForLambdaField(fd, thisValue);
}

/// Given that we are currently emitting a lambda, emit an l-value for
/// one of its members.
///
LValue CIRGenFunction::emitLValueForLambdaField(const FieldDecl *field,
                                                mlir::Value thisValue) {
  bool hasExplicitObjectParameter = false;
  const auto *methD = dyn_cast_if_present<CXXMethodDecl>(curCodeDecl);
  LValue lambdaLV;
  if (methD) {
    hasExplicitObjectParameter = methD->isExplicitObjectMemberFunction();
    assert(methD->getParent()->isLambda());
    assert(methD->getParent() == field->getParent());
  }
  if (hasExplicitObjectParameter) {
    cgm.errorNYI(field->getSourceRange(), "ExplicitObjectMemberFunction");
  } else {
    QualType lambdaTagType =
        getContext().getCanonicalTagType(field->getParent());
    lambdaLV = makeNaturalAlignAddrLValue(thisValue, lambdaTagType);
  }
  return emitLValueForField(lambdaLV, field);
}

LValue CIRGenFunction::emitLValueForLambdaField(const FieldDecl *field) {
  return emitLValueForLambdaField(field, cxxabiThisValue);
}

static LValue emitFunctionDeclLValue(CIRGenFunction &cgf, const Expr *e,
                                     GlobalDecl gd) {
  const FunctionDecl *fd = cast<FunctionDecl>(gd.getDecl());
  cir::FuncOp funcOp = emitFunctionDeclPointer(cgf.cgm, gd);
  mlir::Location loc = cgf.getLoc(e->getSourceRange());
  CharUnits align = cgf.getContext().getDeclAlign(fd);

  assert(!cir::MissingFeatures::sanitizers());

  mlir::Type fnTy = funcOp.getFunctionType();
  mlir::Type ptrTy = cir::PointerType::get(fnTy);
  mlir::Value addr = cir::GetGlobalOp::create(cgf.getBuilder(), loc, ptrTy,
                                              funcOp.getSymName());

  if (funcOp.getFunctionType() != cgf.convertType(fd->getType())) {
    fnTy = cgf.convertType(fd->getType());
    ptrTy = cir::PointerType::get(fnTy);

    addr = cir::CastOp::create(cgf.getBuilder(), addr.getLoc(), ptrTy,
                               cir::CastKind::bitcast, addr);
  }

  return cgf.makeAddrLValue(Address(addr, fnTy, align), e->getType(),
                            AlignmentSource::Decl);
}

/// Determine whether we can emit a reference to \p vd from the current
/// context, despite not necessarily having seen an odr-use of the variable in
/// this context.
/// TODO(cir): This could be shared with classic codegen.
static bool canEmitSpuriousReferenceToVariable(CIRGenFunction &cgf,
                                               const DeclRefExpr *e,
                                               const VarDecl *vd) {
  // For a variable declared in an enclosing scope, do not emit a spurious
  // reference even if we have a capture, as that will emit an unwarranted
  // reference to our capture state, and will likely generate worse code than
  // emitting a local copy.
  if (e->refersToEnclosingVariableOrCapture())
    return false;

  // For a local declaration declared in this function, we can always reference
  // it even if we don't have an odr-use.
  if (vd->hasLocalStorage()) {
    return vd->getDeclContext() ==
           dyn_cast_or_null<DeclContext>(cgf.curCodeDecl);
  }

  // For a global declaration, we can emit a reference to it if we know
  // for sure that we are able to emit a definition of it.
  vd = vd->getDefinition(cgf.getContext());
  if (!vd)
    return false;

  // Don't emit a spurious reference if it might be to a variable that only
  // exists on a different device / target.
  // FIXME: This is unnecessarily broad. Check whether this would actually be a
  // cross-target reference.
  if (cgf.getLangOpts().OpenMP || cgf.getLangOpts().CUDA ||
      cgf.getLangOpts().OpenCL) {
    return false;
  }

  // We can emit a spurious reference only if the linkage implies that we'll
  // be emitting a non-interposable symbol that will be retained until link
  // time.
  switch (cgf.cgm.getCIRLinkageVarDefinition(vd, /*IsConstant=*/false)) {
  case cir::GlobalLinkageKind::ExternalLinkage:
  case cir::GlobalLinkageKind::LinkOnceODRLinkage:
  case cir::GlobalLinkageKind::WeakODRLinkage:
  case cir::GlobalLinkageKind::InternalLinkage:
  case cir::GlobalLinkageKind::PrivateLinkage:
    return true;
  default:
    return false;
  }
}

LValue CIRGenFunction::emitDeclRefLValue(const DeclRefExpr *e) {
  const NamedDecl *nd = e->getDecl();
  QualType ty = e->getType();

  assert(e->isNonOdrUse() != NOUR_Unevaluated &&
         "should not emit an unevaluated operand");

  if (const auto *vd = dyn_cast<VarDecl>(nd)) {
    // Global Named registers access via intrinsics only
    if (vd->getStorageClass() == SC_Register && vd->hasAttr<AsmLabelAttr>() &&
        !vd->isLocalVarDecl()) {
      cgm.errorNYI(e->getSourceRange(),
                   "emitDeclRefLValue: Global Named registers access");
      return LValue();
    }

    // If this DeclRefExpr does not constitute an odr-use of the variable, we're
    // not permitted to emit a reference to it in general, and it might not be
    // captured if capture would be necessary for a use. Emit the constant value
    // directly instead.
    if (e->isNonOdrUse() == NOUR_Constant &&
        (vd->getType()->isReferenceType() ||
         !canEmitSpuriousReferenceToVariable(*this, e, vd))) {
      vd->getAnyInitializer(vd);
      APValue *evaluatedValue = vd->evaluateValue();
      if (!evaluatedValue) {
        cgm.errorNYI(e->getSourceRange(),
                     "emitDeclRefLValue: NonOdrUse constant unavailable");
        return LValue();
      }
      mlir::Attribute valAttr = ConstantEmitter(*this).emitAbstract(
          e->getLocation(), *evaluatedValue, vd->getType());
      if (!valAttr) {
        cgm.errorNYI(e->getSourceRange(),
                     "emitDeclRefLValue: failed constant expression");
        return LValue();
      }
      auto typedVal = mlir::dyn_cast<mlir::TypedAttr>(valAttr);

      if (!vd->getType()->isReferenceType() && typedVal) {
        // Spill the constant value to a private constant global and use its
        // address. This matches classic CodeGen's createUnnamedGlobalFrom.
        cir::GlobalOp gv = cgm.createUnnamedGlobalFrom(
            *vd, typedVal, getContext().getDeclAlign(vd));
        mlir::Type ptrTy = builder.getPointerTo(gv.getSymType());
        mlir::Value addrVal = cir::GetGlobalOp::create(
            builder, getLoc(e->getSourceRange()), ptrTy, gv.getSymNameAttr());
        Address addr(addrVal, gv.getSymType(),
                     getContext().getDeclAlign(vd));
        return makeAddrLValue(addr, ty, AlignmentSource::Decl);
      }

      if (vd->getType()->isReferenceType() && typedVal) {
        // For a reference-typed constant, the emitted constant is the value of
        // the reference itself, i.e. the (constant) address of the referent.
        // Materialize that pointer and build a natural-alignment address for
        // the referent type. This matches classic CodeGen's
        // makeNaturalAddressForPointer path for NOUR_Constant references.
        if (!mlir::isa<cir::PointerType>(typedVal.getType())) {
          cgm.errorNYI(e->getSourceRange(),
                       "emitDeclRefLValue: reference constant with "
                       "non-pointer CIR type");
          return LValue();
        }
        mlir::Value ptrVal =
            builder.getConstant(getLoc(e->getSourceRange()), typedVal);
        CharUnits alignment =
            cgm.getNaturalTypeAlignment(e->getType(), /*baseInfo=*/nullptr);
        Address addr = makeNaturalAddressForPointer(ptrVal, ty, alignment);
        return makeAddrLValue(addr, ty, AlignmentSource::Decl);
      }

      cgm.errorNYI(e->getSourceRange(),
                   "emitDeclRefLValue: NonOdrUse reference constant");
      return LValue();
    }

    // Check for captured variables.
    if (e->refersToEnclosingVariableOrCapture()) {
      vd = vd->getCanonicalDecl();
      if (FieldDecl *fd = lambdaCaptureFields.lookup(vd))
        return emitCapturedFieldLValue(*this, fd, cxxabiThisValue);
      if (curBlockInfo) {
        Address addr = getAddrOfBlockDecl(vd);
        if (!addr.isValid())
          return LValue();
        return makeAddrLValue(addr, ty, AlignmentSource::Decl);
      }
      assert(!cir::MissingFeatures::cgCapturedStmtInfo());
      assert(!cir::MissingFeatures::openMP());
    }
  }

  if (const auto *vd = dyn_cast<VarDecl>(nd)) {
    // Checks for omitted feature handling
    assert(!cir::MissingFeatures::opAllocaStaticLocal());
    assert(!cir::MissingFeatures::opAllocaNonGC());
    assert(!cir::MissingFeatures::opAllocaImpreciseLifetime());
    assert(!cir::MissingFeatures::opAllocaTLS());
    assert(!cir::MissingFeatures::opAllocaOpenMPThreadPrivate());
    assert(!cir::MissingFeatures::opAllocaEscapeByReference());

    // Check if this is a global variable
    if (vd->hasLinkage() || vd->isStaticDataMember())
      return emitGlobalVarDeclLValue(*this, e, vd);

    if (vd->isStaticLocal() && curBlockInfo) {
      cir::GlobalLinkageKind linkage =
          cgm.getCIRLinkageVarDefinition(vd, /*IsConstant=*/false);
      cir::GlobalOp global = cgm.getOrCreateStaticVarDecl(*vd, linkage);
      mlir::Value value =
          builder.createGetGlobal(getLoc(e->getSourceRange()), global,
                                  vd->getTLSKind() != VarDecl::TLS_None);
      mlir::Type realVarTy = convertTypeForMem(vd->getType());
      cir::PointerType realPtrTy = builder.getPointerTo(realVarTy);
      if (realPtrTy != value.getType())
        value = builder.createBitcast(value.getLoc(), value, realPtrTy);
      Address addr(value, realVarTy, getContext().getDeclAlign(vd));
      return makeAddrLValue(addr, ty, AlignmentSource::Decl);
    }

    Address addr = Address::invalid();

    // The variable should generally be present in the local decl map.
    auto iter = localDeclMap.find(vd);
    if (iter != localDeclMap.end()) {
      addr = iter->second;
    } else if (isa<ParmVarDecl>(vd)) {
      const Decl *canonical = vd->getCanonicalDecl();
      for (const auto &entry : localDeclMap) {
        const auto *mapped = dyn_cast<VarDecl>(entry.first);
        if (mapped && mapped->getCanonicalDecl() == canonical) {
          addr = entry.second;
          replaceAddrOfLocalVar(vd, addr);
          break;
        }
      }
    } else if (vd->isStaticLocal()) {
      // Unlike a normal local (whose cir.alloca lives in the function entry
      // block and therefore dominates every use in the function), the
      // cir.get_global materialized below is an ordinary SSA value created
      // at whatever point in the CFG this DeclRefExpr happens to be. A
      // static local can validly be referenced from multiple, mutually
      // non-dominating control-flow branches within the same function (e.g.
      // once in each arm of an if/else, or from inside a lambda that isn't
      // required to capture it). So, unlike other entries in localDeclMap,
      // this address must *not* be cached and replayed at other use sites:
      // doing so would hand out an SSA value from one branch to a use in a
      // sibling branch it doesn't dominate, tripping the MLIR dominance
      // verifier. Classic CodeGen avoids this for the same reason: it never
      // stores the static local's address in LocalDeclMap either, since
      // there each use recomputes an Address around the (dominance-free)
      // llvm::Constant global pointer. Here we instead recompute a fresh
      // cir.get_global at every use.
      cir::GlobalLinkageKind linkage =
          cgm.getCIRLinkageVarDefinition(vd, /*IsConstant=*/false);
      cir::GlobalOp global = cgm.getOrCreateStaticVarDecl(*vd, linkage);
      mlir::Value value =
          builder.createGetGlobal(getLoc(e->getSourceRange()), global,
                                  vd->getTLSKind() != VarDecl::TLS_None);
      mlir::Type realVarTy = convertTypeForMem(vd->getType());
      cir::PointerType realPtrTy = builder.getPointerTo(realVarTy);
      if (realPtrTy != value.getType())
        value = builder.createBitcast(value.getLoc(), value, realPtrTy);
      addr = Address(value, realVarTy, getContext().getDeclAlign(vd));
    } else {
      cgm.errorNYI(e->getSourceRange(),
                   "emitDeclRefLValue: missing local declaration address");
      return LValue();
    }
    if (!addr.isValid()) {
      cgm.errorNYI(e->getSourceRange(),
                   "emitDeclRefLValue: missing local declaration address");
      return LValue();
    }

    // Drill into reference types.
    LValue lv =
        vd->getType()->isReferenceType()
            ? emitLoadOfReferenceLValue(addr, getLoc(e->getSourceRange()),
                                        vd->getType(), AlignmentSource::Decl)
            : makeAddrLValue(addr, ty, AlignmentSource::Decl);

    // Statics are defined as globals, so they are not include in the function's
    // symbol table.
    assert((vd->isStaticLocal() || symbolTable.count(vd)) &&
           "non-static locals should be already mapped");

    return lv;
  }

  if (const auto *bd = dyn_cast<BindingDecl>(nd)) {
    if (e->refersToEnclosingVariableOrCapture()) {
      if (FieldDecl *fd = lambdaCaptureFields.lookup(bd))
        return emitCapturedFieldLValue(*this, fd, cxxabiThisValue);
      cgm.errorNYI(e->getSourceRange(),
                   "emitDeclRefLValue: binding lambda capture lookup failure");
      return LValue();
    }
    return emitLValue(bd->getBinding());
  }

  if (const auto *fd = dyn_cast<FunctionDecl>(nd)) {
    LValue lv = emitFunctionDeclLValue(*this, e, fd);

    // Emit debuginfo for the function declaration if the target wants to.
    if (getContext().getTargetInfo().allowDebugInfoForExternalRef())
      assert(!cir::MissingFeatures::generateDebugInfo());

    return lv;
  }

  if (const auto *tpo = dyn_cast<TemplateParamObjectDecl>(nd)) {
    cir::GlobalViewAttr addr = cgm.getAddrOfTemplateParamObject(tpo);
    if (!addr) {
      cgm.errorNYI(e->getSourceRange(),
                   "emitDeclRefLValue: template parameter object");
      return LValue();
    }

    mlir::Location loc = getLoc(e->getSourceRange());
    mlir::Value ptr = builder.getConstant(loc, addr);
    CharUnits align = getContext().getTypeAlignInChars(tpo->getType());
    mlir::Type objectTy = convertTypeForMem(tpo->getType());
    return makeAddrLValue(Address(ptr, objectTy, align), ty,
                          AlignmentSource::Decl);
  }

  cgm.errorNYI(e->getSourceRange(), "emitDeclRefLValue: unhandled decl type",
               nd->getDeclKindName());
  return LValue();
}

mlir::Value CIRGenFunction::evaluateExprAsBool(const Expr *e) {
  QualType boolTy = getContext().BoolTy;
  SourceLocation loc = e->getExprLoc();

  assert(!cir::MissingFeatures::pgoUse());
  if (e->getType()->getAs<MemberPointerType>()) {
    cgm.errorNYI(e->getSourceRange(),
                 "evaluateExprAsBool: member pointer type");
    return createDummyValue(getLoc(loc), boolTy);
  }

  assert(!cir::MissingFeatures::cgFPOptionsRAII());
  if (!e->getType()->isAnyComplexType())
    return emitScalarConversion(emitScalarExpr(e), e->getType(), boolTy, loc);

  return emitComplexToScalarConversion(emitComplexExpr(e), e->getType(), boolTy,
                                       loc);
}

LValue CIRGenFunction::emitUnaryOpLValue(const UnaryOperator *e) {
  UnaryOperatorKind op = e->getOpcode();

  // __extension__ doesn't affect lvalue-ness.
  if (op == UO_Extension)
    return emitLValue(e->getSubExpr());

  switch (op) {
  case UO_Deref: {
    QualType t = e->getSubExpr()->getType()->getPointeeType();
    assert(!t.isNull() && "CodeGenFunction::EmitUnaryOpLValue: Illegal type");

    assert(!cir::MissingFeatures::opTBAA());
    LValueBaseInfo baseInfo;
    Address addr = emitPointerWithAlignment(e->getSubExpr(), &baseInfo);
    if (!addr.isValid()) {
      cgm.errorNYI(e->getSourceRange(),
                   "emitUnaryOpLValue: unavailable dereference address");
      return LValue();
    }

    // Tag 'load' with deref attribute.
    // FIXME: This misses some derefence cases and has problematic interactions
    // with other operators.
    if (auto loadOp = addr.getDefiningOp<cir::LoadOp>())
      loadOp.setIsDerefAttr(mlir::UnitAttr::get(&getMLIRContext()));

    LValue lv = makeAddrLValue(addr, t, baseInfo);
    assert(!cir::MissingFeatures::addressSpace());
    assert(!cir::MissingFeatures::setNonGC());
    return lv;
  }
  case UO_Real:
  case UO_Imag: {
    LValue lv = emitLValue(e->getSubExpr());
    assert(lv.isSimple() && "real/imag on non-ordinary l-value");

    // __real is valid on scalars. This is a faster way of testing that.
    // __imag can only produce an rvalue on scalars.
    if (e->getOpcode() == UO_Real &&
        !mlir::isa<cir::ComplexType>(lv.getAddress().getElementType())) {
      assert(e->getSubExpr()->getType()->isArithmeticType());
      return lv;
    }

    QualType exprTy = getContext().getCanonicalType(e->getSubExpr()->getType());
    QualType elemTy = exprTy->castAs<clang::ComplexType>()->getElementType();
    mlir::Location loc = getLoc(e->getExprLoc());
    Address component =
        e->getOpcode() == UO_Real
            ? builder.createComplexRealPtr(loc, lv.getAddress())
            : builder.createComplexImagPtr(loc, lv.getAddress());
    assert(!cir::MissingFeatures::opTBAA());
    LValue elemLV = makeAddrLValue(component, elemTy);
    elemLV.getQuals().addQualifiers(lv.getQuals());
    return elemLV;
  }
  case UO_PreInc:
  case UO_PreDec: {
    cir::UnaryOpKind kind =
        e->isIncrementOp() ? cir::UnaryOpKind::Inc : cir::UnaryOpKind::Dec;
    LValue lv = emitLValue(e->getSubExpr());

    assert(e->isPrefix() && "Prefix operator in unexpected state!");

    if (e->getType()->isAnyComplexType()) {
      emitComplexPrePostIncDec(e, lv, kind, /*isPre=*/true);
    } else {
      emitScalarPrePostIncDec(e, lv, kind, /*isPre=*/true);
    }

    return lv;
  }
  case UO_Extension:
    llvm_unreachable("UnaryOperator extension should be handled above!");
  case UO_Plus:
  case UO_Minus:
  case UO_Not:
  case UO_LNot:
  case UO_AddrOf:
  case UO_PostInc:
  case UO_PostDec:
  case UO_Coawait:
    llvm_unreachable("UnaryOperator of non-lvalue kind!");
  }
  llvm_unreachable("Unknown unary operator kind!");
}

/// If the specified expr is a simple decay from an array to pointer,
/// return the array subexpression.
/// FIXME: this could be abstracted into a common AST helper.
static const Expr *getSimpleArrayDecayOperand(const Expr *e) {
  // If this isn't just an array->pointer decay, bail out.
  const auto *castExpr = dyn_cast<CastExpr>(e);
  if (!castExpr || castExpr->getCastKind() != CK_ArrayToPointerDecay)
    return nullptr;

  // If this is a decay from variable width array, bail out.
  const Expr *subExpr = castExpr->getSubExpr();
  if (subExpr->getType()->isVariableArrayType())
    return nullptr;

  return subExpr;
}

static cir::IntAttr getConstantIndexOrNull(mlir::Value idx) {
  // TODO(cir): should we consider using MLIRs IndexType instead of IntegerAttr?
  if (auto constantOp = idx.getDefiningOp<cir::ConstantOp>())
    return constantOp.getValueAttr<cir::IntAttr>();
  return {};
}

static CharUnits getArrayElementAlign(CharUnits arrayAlign, mlir::Value idx,
                                      CharUnits eltSize) {
  // If we have a constant index, we can use the exact offset of the
  // element we're accessing.
  if (const cir::IntAttr constantIdx = getConstantIndexOrNull(idx)) {
    const CharUnits offset = constantIdx.getValue().getZExtValue() * eltSize;
    return arrayAlign.alignmentAtOffset(offset);
  }
  // Otherwise, use the worst-case alignment for any element.
  return arrayAlign.alignmentOfArrayElement(eltSize);
}

static QualType getFixedSizeElementType(const ASTContext &astContext,
                                        const VariableArrayType *vla) {
  QualType eltType;
  do {
    eltType = vla->getElementType();
  } while ((vla = astContext.getAsVariableArrayType(eltType)));
  return eltType;
}

static mlir::Value emitArraySubscriptPtr(CIRGenFunction &cgf,
                                         mlir::Location beginLoc,
                                         mlir::Location endLoc, mlir::Value ptr,
                                         mlir::Type eltTy, mlir::Value idx,
                                         bool shouldDecay) {
  CIRGenModule &cgm = cgf.getCIRGenModule();
  // TODO(cir): LLVM codegen emits in bound gep check here, is there anything
  // that would enhance tracking this later in CIR?
  assert(!cir::MissingFeatures::emitCheckedInBoundsGEP());
  return cgm.getBuilder().getArrayElement(beginLoc, endLoc, ptr, eltTy, idx,
                                          shouldDecay);
}

static Address emitArraySubscriptPtr(CIRGenFunction &cgf,
                                     mlir::Location beginLoc,
                                     mlir::Location endLoc, Address addr,
                                     QualType eltType, mlir::Value idx,
                                     mlir::Location loc, bool shouldDecay) {
  CIRGenModule &cgm = cgf.getCIRGenModule();

  // Determine the element size of the statically-sized base.  This is
  // the thing that the indices are expressed in terms of.
  if (const VariableArrayType *vla =
          cgf.getContext().getAsVariableArrayType(eltType)) {
    eltType = getFixedSizeElementType(cgf.getContext(), vla);
  }

  if (!addr.isValid()) {
    cgm.errorNYI("emitArraySubscriptPtr: unavailable base address");
    return Address::invalid();
  }
  if (!idx) {
    cgm.errorNYI("emitArraySubscriptPtr: unavailable index");
    return Address::invalid();
  }

  // We can use that to compute the best alignment of the element.
  const CharUnits eltSize = cgf.getContext().getTypeSizeInChars(eltType);
  const CharUnits eltAlign =
      getArrayElementAlign(addr.getAlignment(), idx, eltSize);

  assert(!cir::MissingFeatures::preservedAccessIndexRegion());
  const mlir::Value eltPtr =
      emitArraySubscriptPtr(cgf, beginLoc, endLoc, addr.getPointer(),
                            addr.getElementType(), idx, shouldDecay);
  const mlir::Type elementType = cgf.convertTypeForMem(eltType);
  Address elementAddr(eltPtr,
                      mlir::cast<cir::PointerType>(eltPtr.getType())
                          .getPointee(),
                      eltAlign);
  return cgf.getBuilder().createElementBitCast(loc, elementAddr, elementType);
}

LValue
CIRGenFunction::emitArraySubscriptExpr(const clang::ArraySubscriptExpr *e) {
  if (e->getType()->getAs<ObjCObjectType>()) {
    cgm.errorNYI(e->getSourceRange(), "emitArraySubscriptExpr: ObjCObjectType");
    return LValue();
  }

  // The index must always be an integer, which is not an aggregate.  Emit it
  // in lexical order (this complexity is, sadly, required by C++17).
  assert((e->getIdx() == e->getLHS() || e->getIdx() == e->getRHS()) &&
         "index was neither LHS nor RHS");

  auto emitIdxAfterBase = [&](bool promote) -> mlir::Value {
    const mlir::Value idx = emitScalarExpr(e->getIdx());

    // Extend or truncate the index type to 32 or 64-bits.
    auto ptrTy = mlir::dyn_cast<cir::PointerType>(idx.getType());
    if (promote && ptrTy && ptrTy.isPtrTo<cir::IntType>())
      cgm.errorNYI(e->getSourceRange(),
                   "emitArraySubscriptExpr: index type cast");
    return idx;
  };

  // If the base is a vector type, then we are forming a vector element
  // with this subscript.
  if (e->getBase()->getType()->isSubscriptableVectorType() &&
      !isa<ExtVectorElementExpr>(e->getBase())) {
    const mlir::Value idx = emitIdxAfterBase(/*promote=*/false);
    const LValue lv = emitLValue(e->getBase());
    return LValue::makeVectorElt(lv.getAddress(), idx, e->getBase()->getType(),
                                 lv.getBaseInfo());
  }

  const mlir::Value idx = emitIdxAfterBase(/*promote=*/true);

  // Handle the extvector case we ignored above.
  if (isa<ExtVectorElementExpr>(e->getBase())) {
    const LValue lv = emitLValue(e->getBase());
    if (lv.getType().isNull()) {
      cgm.errorNYI(e->getSourceRange(),
                   "emitArraySubscriptExpr: unavailable vector base");
      return LValue();
    }
    Address addr = emitExtVectorElementLValue(lv, cgm.getLoc(e->getExprLoc()));

    QualType elementType = lv.getType()->castAs<VectorType>()->getElementType();
    addr = emitArraySubscriptPtr(*this, cgm.getLoc(e->getBeginLoc()),
                                 cgm.getLoc(e->getEndLoc()), addr, e->getType(),
                                 idx, cgm.getLoc(e->getExprLoc()),
                                 /*shouldDecay=*/false);
    if (!addr.isValid())
      return LValue();

    return makeAddrLValue(addr, elementType, lv.getBaseInfo());
  }

  if (const Expr *array = getSimpleArrayDecayOperand(e->getBase())) {
    LValue arrayLV;
    if (const auto *ase = dyn_cast<ArraySubscriptExpr>(array))
      arrayLV = emitArraySubscriptExpr(ase);
    else
      arrayLV = emitLValue(array);
    if (arrayLV.getType().isNull()) {
      cgm.errorNYI(e->getSourceRange(),
                   "emitArraySubscriptExpr: unavailable array base");
      return LValue();
    }

    // Propagate the alignment from the array itself to the result.
    const Address addr = emitArraySubscriptPtr(
        *this, cgm.getLoc(array->getBeginLoc()), cgm.getLoc(array->getEndLoc()),
        arrayLV.getAddress(), e->getType(), idx, cgm.getLoc(e->getExprLoc()),
        /*shouldDecay=*/true);
    if (!addr.isValid())
      return LValue();

    const LValue lv = LValue::makeAddr(addr, e->getType(), LValueBaseInfo());

    if (getLangOpts().ObjC && getLangOpts().getGC() != LangOptions::NonGC) {
      cgm.errorNYI(e->getSourceRange(), "emitArraySubscriptExpr: ObjC with GC");
    }

    return lv;
  }

  // The base must be a pointer; emit it with an estimate of its alignment.
  assert(e->getBase()->getType()->isPointerType() &&
         "The base must be a pointer");

  LValueBaseInfo eltBaseInfo;
  const Address ptrAddr = emitPointerWithAlignment(e->getBase(), &eltBaseInfo);
  if (!ptrAddr.isValid()) {
    cgm.errorNYI(e->getSourceRange(),
                 "emitArraySubscriptExpr: invalid base pointer");
    return LValue();
  }

  // Propagate the alignment from the array itself to the result.
  const Address addxr = emitArraySubscriptPtr(
      *this, cgm.getLoc(e->getBeginLoc()), cgm.getLoc(e->getEndLoc()), ptrAddr,
      e->getType(), idx, cgm.getLoc(e->getExprLoc()),
      /*shouldDecay=*/false);
  if (!addxr.isValid())
    return LValue();

  const LValue lv = LValue::makeAddr(addxr, e->getType(), eltBaseInfo);

  if (getLangOpts().ObjC && getLangOpts().getGC() != LangOptions::NonGC) {
    cgm.errorNYI(e->getSourceRange(), "emitArraySubscriptExpr: ObjC with GC");
  }

  return lv;
}

LValue CIRGenFunction::emitExtVectorElementExpr(const ExtVectorElementExpr *e) {
  // Emit the base vector as an l-value.
  LValue base;

  // ExtVectorElementExpr's base can either be a vector or pointer to vector.
  if (e->isArrow()) {
    // If it is a pointer to a vector, emit the address and form an lvalue with
    // it.
    LValueBaseInfo baseInfo;
    Address ptr = emitPointerWithAlignment(e->getBase(), &baseInfo);
    const auto *clangPtrTy =
        e->getBase()->getType()->castAs<clang::PointerType>();
    base = makeAddrLValue(ptr, clangPtrTy->getPointeeType(), baseInfo);
    base.getQuals().removeObjCGCAttr();
  } else if (e->getBase()->isGLValue()) {
    // Otherwise, if the base is an lvalue ( as in the case of foo.x.x),
    // emit the base as an lvalue.
    assert(e->getBase()->getType()->isVectorType());
    base = emitLValue(e->getBase());
  } else {
    // Otherwise, the base is a normal rvalue (as in (V+V).x), emit it as such.
    assert(e->getBase()->getType()->isVectorType() &&
           "Result must be a vector");
    mlir::Value vec = emitScalarExpr(e->getBase());

    // Store the vector to memory (because LValue wants an address).
    QualType baseTy = e->getBase()->getType();
    Address vecMem = createMemTemp(baseTy, vec.getLoc(), "tmp");
    if (!getLangOpts().HLSL && baseTy->isExtVectorBoolType()) {
      cgm.errorNYI(e->getSourceRange(),
                   "emitExtVectorElementExpr: ExtVectorBoolType & !HLSL");
      return {};
    }
    builder.createStore(vec.getLoc(), vec, vecMem);
    base = makeAddrLValue(vecMem, baseTy, AlignmentSource::Decl);
  }

  QualType type =
      e->getType().withCVRQualifiers(base.getQuals().getCVRQualifiers());

  // Encode the element access list into a vector of unsigned indices.
  SmallVector<uint32_t, 4> indices;
  e->getEncodedElementAccess(indices);

  if (base.isSimple()) {
    SmallVector<int64_t> attrElts(indices.begin(), indices.end());
    mlir::ArrayAttr elts = builder.getI64ArrayAttr(attrElts);
    return LValue::makeExtVectorElt(base.getAddress(), elts, type,
                                    base.getBaseInfo());
  }

  cgm.errorNYI(e->getSourceRange(),
               "emitExtVectorElementExpr: isSimple is false");
  return {};
}

LValue CIRGenFunction::emitStringLiteralLValue(const StringLiteral *e,
                                               llvm::StringRef name) {
  cir::GlobalOp globalOp = cgm.getGlobalForStringLiteral(e, name);
  assert(globalOp.getAlignment() && "expected alignment for string literal");
  unsigned align = *(globalOp.getAlignment());
  mlir::Value addr =
      builder.createGetGlobal(getLoc(e->getSourceRange()), globalOp);
  return makeAddrLValue(
      Address(addr, globalOp.getSymType(), CharUnits::fromQuantity(align)),
      e->getType(), AlignmentSource::Decl);
}

/// Casts are never lvalues unless that cast is to a reference type. If the cast
/// is to a reference, we can have the usual lvalue result, otherwise if a cast
/// is needed by the code generator in an lvalue context, then it must mean that
/// we need the address of an aggregate in order to access one of its members.
/// This can happen for all the reasons that casts are permitted with aggregate
/// result, including noop aggregate casts, and cast from scalar to union.
LValue CIRGenFunction::emitCastLValue(const CastExpr *e) {
  switch (e->getCastKind()) {
  case CK_ToVoid:
  case CK_BitCast:
  case CK_LValueToRValueBitCast:
  case CK_ArrayToPointerDecay:
  case CK_FunctionToPointerDecay:
  case CK_NullToMemberPointer:
  case CK_NullToPointer:
  case CK_IntegralToPointer:
  case CK_PointerToIntegral:
  case CK_PointerToBoolean:
  case CK_IntegralCast:
  case CK_BooleanToSignedIntegral:
  case CK_IntegralToBoolean:
  case CK_IntegralToFloating:
  case CK_FloatingToIntegral:
  case CK_FloatingToBoolean:
  case CK_FloatingCast:
  case CK_FloatingRealToComplex:
  case CK_FloatingComplexToReal:
  case CK_FloatingComplexToBoolean:
  case CK_FloatingComplexCast:
  case CK_FloatingComplexToIntegralComplex:
  case CK_IntegralRealToComplex:
  case CK_IntegralComplexToReal:
  case CK_IntegralComplexToBoolean:
  case CK_IntegralComplexCast:
  case CK_IntegralComplexToFloatingComplex:
  case CK_DerivedToBaseMemberPointer:
  case CK_BaseToDerivedMemberPointer:
  case CK_MemberPointerToBoolean:
  case CK_ReinterpretMemberPointer:
  case CK_AnyPointerToBlockPointerCast:
  case CK_ARCProduceObject:
  case CK_ARCConsumeObject:
  case CK_ARCReclaimReturnedObject:
  case CK_ARCExtendBlockObject:
  case CK_CopyAndAutoreleaseBlockObject:
  case CK_IntToOCLSampler:
  case CK_FloatingToFixedPoint:
  case CK_FixedPointToFloating:
  case CK_FixedPointCast:
  case CK_FixedPointToBoolean:
  case CK_FixedPointToIntegral:
  case CK_IntegralToFixedPoint:
  case CK_MatrixCast:
  case CK_HLSLVectorTruncation:
  case CK_HLSLMatrixTruncation:
  case CK_HLSLArrayRValue:
  case CK_HLSLElementwiseCast:
  case CK_HLSLAggregateSplatCast:
    llvm_unreachable("unexpected cast lvalue");

  case CK_Dependent:
    llvm_unreachable("dependent cast kind in IR gen!");

  case CK_BuiltinFnToFnPtr:
    llvm_unreachable("builtin functions are handled elsewhere");

  case CK_Dynamic: {
    LValue lv = emitLValue(e->getSubExpr());
    Address v = lv.getAddress();
    const auto *dce = cast<CXXDynamicCastExpr>(e);
    return makeNaturalAlignAddrLValue(emitDynamicCast(v, dce), e->getType());
  }

  // These are never l-values; just use the aggregate emission code.
  case CK_NonAtomicToAtomic:
  case CK_AtomicToNonAtomic:
  case CK_ToUnion:
  case CK_ObjCObjectLValueCast:
  case CK_VectorSplat:
  case CK_ConstructorConversion:
  case CK_UserDefinedConversion:
  case CK_CPointerToObjCPointerCast:
  case CK_BlockPointerToObjCPointerCast:
  case CK_LValueToRValue: {
    cgm.errorNYI(e->getSourceRange(),
                 std::string("emitCastLValue for unhandled cast kind: ") +
                     e->getCastKindName());

    return {};
  }
  case CK_AddressSpaceConversion: {
    LValue lv = emitLValue(e->getSubExpr());
    QualType destTy = getContext().getPointerType(e->getType());

    clang::LangAS srcLangAS = e->getSubExpr()->getType().getAddressSpace();
    cir::TargetAddressSpaceAttr srcAS;
    if (clang::isTargetAddressSpace(srcLangAS))
      srcAS = cir::toCIRTargetAddressSpace(getMLIRContext(), srcLangAS);
    else
      cgm.errorNYI(
          e->getSourceRange(),
          "emitCastLValue: address space conversion from unknown address "
          "space");

    mlir::Value v = getTargetHooks().performAddrSpaceCast(
        *this, lv.getPointer(), srcAS, convertType(destTy));

    return makeAddrLValue(Address(v, convertTypeForMem(e->getType()),
                                  lv.getAddress().getAlignment()),
                          e->getType(), lv.getBaseInfo());
  }

  case CK_LValueBitCast: {
    // This must be a reinterpret_cast (or c-style equivalent).
    const auto *ce = cast<ExplicitCastExpr>(e);

    cgm.emitExplicitCastExprType(ce, this);
    LValue LV = emitLValue(e->getSubExpr());
    Address V = LV.getAddress().withElementType(
        builder, convertTypeForMem(ce->getTypeAsWritten()->getPointeeType()));

    return makeAddrLValue(V, e->getType(), LV.getBaseInfo());
  }

  case CK_NoOp: {
    // CK_NoOp can model a qualification conversion, which can remove an array
    // bound and change the IR type.
    LValue lv = emitLValue(e->getSubExpr());
    // Propagate the volatile qualifier to LValue, if exists in e.
    if (e->changesVolatileQualification())
      lv.getQuals() = e->getType().getQualifiers();
    if (lv.isSimple()) {
      Address v = lv.getAddress();
      if (v.isValid()) {
        mlir::Type ty = convertTypeForMem(e->getType());
        if (v.getElementType() != ty)
          lv.setAddress(v.withElementType(builder, ty));
      }
    }
    return lv;
  }

  case CK_UncheckedDerivedToBase:
  case CK_DerivedToBase: {
    auto *derivedClassDecl = e->getSubExpr()->getType()->castAsCXXRecordDecl();

    LValue lv = emitLValue(e->getSubExpr());
    Address thisAddr = lv.getAddress();

    // Perform the derived-to-base conversion
    Address baseAddr =
        getAddressOfBaseClass(thisAddr, derivedClassDecl, e->path(),
                              /*NullCheckValue=*/false, e->getExprLoc());

    // TODO: Support accesses to members of base classes in TBAA. For now, we
    // conservatively pretend that the complete object is of the base class
    // type.
    assert(!cir::MissingFeatures::opTBAA());
    return makeAddrLValue(baseAddr, e->getType(), lv.getBaseInfo());
  }

  case CK_BaseToDerived: {
    const auto *derivedClassDecl = e->getType()->castAsCXXRecordDecl();
    LValue lv = emitLValue(e->getSubExpr());

    // Perform the base-to-derived conversion
    Address derived = getAddressOfDerivedClass(
        getLoc(e->getSourceRange()), lv.getAddress(), derivedClassDecl,
        e->path(), /*NullCheckValue=*/false);
    // C++11 [expr.static.cast]p2: Behavior is undefined if a downcast is
    // performed and the object is not of the derived type.
    assert(!cir::MissingFeatures::sanitizers());

    assert(!cir::MissingFeatures::opTBAA());
    return makeAddrLValue(derived, e->getType(), lv.getBaseInfo());
  }

  case CK_ZeroToOCLOpaqueType:
    llvm_unreachable("NULL to OpenCL opaque type lvalue cast is not valid");
  }

  llvm_unreachable("Invalid cast kind");
}

static DeclRefExpr *tryToConvertMemberExprToDeclRefExpr(CIRGenFunction &cgf,
                                                        const MemberExpr *me) {
  if (auto *vd = dyn_cast<VarDecl>(me->getMemberDecl())) {
    // Try to emit static variable member expressions as DREs.
    return DeclRefExpr::Create(
        cgf.getContext(), NestedNameSpecifierLoc(), SourceLocation(), vd,
        /*RefersToEnclosingVariableOrCapture=*/false, me->getExprLoc(),
        me->getType(), me->getValueKind(), nullptr, nullptr, me->isNonOdrUse());
  }
  return nullptr;
}

LValue CIRGenFunction::emitMemberExpr(const MemberExpr *e) {
  if (DeclRefExpr *dre = tryToConvertMemberExprToDeclRefExpr(*this, e)) {
    emitIgnoredExpr(e->getBase());
    return emitDeclRefLValue(dre);
  }

  Expr *baseExpr = e->getBase();
  // If this is s.x, emit s as an lvalue.  If it is s->x, emit s as a scalar.
  LValue baseLV;
  if (e->isArrow()) {
    LValueBaseInfo baseInfo;
    assert(!cir::MissingFeatures::opTBAA());
    Address addr = emitPointerWithAlignment(baseExpr, &baseInfo);
    QualType ptrTy = baseExpr->getType()->getPointeeType();
    assert(!cir::MissingFeatures::typeChecks());
    baseLV = makeAddrLValue(addr, ptrTy, baseInfo);
  } else {
    assert(!cir::MissingFeatures::typeChecks());
    baseLV = emitLValue(baseExpr);
  }

  const NamedDecl *nd = e->getMemberDecl();
  if (auto *field = dyn_cast<FieldDecl>(nd)) {
    LValue lv = emitLValueForField(baseLV, field);
    assert(!cir::MissingFeatures::setObjCGCLValueClass());
    if (getLangOpts().OpenMP) {
      // If the member was explicitly marked as nontemporal, mark it as
      // nontemporal. If the base lvalue is marked as nontemporal, mark access
      // to children as nontemporal too.
      cgm.errorNYI(e->getSourceRange(), "emitMemberExpr: OpenMP");
    }
    return lv;
  }

  if (isa<FunctionDecl>(nd)) {
    cgm.errorNYI(e->getSourceRange(), "emitMemberExpr: FunctionDecl");
    return LValue();
  }

  llvm_unreachable("Unhandled member declaration!");
}

/// Evaluate an expression into a given memory location.
void CIRGenFunction::emitAnyExprToMem(const Expr *e, Address location,
                                      Qualifiers quals, bool isInit) {
  // FIXME: This function should take an LValue as an argument.
  switch (getEvaluationKind(e->getType())) {
  case cir::TEK_Complex: {
    LValue lv = makeAddrLValue(location, e->getType());
    emitComplexExprIntoLValue(e, lv, isInit);
    return;
  }

  case cir::TEK_Aggregate: {
    emitAggExpr(e, AggValueSlot::forAddr(location, quals,
                                         AggValueSlot::IsDestructed_t(isInit),
                                         AggValueSlot::IsAliased_t(!isInit),
                                         AggValueSlot::MayOverlap));
    return;
  }

  case cir::TEK_Scalar: {
    mlir::Value value = emitScalarExpr(e);
    if (!value) {
      cgm.errorNYI(e->getSourceRange(),
                   "emitAnyExprToMem: scalar value unavailable");
      return;
    }
    RValue rv = RValue::get(value);
    LValue lv = makeAddrLValue(location, e->getType());
    emitStoreThroughLValue(rv, lv);
    return;
  }
  }

  llvm_unreachable("bad evaluation kind");
}

static Address createReferenceTemporary(CIRGenFunction &cgf,
                                        const MaterializeTemporaryExpr *m,
                                        const Expr *inner) {
  // TODO(cir): cgf.getTargetHooks();
  switch (m->getStorageDuration()) {
  case SD_FullExpression:
  case SD_Automatic: {
    QualType ty = inner->getType();

    assert(!cir::MissingFeatures::mergeAllConstants());

    // The temporary memory should be created in the same scope as the extending
    // declaration of the temporary materialization expression.
    cir::AllocaOp extDeclAlloca;
    if (const ValueDecl *extDecl = m->getExtendingDecl()) {
      auto extDeclAddrIter = cgf.localDeclMap.find(extDecl);
      if (extDeclAddrIter != cgf.localDeclMap.end())
        extDeclAlloca = extDeclAddrIter->second.getDefiningOp<cir::AllocaOp>();
    }
    mlir::OpBuilder::InsertPoint ip;
    if (extDeclAlloca)
      ip = {extDeclAlloca->getBlock(), extDeclAlloca->getIterator()};
    return cgf.createMemTemp(ty, cgf.getLoc(m->getSourceRange()),
                             cgf.getCounterRefTmpAsString(), /*alloca=*/nullptr,
                             ip);
  }
  case SD_Thread:
  case SD_Static: {
    cgf.cgm.errorNYI(
        m->getSourceRange(),
        "createReferenceTemporary: static/thread storage duration");
    return Address::invalid();
  }

  case SD_Dynamic:
    llvm_unreachable("temporary can't have dynamic storage duration");
  }
  llvm_unreachable("unknown storage duration");
}

static void pushTemporaryCleanup(CIRGenFunction &cgf,
                                 const MaterializeTemporaryExpr *m,
                                 const Expr *e, Address referenceTemporary) {
  // Objective-C++ ARC:
  //   If we are binding a reference to a temporary that has ownership, we
  //   need to perform retain/release operations on the temporary.
  //
  // FIXME(ogcg): This should be looking at e, not m.
  if (m->getType().getObjCLifetime()) {
    cgf.cgm.errorNYI(e->getSourceRange(), "pushTemporaryCleanup: ObjCLifetime");
    return;
  }

  const QualType::DestructionKind dk = e->getType().isDestructedType();
  if (dk == QualType::DK_none)
    return;

  switch (m->getStorageDuration()) {
  case SD_Static:
  case SD_Thread: {
    CXXDestructorDecl *referenceTemporaryDtor = nullptr;
    if (const auto *classDecl =
            e->getType()->getBaseElementTypeUnsafe()->getAsCXXRecordDecl();
        classDecl && !classDecl->hasTrivialDestructor())
      // Get the destructor for the reference temporary.
      referenceTemporaryDtor = classDecl->getDestructor();

    if (!referenceTemporaryDtor)
      return;

    cgf.cgm.errorNYI(e->getSourceRange(), "pushTemporaryCleanup: static/thread "
                                          "storage duration with destructors");
    break;
  }

  case SD_FullExpression:
    cgf.pushDestroy(NormalAndEHCleanup, referenceTemporary, e->getType(),
                    CIRGenFunction::destroyCXXObject);
    break;

  case SD_Automatic:
    // The temporary's lifetime is extended to that of the binding (a reference
    // with automatic storage duration), so it must be destroyed when the
    // enclosing block exits rather than at the end of the full expression.
    // createReferenceTemporary already allocates the temporary in the extending
    // declaration's scope; here we register its destructor on the cleanup
    // stack. Classic CodeGen uses a dedicated lifetime-extended cleanup stack to
    // defer activation to the extending declaration's scope. CIR does not model
    // that yet, but pushing the destroy here ties it to the current cleanup
    // scope -- which is the extending declaration's scope for the common case of
    // a temporary materialized while initializing a local reference -- and fires
    // it (after the binding's own cleanup) at scope exit, giving the correct
    // destruction order.
    assert(!cir::MissingFeatures::lifetimeExtendedCleanup());
    cgf.pushDestroy(NormalAndEHCleanup, referenceTemporary, e->getType(),
                    CIRGenFunction::destroyCXXObject);
    break;

  case SD_Dynamic:
    llvm_unreachable("temporary cannot have dynamic storage duration");
  }
}

LValue CIRGenFunction::emitMaterializeTemporaryExpr(
    const MaterializeTemporaryExpr *m) {
  const Expr *e = m->getSubExpr();

  assert((!m->getExtendingDecl() || !isa<VarDecl>(m->getExtendingDecl()) ||
          !cast<VarDecl>(m->getExtendingDecl())->isARCPseudoStrong()) &&
         "Reference should never be pseudo-strong!");

  // FIXME: ideally this would use emitAnyExprToMem, however, we cannot do so
  // as that will cause the lifetime adjustment to be lost for ARC
  auto ownership = m->getType().getObjCLifetime();
  if (ownership != Qualifiers::OCL_None &&
      ownership != Qualifiers::OCL_ExplicitNone) {
    cgm.errorNYI(e->getSourceRange(),
                 "emitMaterializeTemporaryExpr: ObjCLifetime");
    return {};
  }

  SmallVector<const Expr *, 2> commaLHSs;
  SmallVector<SubobjectAdjustment, 2> adjustments;
  e = e->skipRValueSubobjectAdjustments(commaLHSs, adjustments);

  for (const Expr *ignored : commaLHSs)
    emitIgnoredExpr(ignored);

  if (isa<OpaqueValueExpr>(e)) {
    cgm.errorNYI(e->getSourceRange(),
                 "emitMaterializeTemporaryExpr: OpaqueValueExpr");
    return {};
  }

  // Create and initialize the reference temporary.
  Address object = createReferenceTemporary(*this, m, e);

  if (auto var = object.getPointer().getDefiningOp<cir::GlobalOp>()) {
    // TODO(cir): add something akin to stripPointerCasts() to ptr above
    cgm.errorNYI(e->getSourceRange(), "emitMaterializeTemporaryExpr: GlobalOp");
    return {};
  } else {
    assert(!cir::MissingFeatures::emitLifetimeMarkers());
    emitAnyExprToMem(e, object, Qualifiers(), /*isInitializer=*/true);
  }
  pushTemporaryCleanup(*this, m, e, object);

  // Perform derived-to-base casts and/or field accesses, to get from the
  // temporary object we created (and, potentially, for which we extended
  // the lifetime) to the subobject we're binding the reference to.
  if (!adjustments.empty()) {
    cgm.errorNYI(e->getSourceRange(),
                 "emitMaterializeTemporaryExpr: Adjustments");
    return {};
  }

  return makeAddrLValue(object, m->getType(), AlignmentSource::Decl);
}

LValue
CIRGenFunction::getOrCreateOpaqueLValueMapping(const OpaqueValueExpr *e) {
  assert(OpaqueValueMapping::shouldBindAsLValue(e));

  auto it = opaqueLValues.find(e);
  if (it != opaqueLValues.end())
    return it->second;

  assert(e->isUnique() && "LValue for a nonunique OVE hasn't been emitted");
  return emitLValue(e->getSourceExpr());
}

RValue
CIRGenFunction::getOrCreateOpaqueRValueMapping(const OpaqueValueExpr *e) {
  assert(!OpaqueValueMapping::shouldBindAsLValue(e));

  auto it = opaqueRValues.find(e);
  if (it != opaqueRValues.end())
    return it->second;

  assert(e->isUnique() && "RValue for a nonunique OVE hasn't been emitted");
  return emitAnyExpr(e->getSourceExpr());
}

LValue CIRGenFunction::emitCompoundLiteralLValue(const CompoundLiteralExpr *e) {
  if (e->isFileScope()) {
    cgm.errorNYI(e->getSourceRange(), "emitCompoundLiteralLValue: FileScope");
    return {};
  }

  if (e->getType()->isVariablyModifiedType()) {
    cgm.errorNYI(e->getSourceRange(),
                 "emitCompoundLiteralLValue: VariablyModifiedType");
    return {};
  }

  Address declPtr = createMemTemp(e->getType(), getLoc(e->getSourceRange()),
                                  ".compoundliteral");
  const Expr *initExpr = e->getInitializer();
  LValue result = makeAddrLValue(declPtr, e->getType(), AlignmentSource::Decl);

  emitAnyExprToMem(initExpr, declPtr, e->getType().getQualifiers(),
                   /*Init*/ true);

  // Block-scope compound literals are destroyed at the end of the enclosing
  // scope in C.
  if (!getLangOpts().CPlusPlus && e->getType().isDestructedType()) {
    cgm.errorNYI(e->getSourceRange(),
                 "emitCompoundLiteralLValue: non C++ DestructedType");
    return {};
  }

  return result;
}

LValue CIRGenFunction::emitCallExprLValue(const CallExpr *e) {
  RValue rv = emitCallExpr(e);

  if (!rv.isScalar()) {
    cgm.errorNYI(e->getSourceRange(), "emitCallExprLValue: non-scalar return");
    return {};
  }

  assert(e->getCallReturnType(getContext())->isReferenceType() &&
         "Can't have a scalar return unless the return type is a "
         "reference type!");

  return makeNaturalAlignPointeeAddrLValue(rv.getValue(), e->getType());
}

LValue CIRGenFunction::emitBinaryOperatorLValue(const BinaryOperator *e) {
  // Comma expressions just emit their LHS then their RHS as an l-value.
  if (e->getOpcode() == BO_Comma) {
    emitIgnoredExpr(e->getLHS());
    return emitLValue(e->getRHS());
  }

  if (e->getOpcode() == BO_PtrMemD || e->getOpcode() == BO_PtrMemI)
    return emitPointerToDataMemberBinaryExpr(e);

  assert(e->getOpcode() == BO_Assign && "unexpected binary l-value");

  // Note that in all of these cases, __block variables need the RHS
  // evaluated first just in case the variable gets moved by the RHS.

  switch (CIRGenFunction::getEvaluationKind(e->getType())) {
  case cir::TEK_Scalar: {
    assert(!cir::MissingFeatures::objCLifetime());
    if (e->getLHS()->getType().getObjCLifetime() !=
        clang::Qualifiers::ObjCLifetime::OCL_None) {
      cgm.errorNYI(e->getSourceRange(), "objc lifetimes");
      return {};
    }

    RValue rv = emitAnyExpr(e->getRHS());
    LValue lv = emitLValue(e->getLHS());

    SourceLocRAIIObject loc{*this, getLoc(e->getSourceRange())};
    if (lv.isBitField())
      emitStoreThroughBitfieldLValue(rv, lv);
    else
      emitStoreThroughLValue(rv, lv);

    if (getLangOpts().OpenMP) {
      cgm.errorNYI(e->getSourceRange(), "openmp");
      return {};
    }

    return lv;
  }

  case cir::TEK_Complex: {
    return emitComplexAssignmentLValue(e);
  }

  case cir::TEK_Aggregate:
    cgm.errorNYI(e->getSourceRange(), "aggregate lvalues");
    return {};
  }
  llvm_unreachable("bad evaluation kind");
}

namespace {
struct LValueOrRValue {
  LValue lv;
  RValue rv;
};
}

static LValueOrRValue emitPseudoObjectExpr(CIRGenFunction &cgf,
                                           const PseudoObjectExpr *e,
                                           bool forLValue,
                                           AggValueSlot slot) {
  SmallVector<CIRGenFunction::OpaqueValueMappingData, 4> opaques;
  const Expr *resultExpr = e->getResultExpr();
  LValueOrRValue result;

  for (const Expr *semantic : e->semantics()) {
    if (const auto *ov = dyn_cast<OpaqueValueExpr>(semantic)) {
      if (ov->isUnique()) {
        assert(ov != resultExpr);
        continue;
      }

      CIRGenFunction::OpaqueValueMappingData opaqueData;
      if (ov == resultExpr && ov->isPRValue() && !forLValue &&
          CIRGenFunction::hasAggregateEvaluationKind(ov->getType())) {
        cgf.emitAggExpr(ov->getSourceExpr(), slot);
        if (!slot.isIgnored()) {
          LValue lv = cgf.makeAddrLValue(slot.getAddress(), ov->getType(),
                                         AlignmentSource::Decl);
          opaqueData =
              CIRGenFunction::OpaqueValueMappingData::bind(cgf, ov, lv);
          opaques.push_back(opaqueData);
        }
        result.rv = slot.asRValue();
      } else {
        opaqueData =
            CIRGenFunction::OpaqueValueMappingData::bind(cgf, ov,
                                                         ov->getSourceExpr());

        if (ov == resultExpr) {
          if (forLValue)
            result.lv = cgf.emitLValue(ov);
          else
            result.rv = cgf.emitAnyExpr(ov, slot);
        }

        opaques.push_back(opaqueData);
      }
      continue;
    }

    if (semantic == resultExpr) {
      if (forLValue)
        result.lv = cgf.emitLValue(semantic);
      else
        result.rv = cgf.emitAnyExpr(semantic, slot);
      continue;
    }

    cgf.emitIgnoredExpr(semantic);
  }

  for (CIRGenFunction::OpaqueValueMappingData &opaque : opaques)
    opaque.unbind(cgf);

  return result;
}

RValue CIRGenFunction::emitPseudoObjectRValue(const PseudoObjectExpr *e,
                                              AggValueSlot slot) {
  return emitPseudoObjectExpr(*this, e, false, slot).rv;
}

LValue CIRGenFunction::emitPseudoObjectLValue(const PseudoObjectExpr *e) {
  return emitPseudoObjectExpr(*this, e, true, AggValueSlot::ignored()).lv;
}

/// Emit code to compute the specified expression which
/// can have any type.  The result is returned as an RValue struct.
RValue CIRGenFunction::emitAnyExpr(const Expr *e, AggValueSlot aggSlot,
                                   bool ignoreResult) {
  switch (CIRGenFunction::getEvaluationKind(e->getType())) {
  case cir::TEK_Scalar:
    return RValue::get(emitScalarExpr(e, ignoreResult));
  case cir::TEK_Complex:
    return RValue::getComplex(emitComplexExpr(e));
  case cir::TEK_Aggregate: {
    if (!ignoreResult && aggSlot.isIgnored())
      aggSlot = createAggTemp(e->getType(), getLoc(e->getSourceRange()),
                              getCounterAggTmpAsString());
    emitAggExpr(e, aggSlot);
    return aggSlot.asRValue();
  }
  }
  llvm_unreachable("bad evaluation kind");
}

// Detect the unusual situation where an inline version is shadowed by a
// non-inline version. In that case we should pick the external one
// everywhere. That's GCC behavior too.
static bool onlyHasInlineBuiltinDeclaration(const FunctionDecl *fd) {
  for (const FunctionDecl *pd = fd; pd; pd = pd->getPreviousDecl())
    if (!pd->isInlineBuiltinDeclaration())
      return false;
  return true;
}

CIRGenCallee CIRGenFunction::emitDirectCallee(const GlobalDecl &gd) {
  const auto *fd = cast<FunctionDecl>(gd.getDecl());

  if (unsigned builtinID = fd->getBuiltinID()) {
    StringRef ident = cgm.getMangledName(gd);
    std::string fdInlineName = (ident + ".inline").str();

    bool isPredefinedLibFunction =
        cgm.getASTContext().BuiltinInfo.isPredefinedLibFunction(builtinID);
    // Assume nobuiltins everywhere until we actually read the attributes.
    bool hasAttributeNoBuiltin = true;
    assert(!cir::MissingFeatures::attributeNoBuiltin());

    // When directing calling an inline builtin, call it through it's mangled
    // name to make it clear it's not the actual builtin.
    auto fn = cast<cir::FuncOp>(curFn);
    if (fn.getName() != fdInlineName && onlyHasInlineBuiltinDeclaration(fd)) {
      cir::FuncOp clone =
          mlir::cast_or_null<cir::FuncOp>(cgm.getGlobalValue(fdInlineName));

      if (!clone) {
        // Create a forward declaration - the body will be generated in
        // generateCode when the function definition is processed
        cir::FuncOp calleeFunc = emitFunctionDeclPointer(cgm, gd);
        mlir::OpBuilder::InsertionGuard guard(builder);
        builder.setInsertionPointToStart(cgm.getModule().getBody());

        clone = cir::FuncOp::create(builder, calleeFunc.getLoc(), fdInlineName,
                                    calleeFunc.getFunctionType());
        clone.setLinkageAttr(cir::GlobalLinkageKindAttr::get(
            &cgm.getMLIRContext(), cir::GlobalLinkageKind::InternalLinkage));
        clone.setSymVisibility("private");
        clone.setInlineKind(cir::InlineKind::AlwaysInline);
      }
      return CIRGenCallee::forDirect(clone, gd);
    }

    // Replaceable builtins provide their own implementation of a builtin. If we
    // are in an inline builtin implementation, avoid trivial infinite
    // recursion. Honor __attribute__((no_builtin("foo"))) or
    // __attribute__((no_builtin)) on the current function unless foo is
    // not a predefined library function which means we must generate the
    // builtin no matter what.
    else if (!isPredefinedLibFunction || !hasAttributeNoBuiltin)
      return CIRGenCallee::forBuiltin(builtinID, fd);
  }

  cir::FuncOp callee = emitFunctionDeclPointer(cgm, gd);

  assert(!cir::MissingFeatures::hip());

  return CIRGenCallee::forDirect(callee, gd);
}

RValue CIRGenFunction::getUndefRValue(QualType ty) {
  if (ty->isVoidType())
    return RValue::get(nullptr);

  mlir::Type cirTy = convertType(ty);
  return RValue::get(cir::ConstantOp::create(builder, *currSrcLoc,
                                             cir::UndefAttr::get(cirTy)));
}

mlir::Value CIRGenFunction::emitObjCStringLiteral(const ObjCStringLiteral *e) {
  return cgm.getObjCRuntime().generateConstantString(*this, e);
}

RValue CIRGenFunction::emitObjCMessageExpr(const ObjCMessageExpr *e,
                                           ReturnValueSlot returnValue) {
  return cgm.getObjCRuntime().generateMessageSend(*this, e, returnValue);
}

mlir::Value CIRGenFunction::emitBlockLiteral(const BlockExpr *e) {
  return cgm.getBlockRuntime().emitBlockLiteral(*this, e);
}

RValue CIRGenFunction::emitBlockCallExpr(const CallExpr *e,
                                         ReturnValueSlot returnValue) {
  return cgm.getBlockRuntime().emitBlockCallExpr(*this, e, returnValue);
}

Address CIRGenFunction::getAddrOfBlockDecl(const VarDecl *variable) {
  return cgm.getBlockRuntime().getAddrOfBlockDecl(*this, variable);
}

RValue CIRGenFunction::emitCall(clang::QualType calleeTy,
                                const CIRGenCallee &origCallee,
                                const clang::CallExpr *e,
                                ReturnValueSlot returnValue) {
  // Get the actual function type. The callee type will always be a pointer to
  // function type or a block pointer type.
  assert(calleeTy->isFunctionPointerType() &&
         "Callee must have function pointer type!");

  calleeTy = getContext().getCanonicalType(calleeTy);
  auto pointeeTy = cast<PointerType>(calleeTy)->getPointeeType();

  CIRGenCallee callee = origCallee;

  if (getLangOpts().CPlusPlus)
    assert(!cir::MissingFeatures::sanitizers());

  const auto *fnType = cast<FunctionType>(pointeeTy);

  assert(!cir::MissingFeatures::sanitizers());

  CallArgList args;
  assert(!cir::MissingFeatures::opCallArgEvaluationOrder());

  // A call through a C++23 static operator()/operator[] is modeled in the AST
  // as a CXXOperatorCallExpr whose first argument is the object expression even
  // though the static operator takes no implicit object parameter. Mirror
  // classic CodeGen: emit (and ignore) the object argument for its side
  // effects, then drop it so the remaining arguments line up with the
  // prototype's parameters.
  auto arguments = e->arguments();
  if (const auto *oce = dyn_cast<CXXOperatorCallExpr>(e)) {
    if (const auto *md =
            dyn_cast_or_null<CXXMethodDecl>(oce->getCalleeDecl());
        md && md->isStatic()) {
      emitIgnoredExpr(e->getArg(0));
      arguments = drop_begin(arguments, 1);
    }
  }

  emitCallArgs(args, dyn_cast<FunctionProtoType>(fnType), arguments,
               e->getDirectCallee());

  const CIRGenFunctionInfo &funcInfo =
      cgm.getTypes().arrangeFreeFunctionCall(args, fnType);

  // C99 6.5.2.2p6:
  //   If the expression that denotes the called function has a type that does
  //   not include a prototype, [the default argument promotions are performed].
  //   If the number of arguments does not equal the number of parameters, the
  //   behavior is undefined. If the function is defined with a type that
  //   includes a prototype, and either the prototype ends with an ellipsis (,
  //   ...) or the types of the arguments after promotion are not compatible
  //   with the types of the parameters, the behavior is undefined. If the
  //   function is defined with a type that does not include a prototype, and
  //   the types of the arguments after promotion are not compatible with those
  //   of the parameters after promotion, the behavior is undefined [except in
  //   some trivial cases].
  // That is, in the general case, we should assume that a call through an
  // unprototyped function type works like a *non-variadic* call. The way we
  // make this work is to cast to the exxact type fo the promoted arguments.
  if (isa<FunctionNoProtoType>(fnType)) {
    assert(!cir::MissingFeatures::opCallChain());
    assert(!cir::MissingFeatures::addressSpace());
    cir::FuncType calleeTy = getTypes().getFunctionType(funcInfo);
    // get non-variadic function type
    calleeTy = cir::FuncType::get(calleeTy.getInputs(),
                                  calleeTy.getReturnType(), false);
    auto calleePtrTy = cir::PointerType::get(calleeTy);

    mlir::Operation *fn = callee.getFunctionPointer();
    mlir::Value addr;
    if (auto funcOp = mlir::dyn_cast<cir::FuncOp>(fn)) {
      addr = cir::GetGlobalOp::create(
          builder, getLoc(e->getSourceRange()),
          cir::PointerType::get(funcOp.getFunctionType()), funcOp.getSymName());
    } else {
      addr = fn->getResult(0);
    }

    fn = builder.createBitcast(addr, calleePtrTy).getDefiningOp();
    callee.setFunctionPointer(fn);
  }

  assert(!cir::MissingFeatures::opCallFnInfoOpts());
  assert(!cir::MissingFeatures::hip());
  assert(!cir::MissingFeatures::opCallMustTail());

  cir::CIRCallOpInterface callOp;
  RValue callResult = emitCall(funcInfo, callee, returnValue, args, &callOp,
                               getLoc(e->getExprLoc()));

  assert(!cir::MissingFeatures::generateDebugInfo());

  return callResult;
}

CIRGenCallee CIRGenFunction::emitCallee(const clang::Expr *e) {
  e = e->IgnoreParens();

  // Look through function-to-pointer decay.
  if (const auto *implicitCast = dyn_cast<ImplicitCastExpr>(e)) {
    if (implicitCast->getCastKind() == CK_FunctionToPointerDecay ||
        implicitCast->getCastKind() == CK_BuiltinFnToFnPtr) {
      return emitCallee(implicitCast->getSubExpr());
    }
    // When performing an indirect call through a function pointer lvalue, the
    // function pointer lvalue is implicitly converted to an rvalue through an
    // lvalue-to-rvalue conversion.
    assert(implicitCast->getCastKind() == CK_LValueToRValue &&
           "unexpected implicit cast on function pointers");
  } else if (const auto *declRef = dyn_cast<DeclRefExpr>(e)) {
    // Resolve direct calls. The referenced decl is not necessarily a function:
    // a call through a function-pointer variable produces a DeclRefExpr to a
    // VarDecl, which must be handled as an indirect reference below.
    if (const auto *funcDecl = dyn_cast<FunctionDecl>(declRef->getDecl()))
      return emitDirectCallee(funcDecl);
    // Else fall through to the indirect reference handling below.
  } else if (auto me = dyn_cast<MemberExpr>(e)) {
    if (const auto *fd = dyn_cast<FunctionDecl>(me->getMemberDecl())) {
      emitIgnoredExpr(me->getBase());
      return emitDirectCallee(fd);
    }
    // Else fall through to the indirect reference handling below.
  } else if (const auto *nttp = dyn_cast<SubstNonTypeTemplateParmExpr>(e)) {
    // Look through template substitutions.
    return emitCallee(nttp->getReplacement());
  } else if (auto *pde = dyn_cast<CXXPseudoDestructorExpr>(e)) {
    return CIRGenCallee::forPseudoDestructor(pde);
  }

  // Otherwise, we have an indirect reference.
  mlir::Value calleePtr;
  QualType functionType;
  if (const auto *ptrType = e->getType()->getAs<clang::PointerType>()) {
    calleePtr = emitScalarExpr(e);
    functionType = ptrType->getPointeeType();
  } else {
    functionType = e->getType();
    calleePtr = emitLValue(e).getPointer();
  }
  assert(functionType->isFunctionType());

  GlobalDecl gd;
  if (const auto *vd =
          dyn_cast_or_null<VarDecl>(e->getReferencedDeclOfCallee()))
    gd = GlobalDecl(vd);

  CIRGenCalleeInfo calleeInfo(functionType->getAs<FunctionProtoType>(), gd);
  CIRGenCallee callee(calleeInfo, calleePtr.getDefiningOp());
  return callee;
}

RValue CIRGenFunction::emitCallExpr(const clang::CallExpr *e,
                                    ReturnValueSlot returnValue) {
  QualType calleeTy = e->getCallee()->getType();
  if (calleeTy->isBlockPointerType())
    return emitBlockCallExpr(e, returnValue);

  if (const auto *ce = dyn_cast<CXXMemberCallExpr>(e))
    return emitCXXMemberCallExpr(ce, returnValue);

  if (isa<CUDAKernelCallExpr>(e)) {
    cgm.errorNYI(e->getSourceRange(), "call to CUDA kernel");
    return RValue::get(nullptr);
  }

  if (const auto *operatorCall = dyn_cast<CXXOperatorCallExpr>(e)) {
    // A CXXOperatorCallExpr is created even for explicit object methods and for
    // static operators (C++23 static operator()/operator[]), but only implicit
    // object member functions take an implicit 'this' and must be emitted as a
    // C++ operator member call. Static operators fall through to be treated as
    // ordinary (static) function calls.
    if (const CXXMethodDecl *md =
            dyn_cast_or_null<CXXMethodDecl>(operatorCall->getCalleeDecl());
        md && md->isImplicitObjectMemberFunction())
      return emitCXXOperatorMemberCallExpr(operatorCall, md, returnValue);
  }

  CIRGenCallee callee = emitCallee(e->getCallee());

  if (callee.isBuiltin())
    return emitBuiltinExpr(callee.getBuiltinDecl(), callee.getBuiltinID(), e,
                           returnValue);

  if (callee.isPseudoDestructor())
    return emitCXXPseudoDestructorExpr(callee.getPseudoDestructorExpr());

  return emitCall(calleeTy, callee, e, returnValue);
}

/// Emit code to compute the specified expression, ignoring the result.
void CIRGenFunction::emitIgnoredExpr(const Expr *e) {
  if (e->isPRValue()) {
    emitAnyExpr(e, AggValueSlot::ignored(), /*ignoreResult=*/true);
    return;
  }

  // Just emit it as an l-value and drop the result.
  emitLValue(e);
}

Address CIRGenFunction::emitArrayToPointerDecay(const Expr *e,
                                                LValueBaseInfo *baseInfo) {
  assert(!cir::MissingFeatures::opTBAA());
  assert(e->getType()->isArrayType() &&
         "Array to pointer decay must have array source type!");

  // Expressions of array type can't be bitfields or vector elements.
  LValue lv = emitLValue(e);
  if (lv.getType().isNull() || !lv.getPointer()) {
    cgm.errorNYI(e->getSourceRange(),
                 "array-to-pointer decay: source lvalue unavailable");
    return Address::invalid();
  }
  Address addr = lv.getAddress();
  if (!addr.isValid()) {
    cgm.errorNYI(e->getSourceRange(),
                 "array-to-pointer decay: source address unavailable");
    return Address::invalid();
  }

  // If the array type was an incomplete type, we need to make sure
  // the decay ends up being the right type.
  auto lvalueAddrTy =
      mlir::dyn_cast<cir::PointerType>(addr.getPointer().getType());
  if (!lvalueAddrTy) {
    cgm.errorNYI(e->getSourceRange(),
                 "array-to-pointer decay: non-pointer source address");
    return Address::invalid();
  }

  if (e->getType()->isVariableArrayType())
    return addr;

  [[maybe_unused]] auto pointeeTy =
      mlir::cast<cir::ArrayType>(lvalueAddrTy.getPointee());

  [[maybe_unused]] mlir::Type arrayTy = convertType(e->getType());
  assert(mlir::isa<cir::ArrayType>(arrayTy) && "expected array");
  assert(pointeeTy == arrayTy);

  // The result of this decay conversion points to an array element within the
  // base lvalue. However, since TBAA currently does not support representing
  // accesses to elements of member arrays, we conservatively represent accesses
  // to the pointee object as if it had no any base lvalue specified.
  // TODO: Support TBAA for member arrays.
  QualType eltType = e->getType()->castAsArrayTypeUnsafe()->getElementType();
  assert(!cir::MissingFeatures::opTBAA());

  mlir::Value ptr = builder.maybeBuildArrayDecay(
      cgm.getLoc(e->getSourceRange()), addr.getPointer(),
      convertTypeForMem(eltType));
  if (!ptr) {
    cgm.errorNYI(e->getSourceRange(),
                 "array-to-pointer decay: decay unavailable");
    return Address::invalid();
  }
  return Address(ptr, addr.getAlignment());
}

/// Given the address of a temporary variable, produce an r-value of its type.
RValue CIRGenFunction::convertTempToRValue(Address addr, clang::QualType type,
                                           clang::SourceLocation loc) {
  LValue lvalue = makeAddrLValue(addr, type, AlignmentSource::Decl);
  switch (getEvaluationKind(type)) {
  case cir::TEK_Complex:
    return RValue::getComplex(emitLoadOfComplex(lvalue, loc));
  case cir::TEK_Aggregate:
    return lvalue.asAggregateRValue();
  case cir::TEK_Scalar:
    return RValue::get(emitLoadOfScalar(lvalue, loc));
  }
  llvm_unreachable("bad evaluation kind");
}

/// Emit an `if` on a boolean condition, filling `then` and `else` into
/// appropriated regions.
mlir::LogicalResult CIRGenFunction::emitIfOnBoolExpr(const Expr *cond,
                                                     const Stmt *thenS,
                                                     const Stmt *elseS) {
  mlir::Location thenLoc = getLoc(thenS->getSourceRange());
  std::optional<mlir::Location> elseLoc;
  if (elseS)
    elseLoc = getLoc(elseS->getSourceRange());

  mlir::LogicalResult resThen = mlir::success(), resElse = mlir::success();
  cir::IfOp ifOp = emitIfOnBoolExpr(
      cond, /*thenBuilder=*/
      [&](mlir::OpBuilder &, mlir::Location) {
        LexicalScope lexScope{*this, thenLoc, builder.getInsertionBlock()};
        resThen = emitStmt(thenS, /*useCurrentScope=*/true);
      },
      thenLoc,
      /*elseBuilder=*/
      [&](mlir::OpBuilder &, mlir::Location) {
        assert(elseLoc && "Invalid location for elseS.");
        LexicalScope lexScope{*this, *elseLoc, builder.getInsertionBlock()};
        resElse = emitStmt(elseS, /*useCurrentScope=*/true);
      },
      elseLoc);
  if (!ifOp)
    return mlir::failure();

  return mlir::LogicalResult::success(resThen.succeeded() &&
                                      resElse.succeeded());
}

/// Emit an `if` on a boolean condition, filling `then` and `else` into
/// appropriated regions.
static void terminateIfBody(CIRGenBuilderTy &builder, mlir::Region &region,
                            mlir::Location loc) {
  if (region.empty())
    return;

  SmallVector<mlir::Block *, 4> eraseBlocks;
  unsigned numBlocks = region.getBlocks().size();
  for (auto &block : region.getBlocks()) {
    if (numBlocks != 1 && block.empty() && block.hasNoPredecessors() &&
        block.hasNoSuccessors())
      eraseBlocks.push_back(&block);

    if (block.empty() ||
        !block.back().hasTrait<mlir::OpTrait::IsTerminator>()) {
      mlir::OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPointToEnd(&block);
      builder.createYield(loc);
    }
  }

  for (auto *block : eraseBlocks)
    block->erase();
}

cir::IfOp CIRGenFunction::emitIfOnBoolExpr(
    const clang::Expr *cond, BuilderCallbackRef thenBuilder,
    mlir::Location thenLoc, BuilderCallbackRef elseBuilder,
    std::optional<mlir::Location> elseLoc) {
  // Attempt to be as accurate as possible with IfOp location, generate
  // one fused location that has either 2 or 4 total locations, depending
  // on else's availability.
  SmallVector<mlir::Location, 2> ifLocs{thenLoc};
  if (elseLoc)
    ifLocs.push_back(*elseLoc);
  mlir::Location loc = mlir::FusedLoc::get(&getMLIRContext(), ifLocs);

  // Emit the code with the fully general case.
  mlir::Value condV = emitOpOnBoolExpr(loc, cond);
  if (!condV) {
    cgm.errorNYI(cond->getSourceRange(), "if condition unavailable");
    return {};
  }
  mlir::OpBuilder::InsertPoint thenBody;
  mlir::OpBuilder::InsertPoint elseBody;
  cir::IfOp ifOp = cir::IfOp::create(
      builder, loc, condV, elseLoc.has_value(),
      /*thenBuilder=*/
      [&](mlir::OpBuilder &b, mlir::Location) {
        thenBody = b.saveInsertionPoint();
      },
      /*elseBuilder=*/
      [&](mlir::OpBuilder &b, mlir::Location) {
        elseBody = b.saveInsertionPoint();
      });

  {
    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.restoreInsertionPoint(thenBody);
    thenBuilder(builder, thenLoc);
  }
  terminateIfBody(builder, ifOp.getThenRegion(), thenLoc);

  if (elseLoc) {
    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.restoreInsertionPoint(elseBody);
    elseBuilder(builder, *elseLoc);
    terminateIfBody(builder, ifOp.getElseRegion(), *elseLoc);
  }

  return ifOp;
}

/// TODO(cir): see EmitBranchOnBoolExpr for extra ideas).
mlir::Value CIRGenFunction::emitOpOnBoolExpr(mlir::Location loc,
                                             const Expr *cond) {
  assert(!cir::MissingFeatures::pgoUse());
  assert(!cir::MissingFeatures::generateDebugInfo());
  cond = cond->IgnoreParens();

  // In LLVM the condition is reversed here for efficient codegen.
  // This should be done in CIR prior to LLVM lowering, if we do now
  // we can make CIR based diagnostics misleading.
  //  cir.ternary(!x, t, f) -> cir.ternary(x, f, t)
  assert(!cir::MissingFeatures::shouldReverseUnaryCondOnBoolExpr());

  if (const ConditionalOperator *condOp = dyn_cast<ConditionalOperator>(cond)) {
    Expr *trueExpr = condOp->getTrueExpr();
    Expr *falseExpr = condOp->getFalseExpr();
    mlir::Value condV = emitOpOnBoolExpr(loc, condOp->getCond());
    ConditionalEvaluation eval(*this);

    mlir::Value ternaryOpRes =
        cir::TernaryOp::create(
            builder, loc, condV, /*thenBuilder=*/
            [this, &eval, trueExpr](mlir::OpBuilder &b, mlir::Location loc) {
              eval.beginEvaluation();
              mlir::Value lhs = emitScalarExpr(trueExpr);
              eval.endEvaluation();
              cir::YieldOp::create(b, loc, lhs);
            },
            /*elseBuilder=*/
            [this, &eval, falseExpr](mlir::OpBuilder &b, mlir::Location loc) {
              eval.beginEvaluation();
              mlir::Value rhs = emitScalarExpr(falseExpr);
              eval.endEvaluation();
              cir::YieldOp::create(b, loc, rhs);
            })
            .getResult();

    return emitScalarConversion(ternaryOpRes, condOp->getType(),
                                getContext().BoolTy, condOp->getExprLoc());
  }

  if (isa<CXXThrowExpr>(cond)) {
    cgm.errorNYI("NYI");
    return createDummyValue(loc, cond->getType());
  }

  // If the branch has a condition wrapped by __builtin_unpredictable,
  // create metadata that specifies that the branch is unpredictable.
  // Don't bother if not optimizing because that metadata would not be used.
  assert(!cir::MissingFeatures::insertBuiltinUnpredictable());

  // Emit the code with the fully general case.
  return evaluateExprAsBool(cond);
}

mlir::Value CIRGenFunction::emitAlloca(StringRef name, mlir::Type ty,
                                       mlir::Location loc, CharUnits alignment,
                                       bool insertIntoFnEntryBlock,
                                       mlir::Value arraySize) {
  mlir::Block *entryBlock = insertIntoFnEntryBlock
                                ? getCurFunctionEntryBlock()
                                : curLexScope->getEntryBlock();

  // If this is an alloca in the entry basic block of a cir.try and there's
  // a surrounding cir.scope, make sure the alloca ends up in the surrounding
  // scope instead. This is necessary in order to guarantee all SSA values are
  // reachable during cleanups.
  if (auto tryOp =
          llvm::dyn_cast_if_present<cir::TryOp>(entryBlock->getParentOp())) {
    if (auto scopeOp = llvm::dyn_cast<cir::ScopeOp>(tryOp->getParentOp()))
      entryBlock = &scopeOp.getScopeRegion().front();
  }

  return emitAlloca(name, ty, loc, alignment,
                    builder.getBestAllocaInsertPoint(entryBlock), arraySize);
}

mlir::Value CIRGenFunction::emitAlloca(StringRef name, mlir::Type ty,
                                       mlir::Location loc, CharUnits alignment,
                                       mlir::OpBuilder::InsertPoint ip,
                                       mlir::Value arraySize) {
  // CIR uses its own alloca address space rather than follow the target data
  // layout like original CodeGen. The data layout awareness should be done in
  // the lowering pass instead.
  cir::PointerType localVarPtrTy =
      builder.getPointerTo(ty, getCIRAllocaAddressSpace());
  mlir::IntegerAttr alignIntAttr = cgm.getSize(alignment);

  mlir::Value addr;
  {
    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.restoreInsertionPoint(ip);
    addr = builder.createAlloca(loc, /*addr type*/ localVarPtrTy,
                                /*var type*/ ty, name, alignIntAttr, arraySize);
    assert(!cir::MissingFeatures::astVarDeclInterface());
  }
  return addr;
}

static const CXXMethodDecl *
getDirectMemberFunctionPointerMethod(const Expr *e) {
  e = e->IgnoreParens();
  if (const auto *subst = dyn_cast<SubstNonTypeTemplateParmExpr>(e))
    return getDirectMemberFunctionPointerMethod(subst->getReplacement());
  if (const auto *uop = dyn_cast<UnaryOperator>(e)) {
    if (uop->getOpcode() != UO_AddrOf)
      return nullptr;
    const Expr *subExpr = uop->getSubExpr()->IgnoreParens();
    if (const auto *declRef = dyn_cast<DeclRefExpr>(subExpr))
      return dyn_cast<CXXMethodDecl>(declRef->getDecl());
  }
  return nullptr;
}

static const CXXRecordDecl *getPointeeOrObjectRecord(const Expr *e,
                                                     bool isArrow) {
  QualType baseTy = e->getType();
  if (isArrow) {
    const auto *ptrTy = baseTy->getAs<clang::PointerType>();
    return ptrTy ? ptrTy->getPointeeType()->getAsCXXRecordDecl() : nullptr;
  }
  return baseTy->getAsCXXRecordDecl();
}

static RValue emitIndirectCXXMemberFunctionPointerCall(
    CIRGenFunction &cgf, const CXXMemberCallExpr *ce, const BinaryOperator *bo,
    ReturnValueSlot returnValue) {
  bool isArrow = bo->getOpcode() == BO_PtrMemI;
  if (bo->getOpcode() != BO_PtrMemI && bo->getOpcode() != BO_PtrMemD) {
    cgf.cgm.errorNYI(ce->getSourceRange(),
                     "emitCXXMemberCallExpr: unexpected binary callee");
    return RValue::get(nullptr);
  }

  const auto *mpt = bo->getRHS()->getType()->castAs<MemberPointerType>();
  const auto *fpt = mpt->getPointeeType()->getAs<FunctionProtoType>();
  if (!fpt) {
    cgf.cgm.errorNYI(ce->getSourceRange(),
                     "emitCXXMemberCallExpr: unprototyped member function "
                     "pointer call");
    return RValue::get(nullptr);
  }

  const CXXRecordDecl *memberPtrClass = mpt->getMostRecentCXXRecordDecl();
  const CXXRecordDecl *baseClass =
      getPointeeOrObjectRecord(bo->getLHS(), isArrow);
  if (!baseClass ||
      baseClass->getCanonicalDecl() != memberPtrClass->getCanonicalDecl()) {
    cgf.cgm.errorNYI(ce->getSourceRange(),
                     "emitCXXMemberCallExpr: member function pointer this "
                     "adjustment");
    return RValue::get(nullptr);
  }

  const CXXRecordDecl *definition = memberPtrClass->getDefinition();
  if (!definition) {
    cgf.cgm.errorNYI(ce->getSourceRange(),
                     "emitCXXMemberCallExpr: incomplete member pointer class");
    return RValue::get(nullptr);
  }
  if (definition->isDynamicClass()) {
    cgf.cgm.errorNYI(ce->getSourceRange(),
                     "emitCXXMemberCallExpr: virtual-capable member function "
                     "pointer call");
    return RValue::get(nullptr);
  }

  Address thisAddr = Address::invalid();
  if (isArrow)
    thisAddr = cgf.emitPointerWithAlignment(bo->getLHS());
  else
    thisAddr = cgf.emitLValue(bo->getLHS()).getAddress();
  if (!thisAddr.isValid()) {
    cgf.cgm.errorNYI(ce->getSourceRange(),
                     "emitCXXMemberCallExpr: unavailable this address");
    return RValue::get(nullptr);
  }

  LValue memberPtrLValue = cgf.emitLValue(bo->getRHS()->IgnoreParenImpCasts());
  if (!memberPtrLValue.isSimple()) {
    cgf.cgm.errorNYI(ce->getSourceRange(),
                     "emitCXXMemberCallExpr: non-addressable member function "
                     "pointer");
    return RValue::get(nullptr);
  }

  mlir::Location loc = cgf.getLoc(ce->getExprLoc());
  mlir::Type storedFuncPtrTy = cgf.getBuilder().getVoidPtrTy();
  auto methodRecordTy =
      cgf.getBuilder().getAnonRecordTy({storedFuncPtrTy, cgf.cgm.ptrDiffTy});
  Address memberPtrAddr = memberPtrLValue.getAddress();
  auto methodRecordPtrTy = cgf.getBuilder().getPointerTo(methodRecordTy);
  Address methodRecordAddr(
      cgf.getBuilder().createBitcast(loc, memberPtrAddr.getPointer(),
                                     methodRecordPtrTy),
      methodRecordTy, memberPtrAddr.getAlignment());
  mlir::Value methodRecord = cgf.getBuilder().createLoad(loc, methodRecordAddr);
  mlir::Value fnPtr = cir::ExtractMemberOp::create(
      cgf.getBuilder(), loc, storedFuncPtrTy, methodRecord, 0);
  mlir::Value thisAdjustment = cir::ExtractMemberOp::create(
      cgf.getBuilder(), loc, cgf.cgm.ptrDiffTy, methodRecord, 1);

  mlir::Value adjustedThis = thisAddr.getPointer();
  mlir::Value thisAsBytes =
      cgf.getBuilder().createBitcast(loc, adjustedThis, cgf.cgm.uInt8PtrTy);
  mlir::Value adjustedBytes = cir::PtrStrideOp::create(
      cgf.getBuilder(), loc, cgf.cgm.uInt8PtrTy, thisAsBytes, thisAdjustment);
  adjustedThis =
      cgf.getBuilder().createBitcast(loc, adjustedBytes, adjustedThis.getType());

  CallArgList args;
  args.add(RValue::get(adjustedThis),
           cgf.getTypes().deriveThisType(memberPtrClass, nullptr));
  RequiredArgs required = RequiredArgs::getFromProtoWithExtraSlots(fpt, 1);
  cgf.emitCallArgs(args, fpt, ce->arguments(), ce->getDirectCallee());

  const CIRGenFunctionInfo &fnInfo =
      cgf.cgm.getTypes().arrangeCXXMethodCall(args, fpt, required, 0);
  cir::FuncType callFnTy = cgf.getTypes().getFunctionType(fnInfo);
  mlir::Value typedFnPtr =
      cgf.getBuilder().createBitcast(loc, fnPtr,
                                     cgf.getBuilder().getPointerTo(callFnTy));

  return cgf.emitCall(fnInfo,
                      CIRGenCallee::forDirect(typedFnPtr.getDefiningOp()),
                      returnValue, args, nullptr, loc);
}

// Note: this function also emit constructor calls to support a MSVC extensions
// allowing explicit constructor function call.
RValue CIRGenFunction::emitCXXMemberCallExpr(const CXXMemberCallExpr *ce,
                                             ReturnValueSlot returnValue) {
  const Expr *callee = ce->getCallee()->IgnoreParens();

  if (const auto *bo = dyn_cast<BinaryOperator>(callee)) {
    const auto *md = getDirectMemberFunctionPointerMethod(bo->getRHS());
    if (!md)
      return emitIndirectCXXMemberFunctionPointerCall(*this, ce, bo,
                                                     returnValue);

    bool isArrow = bo->getOpcode() == BO_PtrMemI;
    if (bo->getOpcode() != BO_PtrMemI && bo->getOpcode() != BO_PtrMemD) {
      cgm.errorNYI(ce->getSourceRange(),
                   "emitCXXMemberCallExpr: unexpected binary callee");
      return RValue::get(nullptr);
    }

    const auto *mpt = bo->getRHS()->getType()->castAs<MemberPointerType>();
    const CXXRecordDecl *memberPtrClass = mpt->getMostRecentCXXRecordDecl();
    const CXXRecordDecl *baseClass = getPointeeOrObjectRecord(bo->getLHS(),
                                                              isArrow);
    if (!baseClass || baseClass->getCanonicalDecl() !=
                          memberPtrClass->getCanonicalDecl()) {
      cgm.errorNYI(ce->getSourceRange(),
                   "emitCXXMemberCallExpr: member function pointer this "
                   "adjustment");
      return RValue::get(nullptr);
    }

    return emitCXXMemberOrOperatorMemberCallExpr(
        ce, md, returnValue, false, std::nullopt, isArrow, bo->getLHS());
  }

  if (!isa<MemberExpr>(callee)) {
    cgm.errorNYI(ce->getSourceRange(),
                 "emitCXXMemberCallExpr: unsupported callee");
    return RValue::get(nullptr);
  }

  const auto *me = cast<MemberExpr>(callee);
  const auto *md = cast<CXXMethodDecl>(me->getMemberDecl());

  if (md->isStatic()) {
    cgm.errorNYI(ce->getSourceRange(), "emitCXXMemberCallExpr: static method");
    return RValue::get(nullptr);
  }

  bool hasQualifier = me->hasQualifier();
  NestedNameSpecifier qualifier = me->getQualifier();
  bool isArrow = me->isArrow();
  const Expr *base = me->getBase();

  return emitCXXMemberOrOperatorMemberCallExpr(
      ce, md, returnValue, hasQualifier, qualifier, isArrow, base);
}

RValue CIRGenFunction::emitReferenceBindingToExpr(const Expr *e) {
  // Emit the expression as an lvalue.
  LValue lv = emitLValue(e);
  assert(lv.isSimple());
  mlir::Value value = lv.getPointer();

  assert(!cir::MissingFeatures::sanitizers());

  return RValue::get(value);
}

Address CIRGenFunction::emitLoadOfReference(LValue refLVal, mlir::Location loc,
                                            LValueBaseInfo *pointeeBaseInfo) {
  if (refLVal.isVolatile())
    cgm.errorNYI(loc, "load of volatile reference");

  cir::LoadOp load =
      cir::LoadOp::create(builder, loc, refLVal.getAddress().getElementType(),
                          refLVal.getAddress().getPointer());

  assert(!cir::MissingFeatures::opTBAA());

  QualType pointeeType = refLVal.getType()->getPointeeType();
  CharUnits align = cgm.getNaturalTypeAlignment(pointeeType, pointeeBaseInfo);
  return Address(load, convertTypeForMem(pointeeType), align);
}

LValue CIRGenFunction::emitLoadOfReferenceLValue(Address refAddr,
                                                 mlir::Location loc,
                                                 QualType refTy,
                                                 AlignmentSource source) {
  LValue refLVal = makeAddrLValue(refAddr, refTy, LValueBaseInfo(source));
  LValueBaseInfo pointeeBaseInfo;
  assert(!cir::MissingFeatures::opTBAA());
  Address pointeeAddr = emitLoadOfReference(refLVal, loc, &pointeeBaseInfo);
  return makeAddrLValue(pointeeAddr, refLVal.getType()->getPointeeType(),
                        pointeeBaseInfo);
}

void CIRGenFunction::emitTrap(mlir::Location loc, bool createNewBlock) {
  cir::TrapOp::create(builder, loc);
  if (createNewBlock)
    builder.createBlock(builder.getBlock()->getParent());
}

void CIRGenFunction::emitUnreachable(clang::SourceLocation loc,
                                     bool createNewBlock) {
  assert(!cir::MissingFeatures::sanitizers());
  cir::UnreachableOp::create(builder, getLoc(loc));
  if (createNewBlock)
    builder.createBlock(builder.getBlock()->getParent());
}

mlir::Value CIRGenFunction::createDummyValue(mlir::Location loc,
                                             clang::QualType qt) {
  mlir::Type t = convertType(qt);
  CharUnits alignment = getContext().getTypeAlignInChars(qt);
  return builder.createDummyValue(loc, t, alignment);
}

//===----------------------------------------------------------------------===//
// CIR builder helpers
//===----------------------------------------------------------------------===//

Address CIRGenFunction::createMemTemp(QualType ty, mlir::Location loc,
                                      const Twine &name, Address *alloca,
                                      mlir::OpBuilder::InsertPoint ip) {
  // FIXME: Should we prefer the preferred type alignment here?
  return createMemTemp(ty, getContext().getTypeAlignInChars(ty), loc, name,
                       alloca, ip);
}

Address CIRGenFunction::createMemTemp(QualType ty, CharUnits align,
                                      mlir::Location loc, const Twine &name,
                                      Address *alloca,
                                      mlir::OpBuilder::InsertPoint ip) {
  Address result = createTempAlloca(convertTypeForMem(ty), align, loc, name,
                                    /*ArraySize=*/nullptr, alloca, ip);
  if (ty->isConstantMatrixType()) {
    assert(!cir::MissingFeatures::matrixType());
    cgm.errorNYI(loc, "temporary matrix value");
  }
  return result;
}

/// This creates a alloca and inserts it into the entry block of the
/// current region.
Address CIRGenFunction::createTempAllocaWithoutCast(
    mlir::Type ty, CharUnits align, mlir::Location loc, const Twine &name,
    mlir::Value arraySize, mlir::OpBuilder::InsertPoint ip) {
  cir::AllocaOp alloca = ip.isSet()
                             ? createTempAlloca(ty, loc, name, ip, arraySize)
                             : createTempAlloca(ty, loc, name, arraySize);
  alloca.setAlignmentAttr(cgm.getSize(align));
  return Address(alloca, ty, align);
}

/// This creates a alloca and inserts it into the entry block. The alloca is
/// casted to default address space if necessary.
// TODO(cir): Implement address space casting to match classic codegen's
// CreateTempAlloca behavior with DestLangAS parameter
Address CIRGenFunction::createTempAlloca(mlir::Type ty, CharUnits align,
                                         mlir::Location loc, const Twine &name,
                                         mlir::Value arraySize,
                                         Address *allocaAddr,
                                         mlir::OpBuilder::InsertPoint ip) {
  Address alloca =
      createTempAllocaWithoutCast(ty, align, loc, name, arraySize, ip);
  if (allocaAddr)
    *allocaAddr = alloca;
  mlir::Value v = alloca.getPointer();
  // Alloca always returns a pointer in alloca address space, which may
  // be different from the type defined by the language. For example,
  // in C++ the auto variables are in the default address space. Therefore
  // cast alloca to the default address space when necessary.

  LangAS allocaAS = alloca.getAddressSpace()
                        ? clang::getLangASFromTargetAS(
                              alloca.getAddressSpace().getValue().getUInt())
                        : clang::LangAS::Default;
  LangAS dstTyAS = clang::LangAS::Default;
  if (getCIRAllocaAddressSpace()) {
    dstTyAS = clang::getLangASFromTargetAS(
        getCIRAllocaAddressSpace().getValue().getUInt());
  }

  if (dstTyAS != allocaAS) {
    getTargetHooks().performAddrSpaceCast(*this, v, getCIRAllocaAddressSpace(),
                                          builder.getPointerTo(ty, dstTyAS));
  }
  return Address(v, ty, align);
}

/// This creates an alloca and inserts it into the entry block if \p ArraySize
/// is nullptr, otherwise inserts it at the current insertion point of the
/// builder.
cir::AllocaOp CIRGenFunction::createTempAlloca(mlir::Type ty,
                                               mlir::Location loc,
                                               const Twine &name,
                                               mlir::Value arraySize,
                                               bool insertIntoFnEntryBlock) {
  return mlir::cast<cir::AllocaOp>(emitAlloca(name.str(), ty, loc, CharUnits(),
                                              insertIntoFnEntryBlock, arraySize)
                                       .getDefiningOp());
}

/// This creates an alloca and inserts it into the provided insertion point
cir::AllocaOp CIRGenFunction::createTempAlloca(mlir::Type ty,
                                               mlir::Location loc,
                                               const Twine &name,
                                               mlir::OpBuilder::InsertPoint ip,
                                               mlir::Value arraySize) {
  assert(ip.isSet() && "Insertion point is not set");
  return mlir::cast<cir::AllocaOp>(
      emitAlloca(name.str(), ty, loc, CharUnits(), ip, arraySize)
          .getDefiningOp());
}

/// Try to emit a reference to the given value without producing it as
/// an l-value.  For many cases, this is just an optimization, but it avoids
/// us needing to emit global copies of variables if they're named without
/// triggering a formal use in a context where we can't emit a direct
/// reference to them, for instance if a block or lambda or a member of a
/// local class uses a const int variable or constexpr variable from an
/// enclosing function.
///
/// For named members of enums, this is the only way they are emitted.
CIRGenFunction::ConstantEmission
CIRGenFunction::tryEmitAsConstant(const DeclRefExpr *refExpr) {
  const ValueDecl *value = refExpr->getDecl();

  // There is a lot more to do here, but for now only EnumConstantDecl is
  // supported.
  assert(!cir::MissingFeatures::tryEmitAsConstant());

  // The value needs to be an enum constant or a constant variable.
  if (!isa<EnumConstantDecl>(value))
    return ConstantEmission();

  Expr::EvalResult result;
  if (!refExpr->EvaluateAsRValue(result, getContext()))
    return ConstantEmission();

  QualType resultType = refExpr->getType();

  // As long as we're only handling EnumConstantDecl, there should be no
  // side-effects.
  assert(!result.HasSideEffects);

  // Emit as a constant.
  // FIXME(cir): have emitAbstract build a TypedAttr instead (this requires
  // somewhat heavy refactoring...)
  mlir::Attribute c = ConstantEmitter(*this).emitAbstract(
      refExpr->getLocation(), result.Val, resultType);
  mlir::TypedAttr cstToEmit = mlir::dyn_cast_if_present<mlir::TypedAttr>(c);
  assert(cstToEmit && "expected a typed attribute");

  assert(!cir::MissingFeatures::generateDebugInfo());

  return ConstantEmission::forValue(cstToEmit);
}

CIRGenFunction::ConstantEmission
CIRGenFunction::tryEmitAsConstant(const MemberExpr *me) {
  if (DeclRefExpr *dre = tryToConvertMemberExprToDeclRefExpr(*this, me))
    return tryEmitAsConstant(dre);
  return ConstantEmission();
}

mlir::Value CIRGenFunction::emitScalarConstant(
    const CIRGenFunction::ConstantEmission &constant, Expr *e) {
  assert(constant && "not a constant");
  if (constant.isReference()) {
    cgm.errorNYI(e->getSourceRange(), "emitScalarConstant: reference");
    return {};
  }
  return builder.getConstant(getLoc(e->getSourceRange()), constant.getValue());
}

LValue CIRGenFunction::emitPredefinedLValue(const PredefinedExpr *e) {
  const StringLiteral *sl = e->getFunctionName();
  assert(sl != nullptr && "No StringLiteral name in PredefinedExpr");
  auto fn = cast<cir::FuncOp>(curFn);
  StringRef fnName = fn.getName();
  fnName.consume_front("\01");
  std::array<StringRef, 2> nameItems = {
      PredefinedExpr::getIdentKindName(e->getIdentKind()), fnName};
  std::string gvName = llvm::join(nameItems, ".");
  if (auto *bd = dyn_cast_or_null<BlockDecl>(curCodeDecl)) {
    std::string name = std::string(sl->getString());
    if (!name.empty()) {
      unsigned discriminator =
          cgm.getCXXABI().getMangleContext().getBlockId(bd, true);
      if (discriminator)
        name += "_" + Twine(discriminator + 1).str();
      StringLiteral *blockName = StringLiteral::Create(
          getContext(), name, sl->getKind(), sl->isPascal(),
          sl->getType(), sl->getBeginLoc());
      return emitStringLiteralLValue(blockName, gvName);
    }
  }

  return emitStringLiteralLValue(sl, gvName);
}

LValue CIRGenFunction::emitOpaqueValueLValue(const OpaqueValueExpr *e) {
  assert(OpaqueValueMappingData::shouldBindAsLValue(e));
  return getOrCreateOpaqueLValueMapping(e);
}

namespace {
// Handle the case where the condition is a constant evaluatable simple integer,
// which means we don't have to separately handle the true/false blocks.
std::optional<LValue> handleConditionalOperatorLValueSimpleCase(
    CIRGenFunction &cgf, const AbstractConditionalOperator *e) {
  const Expr *condExpr = e->getCond();
  llvm::APSInt condExprVal;
  if (!cgf.constantFoldsToSimpleInteger(condExpr, condExprVal))
    return std::nullopt;

  const Expr *live = e->getTrueExpr(), *dead = e->getFalseExpr();
  if (!condExprVal.getBoolValue())
    std::swap(live, dead);

  if (cgf.containsLabel(dead))
    return std::nullopt;

  // If the true case is live, we need to track its region.
  assert(!cir::MissingFeatures::incrementProfileCounter());
  assert(!cir::MissingFeatures::pgoUse());
  // If a throw expression we emit it and return an undefined lvalue
  // because it can't be used.
  if (auto *throwExpr = dyn_cast<CXXThrowExpr>(live->IgnoreParens())) {
    cgf.emitCXXThrowExpr(throwExpr);
    // Return an undefined lvalue - the throw terminates execution
    // so this value will never actually be used
    mlir::Type elemTy = cgf.convertType(dead->getType());
    mlir::Value undefPtr =
        cgf.getBuilder().getNullPtr(cgf.getBuilder().getPointerTo(elemTy),
                                    cgf.getLoc(throwExpr->getSourceRange()));
    return cgf.makeAddrLValue(Address(undefPtr, elemTy, CharUnits::One()),
                              dead->getType());
  }
  return cgf.emitLValue(live);
}

/// Emit the operand of a glvalue conditional operator. This is either a glvalue
/// or a (possibly-parenthesized) throw-expression. If this is a throw, no
/// LValue is returned and the current block has been terminated.
static std::optional<LValue> emitLValueOrThrowExpression(CIRGenFunction &cgf,
                                                         const Expr *operand) {
  if (auto *throwExpr = dyn_cast<CXXThrowExpr>(operand->IgnoreParens())) {
    cgf.emitCXXThrowExpr(throwExpr);
    return std::nullopt;
  }

  return cgf.emitLValue(operand);
}
} // namespace

// Create and generate the 3 blocks for a conditional operator.
// Leaves the 'current block' in the continuation basic block.
template <typename FuncTy>
CIRGenFunction::ConditionalInfo
CIRGenFunction::emitConditionalBlocks(const AbstractConditionalOperator *e,
                                      const FuncTy &branchGenFunc) {
  ConditionalInfo info;
  ConditionalEvaluation eval(*this);
  mlir::Location loc = getLoc(e->getSourceRange());
  CIRGenBuilderTy &builder = getBuilder();

  mlir::Value condV = emitOpOnBoolExpr(loc, e->getCond());
  SmallVector<mlir::OpBuilder::InsertPoint, 2> insertPoints{};
  mlir::Type yieldTy{};

  auto emitBranch = [&](mlir::OpBuilder &b, mlir::Location loc,
                        const Expr *expr, std::optional<LValue> &resultLV) {
    CIRGenFunction::LexicalScope lexScope{*this, loc, b.getInsertionBlock()};
    curLexScope->setAsTernary();

    assert(!cir::MissingFeatures::incrementProfileCounter());
    eval.beginEvaluation();
    resultLV = branchGenFunc(*this, expr);
    mlir::Value resultPtr = resultLV ? resultLV->getPointer() : mlir::Value();
    eval.endEvaluation();
    if (haveInsertPoint())
      lexScope.forceCleanup();

    if (resultPtr) {
      yieldTy = resultPtr.getType();
      cir::YieldOp::create(b, loc, resultPtr);
    } else {
      // If LHS or RHS is a void expression we need
      // to patch arms as to properly match yield types.
      // If the current block's terminator is an UnreachableOp (from a throw),
      // we don't need a yield
      if (builder.getInsertionBlock()->mightHaveTerminator()) {
        mlir::Operation *terminator =
            builder.getInsertionBlock()->getTerminator();
        if (isa_and_nonnull<cir::UnreachableOp>(terminator))
          insertPoints.push_back(b.saveInsertionPoint());
      }
    }
  };

  info.result = cir::TernaryOp::create(
                    builder, loc, condV,
                    /*trueBuilder=*/
                    [&](mlir::OpBuilder &b, mlir::Location loc) {
                      emitBranch(b, loc, e->getTrueExpr(), info.lhs);
                    },
                    /*falseBuilder=*/
                    [&](mlir::OpBuilder &b, mlir::Location loc) {
                      emitBranch(b, loc, e->getFalseExpr(), info.rhs);
                    })
                    .getResult();

  // If both arms are void, so be it.
  if (!yieldTy)
    yieldTy = voidTy;

  // Insert required yields.
  for (mlir::OpBuilder::InsertPoint &toInsert : insertPoints) {
    mlir::OpBuilder::InsertionGuard guard(builder);
    builder.restoreInsertionPoint(toInsert);

    // Block does not return: build empty yield.
    if (!yieldTy) {
      cir::YieldOp::create(builder, loc);
    } else { // Block returns: set null yield value.
      mlir::Value op0 = builder.getNullValue(yieldTy, loc);
      cir::YieldOp::create(builder, loc, op0);
    }
  }

  return info;
}

LValue CIRGenFunction::emitConditionalOperatorLValue(
    const AbstractConditionalOperator *expr) {
  if (!expr->isGLValue()) {
    // ?: here should be an aggregate.
    assert(hasAggregateEvaluationKind(expr->getType()) &&
           "Unexpected conditional operator!");
    return emitAggExprToLValue(expr);
  }

  OpaqueValueMapping binding(*this, expr);
  if (std::optional<LValue> res =
          handleConditionalOperatorLValueSimpleCase(*this, expr))
    return *res;

  ConditionalInfo info =
      emitConditionalBlocks(expr, [](CIRGenFunction &cgf, const Expr *e) {
        return emitLValueOrThrowExpression(cgf, e);
      });

  if ((info.lhs && !info.lhs->isSimple()) ||
      (info.rhs && !info.rhs->isSimple())) {
    cgm.errorNYI(expr->getSourceRange(),
                 "unsupported conditional operator with non-simple lvalue");
    return LValue();
  }

  if (info.lhs && info.rhs) {
    Address lhsAddr = info.lhs->getAddress();
    Address rhsAddr = info.rhs->getAddress();
    Address result(info.result, lhsAddr.getElementType(),
                   std::min(lhsAddr.getAlignment(), rhsAddr.getAlignment()));
    AlignmentSource alignSource =
        std::max(info.lhs->getBaseInfo().getAlignmentSource(),
                 info.rhs->getBaseInfo().getAlignmentSource());
    assert(!cir::MissingFeatures::opTBAA());
    return makeAddrLValue(result, expr->getType(), LValueBaseInfo(alignSource));
  }

  assert((info.lhs || info.rhs) &&
         "both operands of glvalue conditional are throw-expressions?");
  return info.lhs ? *info.lhs : *info.rhs;
}

/// An LValue is a candidate for having its loads and stores be made atomic if
/// we are operating under /volatile:ms *and* the LValue itself is volatile and
/// performing such an operation can be performed without a libcall.
bool CIRGenFunction::isLValueSuitableForInlineAtomic(LValue lv) {
  if (!cgm.getLangOpts().MSVolatile)
    return false;

  bool isVolatile = lv.isVolatile() || hasVolatileMember(lv.getType());
  if (!isVolatile)
    return false;

  if (getContext().getTypeSize(lv.getType()) >
      getContext().getTypeSize(getContext().getIntPtrType()))
    return false;

  return true;
}
