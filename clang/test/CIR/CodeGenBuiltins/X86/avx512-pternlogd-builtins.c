// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512vl -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512vl -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef int v4si __attribute__((vector_size(16)));
typedef int v8si __attribute__((vector_size(32)));
typedef int v16si __attribute__((vector_size(64)));

v4si test_pternlogd128_mask(v4si a, v4si b, v4si c, unsigned char mask) {
  return __builtin_ia32_pternlogd128_mask(a, b, c, 150, mask);
}

// CIR-LABEL: @test_pternlogd128_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !s32i>>, !cir.vector<4 x !s32i>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !s32i>>, !cir.vector<4 x !s32i>
// CIR:         %[[C:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !s32i>>, !cir.vector<4 x !s32i>
// CIR:         %[[IMM:.*]] = cir.const #cir.int<150> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[TERN:.*]] = cir.call_llvm_intrinsic "x86.avx512.pternlog.d.128" %[[A]], %[[B]], %[[C]], %[[IMM]] : (!cir.vector<4 x !s32i>, !cir.vector<4 x !s32i>, !cir.vector<4 x !s32i>, !s32i) -> !cir.vector<4 x !s32i>
// CIR:         %[[MASK8:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[MASKV:.*]] = cir.vec.shuffle(%[[MASK8]], %[[MASK8]] : !cir.vector<8 x !cir.int<s, 1>>) [#cir.int<0> : !s32i, #cir.int<1> : !s32i, #cir.int<2> : !s32i, #cir.int<3> : !s32i] : !cir.vector<4 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.vec.ternary(%[[MASKV]], %[[TERN]], %[[A]]) : !cir.vector<4 x !cir.int<s, 1>>, !cir.vector<4 x !s32i>

// LLVM-LABEL: @test_pternlogd128_mask
// LLVM:         %[[A:.*]] = load <4 x i32>, ptr %{{.*}}, align 16
// LLVM:         %[[B:.*]] = load <4 x i32>, ptr %{{.*}}, align 16
// LLVM:         %[[C:.*]] = load <4 x i32>, ptr %{{.*}}, align 16
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[TERN:.*]] = call <4 x i32> @llvm.x86.avx512.pternlog.d.128(<4 x i32> %[[A]], <4 x i32> %[[B]], <4 x i32> %[[C]], i32 150)
// LLVM:         %[[MASK8:.*]] = bitcast i8 %[[MASK]] to <8 x i1>
// LLVM:         %[[MASKV:.*]] = shufflevector <8 x i1> %{{.*}}, <8 x i1> %{{.*}}, <4 x i32> <i32 0, i32 1, i32 2, i32 3>
// LLVM:         %[[RESULT:.*]] = select <4 x i1> %{{.*}}, <4 x i32> %[[TERN]], <4 x i32> %[[A]]

v8si test_pternlogd256_mask(v8si a, v8si b, v8si c, unsigned char mask) {
  return __builtin_ia32_pternlogd256_mask(a, b, c, 105, mask);
}

// CIR-LABEL: @test_pternlogd256_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !s32i>>, !cir.vector<8 x !s32i>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !s32i>>, !cir.vector<8 x !s32i>
// CIR:         %[[C:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !s32i>>, !cir.vector<8 x !s32i>
// CIR:         %[[IMM:.*]] = cir.const #cir.int<105> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[TERN:.*]] = cir.call_llvm_intrinsic "x86.avx512.pternlog.d.256" %[[A]], %[[B]], %[[C]], %[[IMM]] : (!cir.vector<8 x !s32i>, !cir.vector<8 x !s32i>, !cir.vector<8 x !s32i>, !s32i) -> !cir.vector<8 x !s32i>
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.vec.ternary(%[[MASKV]], %[[TERN]], %[[A]]) : !cir.vector<8 x !cir.int<s, 1>>, !cir.vector<8 x !s32i>

// LLVM-LABEL: @test_pternlogd256_mask
// LLVM:         %[[A:.*]] = load <8 x i32>, ptr %{{.*}}, align 32
// LLVM:         %[[B:.*]] = load <8 x i32>, ptr %{{.*}}, align 32
// LLVM:         %[[C:.*]] = load <8 x i32>, ptr %{{.*}}, align 32
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[TERN:.*]] = call <8 x i32> @llvm.x86.avx512.pternlog.d.256(<8 x i32> %[[A]], <8 x i32> %[[B]], <8 x i32> %[[C]], i32 105)
// LLVM:         %[[MASKV:.*]] = bitcast i8 %[[MASK]] to <8 x i1>
// LLVM:         %[[RESULT:.*]] = select <8 x i1> %{{.*}}, <8 x i32> %[[TERN]], <8 x i32> %[[A]]

v16si test_pternlogd512_mask(v16si a, v16si b, v16si c,
                             unsigned short mask) {
  return __builtin_ia32_pternlogd512_mask(a, b, c, 90, mask);
}

// CIR-LABEL: @test_pternlogd512_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<16 x !s32i>>, !cir.vector<16 x !s32i>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<16 x !s32i>>, !cir.vector<16 x !s32i>
// CIR:         %[[C:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<16 x !s32i>>, !cir.vector<16 x !s32i>
// CIR:         %[[IMM:.*]] = cir.const #cir.int<90> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u16i>, !u16i
// CIR:         %[[TERN:.*]] = cir.call_llvm_intrinsic "x86.avx512.pternlog.d.512" %[[A]], %[[B]], %[[C]], %[[IMM]] : (!cir.vector<16 x !s32i>, !cir.vector<16 x !s32i>, !cir.vector<16 x !s32i>, !s32i) -> !cir.vector<16 x !s32i>
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u16i -> !cir.vector<16 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.vec.ternary(%[[MASKV]], %[[TERN]], %[[A]]) : !cir.vector<16 x !cir.int<s, 1>>, !cir.vector<16 x !s32i>

// LLVM-LABEL: @test_pternlogd512_mask
// LLVM:         %[[A:.*]] = load <16 x i32>, ptr %{{.*}}, align 64
// LLVM:         %[[B:.*]] = load <16 x i32>, ptr %{{.*}}, align 64
// LLVM:         %[[C:.*]] = load <16 x i32>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i16, ptr %{{.*}}, align 2
// LLVM:         %[[TERN:.*]] = call <16 x i32> @llvm.x86.avx512.pternlog.d.512(<16 x i32> %[[A]], <16 x i32> %[[B]], <16 x i32> %[[C]], i32 90)
// LLVM:         %[[MASKV:.*]] = bitcast i16 %[[MASK]] to <16 x i1>
// LLVM:         %[[RESULT:.*]] = select <16 x i1> %{{.*}}, <16 x i32> %[[TERN]], <16 x i32> %[[A]]
