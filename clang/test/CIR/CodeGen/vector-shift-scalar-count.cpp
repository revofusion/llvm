// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=LLVM

typedef unsigned long v2u64 __attribute__((vector_size(16)));
typedef unsigned long v4u64 __attribute__((vector_size(32)));

extern "C" v2u64 rotate_v2(v2u64 value, int shift) {
  return (value << shift) | (value >> ((-shift) & 63));
}

// CIR-LABEL: cir.func{{.*}} @rotate_v2
// CIR: %[[LEFT_COUNT:.*]] = cir.cast integral %{{.*}} : !s32i -> !u64i
// CIR: %[[LEFT_SPLAT:.*]] = cir.vec.splat %[[LEFT_COUNT]] : !u64i, !cir.vector<2 x !u64i>
// CIR: cir.shift(left, %{{.*}} : !cir.vector<2 x !u64i>, %[[LEFT_SPLAT]] : !cir.vector<2 x !u64i>) -> !cir.vector<2 x !u64i>
// CIR: %[[RIGHT_COUNT:.*]] = cir.cast integral %{{.*}} : !s32i -> !u64i
// CIR: %[[RIGHT_SPLAT:.*]] = cir.vec.splat %[[RIGHT_COUNT]] : !u64i, !cir.vector<2 x !u64i>
// CIR: cir.shift(right, %{{.*}} : !cir.vector<2 x !u64i>, %[[RIGHT_SPLAT]] : !cir.vector<2 x !u64i>) -> !cir.vector<2 x !u64i>

// LLVM-LABEL: define{{.*}} <2 x i64> @rotate_v2
// LLVM: shl <2 x i64> %{{.*}}, %{{.*}}
// LLVM: lshr <2 x i64> %{{.*}}, %{{.*}}

extern "C" v4u64 rotate_v4(v4u64 value, int shift) {
  return (value << shift) | (value >> ((-shift) & 63));
}

// CIR-LABEL: cir.func{{.*}} @rotate_v4
// CIR: %[[LEFT_COUNT:.*]] = cir.cast integral %{{.*}} : !s32i -> !u64i
// CIR: %[[LEFT_SPLAT:.*]] = cir.vec.splat %[[LEFT_COUNT]] : !u64i, !cir.vector<4 x !u64i>
// CIR: cir.shift(left, %{{.*}} : !cir.vector<4 x !u64i>, %[[LEFT_SPLAT]] : !cir.vector<4 x !u64i>) -> !cir.vector<4 x !u64i>
// CIR: %[[RIGHT_COUNT:.*]] = cir.cast integral %{{.*}} : !s32i -> !u64i
// CIR: %[[RIGHT_SPLAT:.*]] = cir.vec.splat %[[RIGHT_COUNT]] : !u64i, !cir.vector<4 x !u64i>
// CIR: cir.shift(right, %{{.*}} : !cir.vector<4 x !u64i>, %[[RIGHT_SPLAT]] : !cir.vector<4 x !u64i>) -> !cir.vector<4 x !u64i>

// LLVM-LABEL: define{{.*}} <4 x i64> @rotate_v4
// LLVM: shl <4 x i64> %{{.*}}, %{{.*}}
// LLVM: lshr <4 x i64> %{{.*}}, %{{.*}}

extern "C" unsigned long scalar_rotate(unsigned long value, int shift) {
  return (value << shift) | (value >> ((-shift) & 63));
}

// CIR-LABEL: cir.func{{.*}} @scalar_rotate
// CIR: cir.shift(left, %{{.*}} : !u64i, %{{.*}} : !s32i) -> !u64i
// CIR: cir.shift(right, %{{.*}} : !u64i, %{{.*}} : !s32i) -> !u64i

// LLVM-LABEL: define{{.*}} i64 @scalar_rotate
// LLVM: shl i64 %{{.*}}, %{{.*}}
// LLVM: lshr i64 %{{.*}}, %{{.*}}
