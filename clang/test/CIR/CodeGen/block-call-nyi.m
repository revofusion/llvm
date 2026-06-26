// RUN: %clang_cc1 -triple arm64-apple-macosx -fclangir -emit-cir -fblocks %s -o - | FileCheck %s

void takes_pointer(void *);

void block_argument_call(void) {
  takes_pointer((void *)^{});
}

// CHECK: cir.global{{.*}} @__block_descriptor
// CHECK: cir.global{{.*}} @_NSConcreteStackBlock
// CHECK: cir.func internal private @__cir_block_invoke
// CHECK: cir.get_member {{.*}} {name = "block.isa"}
// CHECK: cir.get_member {{.*}} {name = "block.invoke"}
// CHECK: cir.get_member {{.*}} {name = "block.descriptor"}
