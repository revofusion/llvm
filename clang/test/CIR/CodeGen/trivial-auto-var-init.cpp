// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -ftrivial-auto-var-init=zero %s -o - | FileCheck %s --check-prefix=CIR-ZERO
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -ftrivial-auto-var-init=pattern %s -o - | FileCheck %s --check-prefix=CIR-PATTERN
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm -ftrivial-auto-var-init=zero %s -o - | FileCheck %s --check-prefix=LLVM-ZERO
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm -ftrivial-auto-var-init=pattern %s -o - | FileCheck %s --check-prefix=LLVM-PATTERN
// RUN: %clang_cc1 -std=c++20 -triple i386-unknown-linux-gnu -fclangir -emit-cir -ftrivial-auto-var-init=pattern %s -o - | FileCheck %s --check-prefix=CIR-PATTERN32
// RUN: %clang_cc1 -std=c++20 -triple i386-unknown-linux-gnu -fclangir -emit-llvm -ftrivial-auto-var-init=pattern %s -o - | FileCheck %s --check-prefix=LLVM-PATTERN32

struct Pair {
  int first;
  int second;
};

using Vec = int __attribute__((ext_vector_type(4)));

int scalar(unsigned long size) {
  return *static_cast<int *>(__builtin_alloca(size));
}

Pair aggregate() {
  return *static_cast<Pair *>(__builtin_alloca(sizeof(Pair)));
}

Vec vector() {
  return *static_cast<Vec *>(__builtin_alloca(sizeof(Vec)));
}

// CIR-ZERO-LABEL: cir.func {{.*}}@_Z6scalarm
// CIR-ZERO: %[[ZERO_SCALAR_ALLOCA:.*]] = cir.alloca "bi_alloca" align(16) size(%[[ZERO_SCALAR_SIZE:.*]]) : !cir.ptr<!u8i>
// CIR-ZERO: %[[ZERO_SCALAR_BYTE:.*]] = cir.const #cir.int<0> : !u8i
// CIR-ZERO: %[[ZERO_SCALAR_VOID:.*]] = cir.cast bitcast %[[ZERO_SCALAR_ALLOCA]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR-ZERO: cir.libc.memset %[[ZERO_SCALAR_SIZE]] bytes at %[[ZERO_SCALAR_VOID]] align(16) to %[[ZERO_SCALAR_BYTE]] : !cir.ptr<!void>, !u8i, !u64i
// CIR-ZERO-LABEL: cir.func {{.*}}@_Z9aggregatev
// CIR-ZERO: %[[ZERO_AGGREGATE_BYTE:.*]] = cir.const #cir.int<0> : !u8i
// CIR-ZERO: cir.libc.memset {{.*}} align(16) to %[[ZERO_AGGREGATE_BYTE]]
// CIR-ZERO-LABEL: cir.func {{.*}}@_Z6vectorv
// CIR-ZERO: %[[ZERO_VECTOR_BYTE:.*]] = cir.const #cir.int<0> : !u8i
// CIR-ZERO: cir.libc.memset {{.*}} align(16) to %[[ZERO_VECTOR_BYTE]]

// CIR-PATTERN-LABEL: cir.func {{.*}}@_Z6scalarm
// CIR-PATTERN: %[[PATTERN_SCALAR_ALLOCA:.*]] = cir.alloca "bi_alloca" align(16) size(%[[PATTERN_SCALAR_SIZE:.*]]) : !cir.ptr<!u8i>
// CIR-PATTERN: %[[PATTERN_SCALAR_BYTE:.*]] = cir.const #cir.int<170> : !u8i
// CIR-PATTERN: %[[PATTERN_SCALAR_VOID:.*]] = cir.cast bitcast %[[PATTERN_SCALAR_ALLOCA]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR-PATTERN: cir.libc.memset %[[PATTERN_SCALAR_SIZE]] bytes at %[[PATTERN_SCALAR_VOID]] align(16) to %[[PATTERN_SCALAR_BYTE]] : !cir.ptr<!void>, !u8i, !u64i
// CIR-PATTERN-LABEL: cir.func {{.*}}@_Z9aggregatev
// CIR-PATTERN: %[[PATTERN_AGGREGATE_BYTE:.*]] = cir.const #cir.int<170> : !u8i
// CIR-PATTERN: cir.libc.memset {{.*}} align(16) to %[[PATTERN_AGGREGATE_BYTE]]
// CIR-PATTERN-LABEL: cir.func {{.*}}@_Z6vectorv
// CIR-PATTERN: %[[PATTERN_VECTOR_BYTE:.*]] = cir.const #cir.int<170> : !u8i
// CIR-PATTERN: cir.libc.memset {{.*}} align(16) to %[[PATTERN_VECTOR_BYTE]]

// LLVM-ZERO-LABEL: define {{.*}}i32 @_Z6scalarm
// LLVM-ZERO: %[[ZERO_SCALAR_ALLOCA:.*]] = alloca i8, i64 %[[ZERO_SCALAR_SIZE:.*]], align 16
// LLVM-ZERO: call void @llvm.memset.p0.i64(ptr align 16 %[[ZERO_SCALAR_ALLOCA]], i8 0, i64 %[[ZERO_SCALAR_SIZE]], i1 false)
// LLVM-ZERO-LABEL: define {{.*}} @_Z9aggregatev
// LLVM-ZERO: call void @llvm.memset.p0.i64({{.*}} i8 0, i64 8, i1 false)
// LLVM-ZERO-LABEL: define {{.*}}<4 x i32> @_Z6vectorv
// LLVM-ZERO: call void @llvm.memset.p0.i64({{.*}} i8 0, i64 16, i1 false)

// LLVM-PATTERN-LABEL: define {{.*}}i32 @_Z6scalarm
// LLVM-PATTERN: %[[PATTERN_SCALAR_ALLOCA:.*]] = alloca i8, i64 %[[PATTERN_SCALAR_SIZE:.*]], align 16
// LLVM-PATTERN: call void @llvm.memset.p0.i64(ptr align 16 %[[PATTERN_SCALAR_ALLOCA]], i8 -86, i64 %[[PATTERN_SCALAR_SIZE]], i1 false)
// LLVM-PATTERN-LABEL: define {{.*}} @_Z9aggregatev
// LLVM-PATTERN: call void @llvm.memset.p0.i64({{.*}} i8 -86, i64 8, i1 false)
// LLVM-PATTERN-LABEL: define {{.*}}<4 x i32> @_Z6vectorv
// LLVM-PATTERN: call void @llvm.memset.p0.i64({{.*}} i8 -86, i64 16, i1 false)

// CIR-PATTERN32-LABEL: cir.func {{.*}}@_Z6scalarm
// CIR-PATTERN32: %[[PATTERN32_SCALAR_BYTE:.*]] = cir.const #cir.int<255> : !u8i
// CIR-PATTERN32: cir.libc.memset {{.*}} to %[[PATTERN32_SCALAR_BYTE]]

// LLVM-PATTERN32-LABEL: define {{.*}}i32 @_Z6scalarm
// LLVM-PATTERN32: call void @llvm.memset.p0.i32({{.*}} i8 -1, i32 {{.*}}, i1 false)
