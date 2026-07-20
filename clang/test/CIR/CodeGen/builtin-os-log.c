// RUN: %clang_cc1 -triple arm64-apple-macosx -fclangir -emit-cir %s -o - | FileCheck %s
// RUN: %clang_cc1 -triple arm64-apple-macosx -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=CIRLLVM
// RUN: %clang_cc1 -triple arm64-apple-macosx -emit-llvm %s -o - | FileCheck %s --check-prefix=OGCG

void test_builtin_os_log(void *buffer, int value, const char *text) {
  __builtin_os_log_format(buffer, "%d %{public}s", value, text);
}

// CHECK-LABEL: cir.func no_inline @test_builtin_os_log(
// CHECK: %[[BUFFER:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// CHECK: %[[BYTES:.*]] = cir.cast bitcast %[[BUFFER]] : !cir.ptr<!void> -> !cir.ptr<!u8i>
// CHECK: %[[SUMMARY:.*]] = cir.const #cir.int<2> : !u8i
// CHECK: cir.store align(1) %[[SUMMARY]], {{.*}} : !u8i, !cir.ptr<!u8i>
// CHECK: %[[COUNT:.*]] = cir.const #cir.int<2> : !u8i
// CHECK: cir.store align(1) %[[COUNT]], {{.*}} : !u8i, !cir.ptr<!u8i>
// CHECK: cir.cast ptr_to_int {{.*}} : !cir.ptr<!s8i> -> !u64i
// CHECK-NOT: cir.call @__builtin_os_log_format
// CHECK: cir.return

// CIRLLVM-LABEL: define void @test_builtin_os_log(
// CIRLLVM: store i8 2, ptr {{.*}}, align 1
// CIRLLVM: store i8 2, ptr {{.*}}, align 1
// CIRLLVM: store i32 {{.*}}, ptr {{.*}}, align 1
// CIRLLVM: ptrtoint ptr {{.*}} to i64
// CIRLLVM: store i64 {{.*}}, ptr {{.*}}, align 1
// CIRLLVM-NOT: call void @__builtin_os_log_format
// CIRLLVM: ret void

// OGCG-LABEL: define void @test_builtin_os_log(
// OGCG: ptrtoint ptr {{.*}} to i64
// OGCG: call void @__os_log_helper_
// OGCG: ret void
// OGCG-LABEL: define linkonce_odr hidden void @__os_log_helper_
// OGCG: store i8 2, ptr {{.*}}, align 1
// OGCG: store i8 2, ptr {{.*}}, align 1
// OGCG: store i32 {{.*}}, ptr {{.*}}, align 1
// OGCG: store i64 {{.*}}, ptr {{.*}}, align 1
// OGCG: ret void
