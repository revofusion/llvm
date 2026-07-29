//===---------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#include "clang/CIR/Interfaces/CIRLoopOpInterface.h"

#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "clang/CIR/Interfaces/CIRLoopOpInterface.cpp.inc"
#include "llvm/Support/ErrorHandling.h"

namespace cir {

void LoopOpInterface::getLoopOpSuccessorRegions(
    LoopOpInterface op, mlir::RegionBranchPoint point,
    llvm::SmallVectorImpl<mlir::RegionSuccessor> &regions) {
  assert(point.isParent() || point.getTerminatorPredecessorOrNull());

  // Branching to first region: go to condition or body (do-while).
  if (point.isParent()) {
    regions.emplace_back(&op.getEntry());
    return;
  }

  mlir::Region *parentRegion =
      point.getTerminatorPredecessorOrNull()->getParentRegion();
  mlir::Region *step = op.maybeGetStep();
  mlir::Region *cleanup = op.maybeGetCleanup();

  // The false condition edge, the normal end of the body, and the normal end
  // of a step all route through a per-evaluation cleanup when one is present.
  if (&op.getCond() == parentRegion) {
    if (cleanup)
      regions.emplace_back(cleanup);
    else
      regions.emplace_back(op);
    regions.emplace_back(&op.getBody());
    return;
  }

  if (&op.getBody() == parentRegion) {
    if (step)
      regions.emplace_back(step);
    else if (cleanup)
      regions.emplace_back(cleanup);
    else
      regions.emplace_back(&op.getCond());
    return;
  }

  if (step == parentRegion) {
    regions.emplace_back(cleanup ? cleanup : &op.getCond());
    return;
  }

  if (cleanup == parentRegion) {
    regions.emplace_back(&op.getCond());
    regions.emplace_back(op);
    return;
  }

  llvm_unreachable("unexpected branch origin");
}

mlir::ValueRange
LoopOpInterface::getLoopOpSuccessorInputs(LoopOpInterface op,
                                          mlir::RegionSuccessor successor) {
  if (successor.isOperation())
    return op->getResults();
  if (mlir::Region *region = successor.getSuccessor())
    return region->getArguments();
  llvm_unreachable("invalid region successor");
}

/// Verify invariants of the LoopOpInterface.
llvm::LogicalResult detail::verifyLoopOpInterface(mlir::Operation *op) {
  // FIXME: fix this so the conditionop isn't requiring MLIRCIR
  // auto loopOp = mlir::cast<LoopOpInterface>(op);
  // if (!mlir::isa<ConditionOp>(loopOp.getCond().back().getTerminator()))
  //   return op->emitOpError(
  //       "expected condition region to terminate with 'cir.condition'");
  return llvm::success();
}

} // namespace cir
