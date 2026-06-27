// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -fblocks -fclangir -emit-cir %s -o - | FileCheck %s

typedef void (^dispatch_block_t)(void);
void dispatch_once(long *, dispatch_block_t);

static int value;

void use_block(void) {
  static long once;
  dispatch_once(&once, ^{
    value = 1;
    (void)__FUNCTION__;
  });
}

// CHECK-DAG: cir.global "private" external @_NSConcreteStackBlock
// CHECK-DAG: cir.func internal private @__cir_block_invoke
// CHECK-DAG: cir.global "private" constant internal @__block_descriptor
// CHECK: cir.func {{.*}}@use_block
// CHECK: %[[BLOCK:.*]] = cir.alloca {{.*}} ["block"]
// CHECK: cir.get_member %[[BLOCK]][0] {name = "block.isa"}
// CHECK: cir.get_member %[[BLOCK]][3] {name = "block.invoke"}
// CHECK: cir.get_member %[[BLOCK]][4] {name = "block.descriptor"}
// CHECK: %[[OPAQUE_BLOCK:.*]] = cir.cast bitcast %[[BLOCK]]
// CHECK: cir.call @dispatch_once({{.*}}, %[[OPAQUE_BLOCK]]) : (!cir.ptr<!s64i>, !cir.ptr<!void>) -> ()
