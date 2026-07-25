// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-cir %s -o - | FileCheck %s -check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o - | FileCheck %s -check-prefix=LLVM
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o - | FileCheck %s -check-prefix=LLVM

int sync_val_cas_i32(int *ptr, int expected, int desired) {
  return __sync_val_compare_and_swap(ptr, expected, desired);
}

// CIR-LABEL: cir.func {{.*}} @sync_val_cas_i32
// CIR: %[[OLD32:.+]], %{{.+}} = cir.atomic.cmpxchg success(seq_cst) failure(seq_cst) syncscope(system) %{{.+}}, %{{.+}}, %{{.+}} align(4) : (!cir.ptr<!s32i>, !s32i, !s32i) -> (!s32i, !cir.bool)
// CIR: cir.store{{.*}} %[[OLD32]], %{{.+}} : !s32i, !cir.ptr<!s32i>
// LLVM-LABEL: define{{.*}} i32 @sync_val_cas_i32
// LLVM: %[[VAL_PAIR32:.+]] = cmpxchg ptr %{{.+}}, i32 %{{.+}}, i32 %{{.+}} seq_cst seq_cst, align 4
// LLVM: extractvalue { i32, i1 } %[[VAL_PAIR32]], 0

long sync_val_cas_i64(long *ptr, long expected, long desired) {
  return __sync_val_compare_and_swap(ptr, expected, desired);
}

// CIR-LABEL: cir.func {{.*}} @sync_val_cas_i64
// CIR: %[[OLD64:.+]], %{{.+}} = cir.atomic.cmpxchg success(seq_cst) failure(seq_cst) syncscope(system) %{{.+}}, %{{.+}}, %{{.+}} align(8) : (!cir.ptr<!s64i>, !s64i, !s64i) -> (!s64i, !cir.bool)
// CIR: cir.store{{.*}} %[[OLD64]], %{{.+}} : !s64i, !cir.ptr<!s64i>
// LLVM-LABEL: define{{.*}} i64 @sync_val_cas_i64
// LLVM: %[[VAL_PAIR64:.+]] = cmpxchg ptr %{{.+}}, i64 %{{.+}}, i64 %{{.+}} seq_cst seq_cst, align 8
// LLVM: extractvalue { i64, i1 } %[[VAL_PAIR64]], 0

int sync_bool_cas_i32(int *ptr, int expected, int desired) {
  return __sync_bool_compare_and_swap(ptr, expected, desired);
}

// CIR-LABEL: cir.func {{.*}} @sync_bool_cas_i32
// CIR: %{{.+}}, %[[SUCCESS32:.+]] = cir.atomic.cmpxchg success(seq_cst) failure(seq_cst) syncscope(system) %{{.+}}, %{{.+}}, %{{.+}} align(4) : (!cir.ptr<!s32i>, !s32i, !s32i) -> (!s32i, !cir.bool)
// CIR: %[[RESULT32:.+]] = cir.cast bool_to_int %[[SUCCESS32]] : !cir.bool -> !s32i
// CIR: cir.store{{.*}} %[[RESULT32]], %{{.+}} : !s32i, !cir.ptr<!s32i>
// LLVM-LABEL: define{{.*}} i32 @sync_bool_cas_i32
// LLVM: %[[BOOL_PAIR32:.+]] = cmpxchg ptr %{{.+}}, i32 %{{.+}}, i32 %{{.+}} seq_cst seq_cst, align 4
// LLVM: %[[SUCCESS32:.+]] = extractvalue { i32, i1 } %[[BOOL_PAIR32]], 1
// LLVM: zext i1 %[[SUCCESS32]] to i32

int sync_bool_cas_i64(long *ptr, long expected, long desired) {
  return __sync_bool_compare_and_swap(ptr, expected, desired);
}

// CIR-LABEL: cir.func {{.*}} @sync_bool_cas_i64
// CIR: %{{.+}}, %[[SUCCESS64:.+]] = cir.atomic.cmpxchg success(seq_cst) failure(seq_cst) syncscope(system) %{{.+}}, %{{.+}}, %{{.+}} align(8) : (!cir.ptr<!s64i>, !s64i, !s64i) -> (!s64i, !cir.bool)
// CIR: %[[RESULT64:.+]] = cir.cast bool_to_int %[[SUCCESS64]] : !cir.bool -> !s32i
// CIR: cir.store{{.*}} %[[RESULT64]], %{{.+}} : !s32i, !cir.ptr<!s32i>
// LLVM-LABEL: define{{.*}} i32 @sync_bool_cas_i64
// LLVM: %[[BOOL_PAIR64:.+]] = cmpxchg ptr %{{.+}}, i64 %{{.+}}, i64 %{{.+}} seq_cst seq_cst, align 8
// LLVM: %[[SUCCESS64:.+]] = extractvalue { i64, i1 } %[[BOOL_PAIR64]], 1
// LLVM: zext i1 %[[SUCCESS64]] to i32
