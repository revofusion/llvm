// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512vl -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512vl -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef long long v2di __attribute__((vector_size(16)));
typedef long long v4di __attribute__((vector_size(32)));
typedef long long v8di __attribute__((vector_size(64)));

v2di test_pternlogq128_mask(v2di a, v2di b, v2di c, unsigned char mask) {
  return __builtin_ia32_pternlogq128_mask(a, b, c, 150, mask);
}

// CIR-LABEL: @test_pternlogq128_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<2 x !s64i>>, !cir.vector<2 x !s64i>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<2 x !s64i>>, !cir.vector<2 x !s64i>
// CIR:         %[[C:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<2 x !s64i>>, !cir.vector<2 x !s64i>
// CIR:         %[[IMM:.*]] = cir.const #cir.int<150> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[TERN:.*]] = cir.call_llvm_intrinsic "x86.avx512.pternlog.q.128" %[[A]], %[[B]], %[[C]], %[[IMM]] : (!cir.vector<2 x !s64i>, !cir.vector<2 x !s64i>, !cir.vector<2 x !s64i>, !s32i) -> !cir.vector<2 x !s64i>
// CIR:         %[[MASK8:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[MASKV:.*]] = cir.vec.shuffle(%[[MASK8]], %[[MASK8]] : !cir.vector<8 x !cir.int<s, 1>>) [#cir.int<0> : !s32i, #cir.int<1> : !s32i] : !cir.vector<2 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.vec.ternary(%[[MASKV]], %[[TERN]], %[[A]]) : !cir.vector<2 x !cir.int<s, 1>>, !cir.vector<2 x !s64i>

// LLVM-LABEL: @test_pternlogq128_mask
// LLVM:         %[[A:.*]] = load <2 x i64>, ptr %{{.*}}, align 16
// LLVM:         %[[B:.*]] = load <2 x i64>, ptr %{{.*}}, align 16
// LLVM:         %[[C:.*]] = load <2 x i64>, ptr %{{.*}}, align 16
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[TERN:.*]] = call <2 x i64> @llvm.x86.avx512.pternlog.q.128(<2 x i64> %[[A]], <2 x i64> %[[B]], <2 x i64> %[[C]], i32 150)
// LLVM:         %[[MASK8:.*]] = bitcast i8 %[[MASK]] to <8 x i1>
// LLVM:         %[[MASKV:.*]] = shufflevector <8 x i1> %{{.*}}, <8 x i1> %{{.*}}, <2 x i32> <i32 0, i32 1>
// LLVM:         %[[RESULT:.*]] = select <2 x i1> %{{.*}}, <2 x i64> %[[TERN]], <2 x i64> %[[A]]

v4di test_pternlogq256_mask(v4di a, v4di b, v4di c, unsigned char mask) {
  return __builtin_ia32_pternlogq256_mask(a, b, c, 105, mask);
}

// CIR-LABEL: @test_pternlogq256_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !s64i>>, !cir.vector<4 x !s64i>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !s64i>>, !cir.vector<4 x !s64i>
// CIR:         %[[C:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !s64i>>, !cir.vector<4 x !s64i>
// CIR:         %[[IMM:.*]] = cir.const #cir.int<105> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[TERN:.*]] = cir.call_llvm_intrinsic "x86.avx512.pternlog.q.256" %[[A]], %[[B]], %[[C]], %[[IMM]] : (!cir.vector<4 x !s64i>, !cir.vector<4 x !s64i>, !cir.vector<4 x !s64i>, !s32i) -> !cir.vector<4 x !s64i>
// CIR:         %[[MASK8:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[MASKV:.*]] = cir.vec.shuffle(%[[MASK8]], %[[MASK8]] : !cir.vector<8 x !cir.int<s, 1>>) [#cir.int<0> : !s32i, #cir.int<1> : !s32i, #cir.int<2> : !s32i, #cir.int<3> : !s32i] : !cir.vector<4 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.vec.ternary(%[[MASKV]], %[[TERN]], %[[A]]) : !cir.vector<4 x !cir.int<s, 1>>, !cir.vector<4 x !s64i>

// LLVM-LABEL: @test_pternlogq256_mask
// LLVM:         %[[A:.*]] = load <4 x i64>, ptr %{{.*}}, align 32
// LLVM:         %[[B:.*]] = load <4 x i64>, ptr %{{.*}}, align 32
// LLVM:         %[[C:.*]] = load <4 x i64>, ptr %{{.*}}, align 32
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[TERN:.*]] = call <4 x i64> @llvm.x86.avx512.pternlog.q.256(<4 x i64> %[[A]], <4 x i64> %[[B]], <4 x i64> %[[C]], i32 105)
// LLVM:         %[[MASK8:.*]] = bitcast i8 %[[MASK]] to <8 x i1>
// LLVM:         %[[MASKV:.*]] = shufflevector <8 x i1> %{{.*}}, <8 x i1> %{{.*}}, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
// LLVM:         %[[RESULT:.*]] = select <4 x i1> %{{.*}}, <4 x i64> %[[TERN]], <4 x i64> %[[A]]

v8di test_pternlogq512_mask(v8di a, v8di b, v8di c, unsigned char mask) {
  return __builtin_ia32_pternlogq512_mask(a, b, c, 90, mask);
}

// CIR-LABEL: @test_pternlogq512_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !s64i>>, !cir.vector<8 x !s64i>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !s64i>>, !cir.vector<8 x !s64i>
// CIR:         %[[C:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !s64i>>, !cir.vector<8 x !s64i>
// CIR:         %[[IMM:.*]] = cir.const #cir.int<90> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[TERN:.*]] = cir.call_llvm_intrinsic "x86.avx512.pternlog.q.512" %[[A]], %[[B]], %[[C]], %[[IMM]] : (!cir.vector<8 x !s64i>, !cir.vector<8 x !s64i>, !cir.vector<8 x !s64i>, !s32i) -> !cir.vector<8 x !s64i>
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.vec.ternary(%[[MASKV]], %[[TERN]], %[[A]]) : !cir.vector<8 x !cir.int<s, 1>>, !cir.vector<8 x !s64i>

// LLVM-LABEL: @test_pternlogq512_mask
// LLVM:         %[[A:.*]] = load <8 x i64>, ptr %{{.*}}, align 64
// LLVM:         %[[B:.*]] = load <8 x i64>, ptr %{{.*}}, align 64
// LLVM:         %[[C:.*]] = load <8 x i64>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[TERN:.*]] = call <8 x i64> @llvm.x86.avx512.pternlog.q.512(<8 x i64> %[[A]], <8 x i64> %[[B]], <8 x i64> %[[C]], i32 90)
// LLVM:         %[[MASKV:.*]] = bitcast i8 %[[MASK]] to <8 x i1>
// LLVM:         %[[RESULT:.*]] = select <8 x i1> %{{.*}}, <8 x i64> %[[TERN]], <8 x i64> %[[A]]
