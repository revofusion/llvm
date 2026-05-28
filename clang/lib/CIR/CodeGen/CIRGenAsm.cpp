//===--- CIRGenAsm.cpp - Inline Assembly Support for CIR CodeGen ---------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file contains code to emit inline assembly.
//
//===----------------------------------------------------------------------===//

#include "CIRGenFunction.h"
#include "clang/CIR/MissingFeatures.h"

using namespace clang;
using namespace clang::CIRGen;
using namespace cir;

static AsmFlavor inferFlavor(const CIRGenModule &cgm, const AsmStmt &s) {
  AsmFlavor gnuAsmFlavor =
      cgm.getCodeGenOpts().getInlineAsmDialect() == CodeGenOptions::IAD_ATT
          ? AsmFlavor::x86_att
          : AsmFlavor::x86_intel;

  return isa<MSAsmStmt>(&s) ? AsmFlavor::x86_intel : gnuAsmFlavor;
}

static void collectClobbers(const CIRGenFunction &cgf, const AsmStmt &s,
                            std::string &constraints, bool &hasUnwindClobber,
                            bool &readOnly, bool readNone) {

  hasUnwindClobber = false;
  const CIRGenModule &cgm = cgf.getCIRGenModule();

  // Clobbers
  for (unsigned i = 0, e = s.getNumClobbers(); i != e; i++) {
    std::string clobber = s.getClobber(i);
    if (clobber == "memory") {
      readOnly = readNone = false;
    } else if (clobber == "unwind") {
      hasUnwindClobber = true;
      continue;
    } else if (clobber != "cc") {
      clobber = cgf.getTarget().getNormalizedGCCRegisterName(clobber);
      if (cgm.getCodeGenOpts().StackClashProtector &&
          cgf.getTarget().isSPRegName(clobber))
        cgm.getDiags().Report(s.getAsmLoc(),
                              diag::warn_stack_clash_protection_inline_asm);
    }

    if (isa<MSAsmStmt>(&s)) {
      if (clobber == "eax" || clobber == "edx") {
        if (constraints.find("=&A") != std::string::npos)
          continue;
        std::string::size_type position1 =
            constraints.find("={" + clobber + "}");
        if (position1 != std::string::npos) {
          constraints.insert(position1 + 1, "&");
          continue;
        }
        std::string::size_type position2 = constraints.find("=A");
        if (position2 != std::string::npos) {
          constraints.insert(position2 + 1, "&");
          continue;
        }
      }
    }
    if (!constraints.empty())
      constraints += ',';

    constraints += "~{";
    constraints += clobber;
    constraints += '}';
  }

  // Add machine specific clobbers
  std::string_view machineClobbers = cgf.getTarget().getClobbers();
  if (!machineClobbers.empty()) {
    if (!constraints.empty())
      constraints += ',';
    constraints += machineClobbers;
  }
}

mlir::LogicalResult CIRGenFunction::emitAsmStmt(const AsmStmt &s) {
  // Assemble the final asm string.
  std::string asmString = s.generateAsmString(getContext());

  bool isGCCAsmGoto = false;
  if (const auto *gccAsm = dyn_cast<GCCAsmStmt>(&s))
    isGCCAsmGoto = gccAsm->isAsmGoto();

  std::string constraints;
  std::vector<mlir::Value> outArgs;
  std::vector<mlir::Value> inArgs;
  std::vector<mlir::Value> inOutArgs;
  llvm::SmallVector<mlir::Attribute> operandAttrs;
  llvm::SmallVector<LValue> resultStores;
  llvm::SmallVector<mlir::Type> resultTypes;

  // An inline asm can be marked readonly if it meets the following conditions:
  //  - it doesn't have any sideeffects
  //  - it doesn't clobber memory
  //  - it doesn't return a value by-reference
  // It can be marked readnone if it doesn't have any input memory constraints
  // in addition to meeting the conditions listed above.
  bool readOnly = true, readNone = true;

  auto appendConstraint = [&](llvm::StringRef constraint) {
    if (!constraints.empty())
      constraints += ',';
    constraints += constraint;
  };

  mlir::Type resultType;
  for (unsigned i = 0, e = s.getNumOutputs(); i != e; ++i) {
    std::string constraint = s.getOutputConstraint(i);
    appendConstraint(constraint);

    const Expr *outExpr = s.getOutputExpr(i);
    LValue outLV = emitLValue(outExpr);
    bool isReadWrite = s.isOutputPlusConstraint(i);
    bool isMemory = constraint.find('m') != std::string::npos ||
                    constraint.find('o') != std::string::npos ||
                    constraint.find('V') != std::string::npos;

    if (isMemory) {
      outArgs.push_back(outLV.getPointer());
      operandAttrs.push_back(builder.getUnitAttr());
      continue;
    }

    if (isReadWrite) {
      mlir::Value inOut =
          emitLoadOfLValue(outLV, outExpr->getExprLoc()).getValue();
      inOutArgs.push_back(inOut);
      resultTypes.push_back(inOut.getType());
    } else {
      resultTypes.push_back(convertType(outExpr->getType()));
    }
    resultStores.push_back(outLV);
  }

  if (resultTypes.size() == 1)
    resultType = resultTypes.front();
  else if (resultTypes.size() > 1)
    resultType = builder.getAnonRecordTy(resultTypes);

  for (unsigned i = 0, e = s.getNumInputs(); i != e; ++i) {
    std::string constraint = s.getInputConstraint(i);
    appendConstraint(constraint);
    mlir::Value input = emitScalarExpr(s.getInputExpr(i));
    inArgs.push_back(input);
  }

  operandAttrs.resize(outArgs.size() + inArgs.size() + inOutArgs.size(),
                      mlir::Attribute());

  bool hasUnwindClobber = false;
  collectClobbers(*this, s, constraints, hasUnwindClobber, readOnly, readNone);

  std::array<mlir::ValueRange, 3> operands = {outArgs, inArgs, inOutArgs};

  bool hasSideEffect = s.isVolatile() || s.getNumOutputs() == 0;

  cir::InlineAsmOp ia = cir::InlineAsmOp::create(
      builder, getLoc(s.getAsmLoc()), resultType, operands, asmString,
      constraints, hasSideEffect, inferFlavor(cgm, s), mlir::ArrayAttr());

  if (isGCCAsmGoto) {
    assert(!cir::MissingFeatures::asmGoto());
  } else if (hasUnwindClobber) {
    assert(!cir::MissingFeatures::asmUnwindClobber());
  } else {
    assert(!cir::MissingFeatures::asmMemoryEffects());
  }

  ia.setOperandAttrsAttr(builder.getArrayAttr(operandAttrs));

  if (!resultStores.empty()) {
    mlir::Value result = ia.getRes();
    if (!result) {
      cgm.errorNYI(s.getAsmLoc(), "asm register output without result");
      return mlir::failure();
    }
    mlir::Location loc = getLoc(s.getAsmLoc());
    if (resultStores.size() == 1) {
      emitStoreThroughLValue(RValue::get(result), resultStores.front());
    } else {
      for (auto [index, resultStore] : llvm::enumerate(resultStores)) {
        auto member =
            cir::ExtractMemberOp::create(builder, loc, result, index).getResult();
        emitStoreThroughLValue(RValue::get(member), resultStore);
      }
    }
  }

  return mlir::success();
}
