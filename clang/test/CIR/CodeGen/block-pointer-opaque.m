// RUN: %clang_cc1 -triple arm64-apple-macosx -fclangir -emit-cir -fblocks -o - %s | FileCheck %s

void takes_block(void (^block)(void));

void forward_block(void (^block)(void)) {
  takes_block(block);
}

// CHECK-LABEL: cir.func{{.*}} @forward_block(
// CHECK-SAME: %[[BLOCK:.*]]: !cir.ptr<!void>
// CHECK: cir.store %[[BLOCK]],
// CHECK: %[[LOADED:.*]] = cir.load
// CHECK: cir.call @takes_block(%[[LOADED]]) : (!cir.ptr<!void>) -> ()
