// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t.ll %s

void scalar(int n) {
  new int[n]();
}

// CIR-LABEL: cir.func{{.*}} @_Z6scalari(
// CIR:         %[[N:.*]] = cir.load{{.*}} : !cir.ptr<!s32i>, !s32i
// CIR:         %[[COUNT:.*]] = cir.cast integral %[[N]] : !s32i -> !u64i
// CIR:         %[[ELEMENT_SIZE:.*]] = cir.const #cir.int<4> : !u64i
// CIR:         %[[BYTE_COUNT:.*]], %[[OVERFLOW:.*]] = cir.mul.overflow %[[COUNT]], %[[ELEMENT_SIZE]] : !u64i -> !u64i
// CIR:         %[[ALLOC_SIZE:.*]] = cir.select if %[[OVERFLOW]] then %{{.*}} else %[[BYTE_COUNT]] : (!cir.bool, !u64i, !u64i) -> !u64i
// CIR:         %[[RAW:.*]] = cir.call @_Znam(%[[ALLOC_SIZE]])
// CIR:         %[[BEGIN:.*]] = cir.cast bitcast %[[RAW]] : !cir.ptr<!void> -> !cir.ptr<!s32i>
// CIR:         %[[AS_VOID:.*]] = cir.cast bitcast %[[BEGIN]] : !cir.ptr<!s32i> -> !cir.ptr<!void>
// CIR:         %[[ZERO:.*]] = cir.const #cir.int<0> : !u8i
// CIR:         cir.libc.memset %[[ALLOC_SIZE]] bytes at %[[AS_VOID]] to %[[ZERO]] : !cir.ptr<!void>, !u8i, !u64i

// LLVM-LABEL: define{{.*}} void @_Z6scalari(
// LLVM:         %[[MUL:.*]] = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %{{.*}}, i64 4)
// LLVM-DAG:     %[[BYTE_COUNT:.*]] = extractvalue { i64, i1 } %[[MUL]], 0
// LLVM-DAG:     %[[OVERFLOW:.*]] = extractvalue { i64, i1 } %[[MUL]], 1
// LLVM:         %[[ALLOC_SIZE:.*]] = select i1 %[[OVERFLOW]], i64 -1, i64 %[[BYTE_COUNT]]
// LLVM:         %[[RAW:.*]] = call{{.*}} ptr @_Znam(i64 noundef %[[ALLOC_SIZE]])
// LLVM:         call void @llvm.memset.p0.i64(ptr{{.*}} %[[RAW]], i8 0, i64 %[[ALLOC_SIZE]], i1 false)

struct TrivialAggregate {
  TrivialAggregate() = default;
  int first;
  int second;
};

void aggregate(int n) {
  new TrivialAggregate[n]();
}

// CIR-LABEL: cir.func{{.*}} @_Z9aggregatei(
// CIR:         %[[N:.*]] = cir.load{{.*}} : !cir.ptr<!s32i>, !s32i
// CIR:         %[[COUNT:.*]] = cir.cast integral %[[N]] : !s32i -> !u64i
// CIR:         %[[ELEMENT_SIZE:.*]] = cir.const #cir.int<8> : !u64i
// CIR:         %[[BYTE_COUNT:.*]], %[[OVERFLOW:.*]] = cir.mul.overflow %[[COUNT]], %[[ELEMENT_SIZE]] : !u64i -> !u64i
// CIR:         %[[ALLOC_SIZE:.*]] = cir.select if %[[OVERFLOW]] then %{{.*}} else %[[BYTE_COUNT]] : (!cir.bool, !u64i, !u64i) -> !u64i
// CIR:         %[[RAW:.*]] = cir.call @_Znam(%[[ALLOC_SIZE]])
// CIR:         %[[BEGIN:.*]] = cir.cast bitcast %[[RAW]] : !cir.ptr<!void> -> !cir.ptr<!rec_TrivialAggregate>
// CIR:         %[[AS_VOID:.*]] = cir.cast bitcast %[[BEGIN]] : !cir.ptr<!rec_TrivialAggregate> -> !cir.ptr<!void>
// CIR:         %[[ZERO:.*]] = cir.const #cir.int<0> : !u8i
// CIR:         cir.libc.memset %[[ALLOC_SIZE]] bytes at %[[AS_VOID]] to %[[ZERO]] : !cir.ptr<!void>, !u8i, !u64i

// LLVM-LABEL: define{{.*}} void @_Z9aggregatei(
// LLVM:         %[[MUL:.*]] = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %{{.*}}, i64 8)
// LLVM-DAG:     %[[BYTE_COUNT:.*]] = extractvalue { i64, i1 } %[[MUL]], 0
// LLVM-DAG:     %[[OVERFLOW:.*]] = extractvalue { i64, i1 } %[[MUL]], 1
// LLVM:         %[[ALLOC_SIZE:.*]] = select i1 %[[OVERFLOW]], i64 -1, i64 %[[BYTE_COUNT]]
// LLVM:         %[[RAW:.*]] = call{{.*}} ptr @_Znam(i64 noundef %[[ALLOC_SIZE]])
// LLVM:         call void @llvm.memset.p0.i64(ptr{{.*}} %[[RAW]], i8 0, i64 %[[ALLOC_SIZE]], i1 false)

void aggregate_const() {
  new TrivialAggregate[3]();
}

// CIR-LABEL: cir.func{{.*}} @_Z15aggregate_constv(
// CIR:         %[[ALLOC_SIZE:.*]] = cir.const #cir.int<24> : !u64i
// CIR:         %[[RAW:.*]] = cir.call @_Znam(%[[ALLOC_SIZE]])
// CIR:         %[[BEGIN:.*]] = cir.cast bitcast %[[RAW]] : !cir.ptr<!void> -> !cir.ptr<!rec_TrivialAggregate>
// CIR:         %[[AS_VOID:.*]] = cir.cast bitcast %[[BEGIN]] : !cir.ptr<!rec_TrivialAggregate> -> !cir.ptr<!void>
// CIR:         %[[ZERO:.*]] = cir.const #cir.int<0> : !u8i
// CIR:         cir.libc.memset %[[ALLOC_SIZE]] bytes at %[[AS_VOID]] to %[[ZERO]] : !cir.ptr<!void>, !u8i, !u64i

// LLVM-LABEL: define{{.*}} void @_Z15aggregate_constv(
// LLVM:         %[[RAW:.*]] = call{{.*}} ptr @_Znam(i64 noundef 24)
// LLVM:         call void @llvm.memset.p0.i64(ptr{{.*}} %[[RAW]], i8 0, i64 24, i1 false)
