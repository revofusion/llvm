// RUN: %clang_cc1 -triple arm64-apple-macosx -fclangir -emit-cir -fblocks %s -o - | FileCheck %s

void takes_pointer(void *);
void use_string(const char *);

void predefined_in_block(void) {
  takes_pointer((void *)^{ use_string(__FUNCTION__); });
}

// CHECK: cir.global{{.*}} @__FUNCTION__.__cir_block_invoke
// CHECK: #cir.const_array<"predefined_in_block_block_invoke\00"
// CHECK: cir.func internal private @__cir_block_invoke
// CHECK: cir.call @use_string
