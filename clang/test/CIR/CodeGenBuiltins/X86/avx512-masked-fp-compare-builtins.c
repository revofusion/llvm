// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512vl -target-feature +avx512fp16 -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512vl -target-feature +avx512fp16 -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef _Float16 v8hf __attribute__((vector_size(16)));
typedef _Float16 v16hf __attribute__((vector_size(32)));
typedef _Float16 v32hf __attribute__((vector_size(64)));
typedef float v4sf __attribute__((vector_size(16)));
typedef float v8sf __attribute__((vector_size(32)));
typedef float v16sf __attribute__((vector_size(64)));
typedef double v2df __attribute__((vector_size(16)));
typedef double v4df __attribute__((vector_size(32)));
typedef double v8df __attribute__((vector_size(64)));

unsigned char test_cmpph128_mask(v8hf a, v8hf b, unsigned char mask) {
  return __builtin_ia32_cmpph128_mask(a, b, 17, mask);
}

// CIR-LABEL: @test_cmpph128_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !cir.f16>>, !cir.vector<8 x !cir.f16>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !cir.f16>>, !cir.vector<8 x !cir.f16>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<17> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx512fp16.mask.cmp.ph.128" %[[A]], %[[B]], %[[PRED]], %[[MASKV]] : (!cir.vector<8 x !cir.f16>, !cir.vector<8 x !cir.f16>, !s32i, !cir.vector<8 x !cir.int<s, 1>>) -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.cast bitcast %[[CMP]] : !cir.vector<8 x !cir.int<s, 1>> -> !u8i

// LLVM-LABEL: @test_cmpph128_mask
// LLVM:         %[[A:.*]] = load <8 x half>, ptr %{{.*}}, align 16
// LLVM:         %[[B:.*]] = load <8 x half>, ptr %{{.*}}, align 16
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[MASK_BITS:.*]] = bitcast i8 %[[MASK]] to <8 x i1>
// LLVM:         %[[CMP:.*]] = call <8 x i1> @llvm.x86.avx512fp16.mask.cmp.ph.128(<8 x half> %[[A]], <8 x half> %[[B]], i32 17, <8 x i1> %{{.*}})
// LLVM:         %[[RESULT:.*]] = bitcast <8 x i1> %[[CMP]] to i8

unsigned short test_cmpph256_mask(v16hf a, v16hf b, unsigned short mask) {
  return __builtin_ia32_cmpph256_mask(a, b, 18, mask);
}

// CIR-LABEL: @test_cmpph256_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<16 x !cir.f16>>, !cir.vector<16 x !cir.f16>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<16 x !cir.f16>>, !cir.vector<16 x !cir.f16>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<18> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u16i>, !u16i
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u16i -> !cir.vector<16 x !cir.int<s, 1>>
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx512fp16.mask.cmp.ph.256" %[[A]], %[[B]], %[[PRED]], %[[MASKV]] : (!cir.vector<16 x !cir.f16>, !cir.vector<16 x !cir.f16>, !s32i, !cir.vector<16 x !cir.int<s, 1>>) -> !cir.vector<16 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.cast bitcast %[[CMP]] : !cir.vector<16 x !cir.int<s, 1>> -> !u16i

// LLVM-LABEL: @test_cmpph256_mask
// LLVM:         %[[A:.*]] = load <16 x half>, ptr %{{.*}}, align 32
// LLVM:         %[[B:.*]] = load <16 x half>, ptr %{{.*}}, align 32
// LLVM:         %[[MASK:.*]] = load i16, ptr %{{.*}}, align 2
// LLVM:         %[[MASK_BITS:.*]] = bitcast i16 %[[MASK]] to <16 x i1>
// LLVM:         %[[CMP:.*]] = call <16 x i1> @llvm.x86.avx512fp16.mask.cmp.ph.256(<16 x half> %[[A]], <16 x half> %[[B]], i32 18, <16 x i1> %{{.*}})
// LLVM:         %[[RESULT:.*]] = bitcast <16 x i1> %[[CMP]] to i16

unsigned test_cmpph512_mask(v32hf a, v32hf b, unsigned mask) {
  return __builtin_ia32_cmpph512_mask(a, b, 19, mask, 8);
}

// CIR-LABEL: @test_cmpph512_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<32 x !cir.f16>>, !cir.vector<32 x !cir.f16>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<32 x !cir.f16>>, !cir.vector<32 x !cir.f16>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<19> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u32i>, !u32i
// CIR:         %[[ROUND:.*]] = cir.const #cir.int<8> : !s32i
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u32i -> !cir.vector<32 x !cir.int<s, 1>>
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx512fp16.mask.cmp.ph.512" %[[A]], %[[B]], %[[PRED]], %[[MASKV]], %[[ROUND]] : (!cir.vector<32 x !cir.f16>, !cir.vector<32 x !cir.f16>, !s32i, !cir.vector<32 x !cir.int<s, 1>>, !s32i) -> !cir.vector<32 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.cast bitcast %[[CMP]] : !cir.vector<32 x !cir.int<s, 1>> -> !u32i

// LLVM-LABEL: @test_cmpph512_mask
// LLVM:         %[[A:.*]] = load <32 x half>, ptr %{{.*}}, align 64
// LLVM:         %[[B:.*]] = load <32 x half>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i32, ptr %{{.*}}, align 4
// LLVM:         %[[MASK_BITS:.*]] = bitcast i32 %[[MASK]] to <32 x i1>
// LLVM:         %[[CMP:.*]] = call <32 x i1> @llvm.x86.avx512fp16.mask.cmp.ph.512(<32 x half> %[[A]], <32 x half> %[[B]], i32 19, <32 x i1> %{{.*}}, i32 8)
// LLVM:         %[[RESULT:.*]] = bitcast <32 x i1> %[[CMP]] to i32

unsigned char test_cmpps128_mask(v4sf a, v4sf b, unsigned char mask) {
  return __builtin_ia32_cmpps128_mask(a, b, 20, mask);
}

// CIR-LABEL: @test_cmpps128_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !cir.float>>, !cir.vector<4 x !cir.float>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !cir.float>>, !cir.vector<4 x !cir.float>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<20> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[MASK8:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[MASKV:.*]] = cir.vec.shuffle(%[[MASK8]], %[[MASK8]] : !cir.vector<8 x !cir.int<s, 1>>) [#cir.int<0> : !s32i, #cir.int<1> : !s32i, #cir.int<2> : !s32i, #cir.int<3> : !s32i] : !cir.vector<4 x !cir.int<s, 1>>
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx512.mask.cmp.ps.128" %[[A]], %[[B]], %[[PRED]], %[[MASKV]] : (!cir.vector<4 x !cir.float>, !cir.vector<4 x !cir.float>, !s32i, !cir.vector<4 x !cir.int<s, 1>>) -> !cir.vector<4 x !cir.int<s, 1>>

// LLVM-LABEL: @test_cmpps128_mask
// LLVM:         %[[A:.*]] = load <4 x float>, ptr %{{.*}}, align 16
// LLVM:         %[[B:.*]] = load <4 x float>, ptr %{{.*}}, align 16
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[CMP:.*]] = call <4 x i1> @llvm.x86.avx512.mask.cmp.ps.128(<4 x float> %[[A]], <4 x float> %[[B]], i32 20, <4 x i1> %{{.*}})
// LLVM:         %[[WIDE:.*]] = shufflevector <4 x i1> %[[CMP]], <4 x i1> zeroinitializer, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
// LLVM:         %[[RESULT:.*]] = bitcast <8 x i1> %[[WIDE]] to i8

unsigned char test_cmpps256_mask(v8sf a, v8sf b, unsigned char mask) {
  return __builtin_ia32_cmpps256_mask(a, b, 21, mask);
}

// CIR-LABEL: @test_cmpps256_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !cir.float>>, !cir.vector<8 x !cir.float>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !cir.float>>, !cir.vector<8 x !cir.float>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<21> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx512.mask.cmp.ps.256" %[[A]], %[[B]], %[[PRED]], %[[MASKV]] : (!cir.vector<8 x !cir.float>, !cir.vector<8 x !cir.float>, !s32i, !cir.vector<8 x !cir.int<s, 1>>) -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.cast bitcast %[[CMP]] : !cir.vector<8 x !cir.int<s, 1>> -> !u8i

// LLVM-LABEL: @test_cmpps256_mask
// LLVM:         %[[A:.*]] = load <8 x float>, ptr %{{.*}}, align 32
// LLVM:         %[[B:.*]] = load <8 x float>, ptr %{{.*}}, align 32
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[CMP:.*]] = call <8 x i1> @llvm.x86.avx512.mask.cmp.ps.256(<8 x float> %[[A]], <8 x float> %[[B]], i32 21, <8 x i1> %{{.*}})
// LLVM:         %[[RESULT:.*]] = bitcast <8 x i1> %[[CMP]] to i8

unsigned short test_cmpps512_mask(v16sf a, v16sf b, unsigned short mask) {
  return __builtin_ia32_cmpps512_mask(a, b, 22, mask, 8);
}

// CIR-LABEL: @test_cmpps512_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<16 x !cir.float>>, !cir.vector<16 x !cir.float>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<16 x !cir.float>>, !cir.vector<16 x !cir.float>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<22> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u16i>, !u16i
// CIR:         %[[ROUND:.*]] = cir.const #cir.int<8> : !s32i
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u16i -> !cir.vector<16 x !cir.int<s, 1>>
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx512.mask.cmp.ps.512" %[[A]], %[[B]], %[[PRED]], %[[MASKV]], %[[ROUND]] : (!cir.vector<16 x !cir.float>, !cir.vector<16 x !cir.float>, !s32i, !cir.vector<16 x !cir.int<s, 1>>, !s32i) -> !cir.vector<16 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.cast bitcast %[[CMP]] : !cir.vector<16 x !cir.int<s, 1>> -> !u16i

// LLVM-LABEL: @test_cmpps512_mask
// LLVM:         %[[A:.*]] = load <16 x float>, ptr %{{.*}}, align 64
// LLVM:         %[[B:.*]] = load <16 x float>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i16, ptr %{{.*}}, align 2
// LLVM:         %[[CMP:.*]] = call <16 x i1> @llvm.x86.avx512.mask.cmp.ps.512(<16 x float> %[[A]], <16 x float> %[[B]], i32 22, <16 x i1> %{{.*}}, i32 8)
// LLVM:         %[[RESULT:.*]] = bitcast <16 x i1> %[[CMP]] to i16

unsigned char test_cmppd128_mask(v2df a, v2df b, unsigned char mask) {
  return __builtin_ia32_cmppd128_mask(a, b, 23, mask);
}

// CIR-LABEL: @test_cmppd128_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<2 x !cir.double>>, !cir.vector<2 x !cir.double>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<2 x !cir.double>>, !cir.vector<2 x !cir.double>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<23> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[MASK8:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[MASKV:.*]] = cir.vec.shuffle(%[[MASK8]], %[[MASK8]] : !cir.vector<8 x !cir.int<s, 1>>) [#cir.int<0> : !s32i, #cir.int<1> : !s32i] : !cir.vector<2 x !cir.int<s, 1>>
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx512.mask.cmp.pd.128" %[[A]], %[[B]], %[[PRED]], %[[MASKV]] : (!cir.vector<2 x !cir.double>, !cir.vector<2 x !cir.double>, !s32i, !cir.vector<2 x !cir.int<s, 1>>) -> !cir.vector<2 x !cir.int<s, 1>>

// LLVM-LABEL: @test_cmppd128_mask
// LLVM:         %[[A:.*]] = load <2 x double>, ptr %{{.*}}, align 16
// LLVM:         %[[B:.*]] = load <2 x double>, ptr %{{.*}}, align 16
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[CMP:.*]] = call <2 x i1> @llvm.x86.avx512.mask.cmp.pd.128(<2 x double> %[[A]], <2 x double> %[[B]], i32 23, <2 x i1> %{{.*}})
// LLVM:         %[[WIDE:.*]] = shufflevector <2 x i1> %[[CMP]], <2 x i1> zeroinitializer, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 2, i32 3, i32 2, i32 3>
// LLVM:         %[[RESULT:.*]] = bitcast <8 x i1> %[[WIDE]] to i8

unsigned char test_cmppd256_mask(v4df a, v4df b, unsigned char mask) {
  return __builtin_ia32_cmppd256_mask(a, b, 24, mask);
}

// CIR-LABEL: @test_cmppd256_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !cir.double>>, !cir.vector<4 x !cir.double>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !cir.double>>, !cir.vector<4 x !cir.double>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<24> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[MASK8:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[MASKV:.*]] = cir.vec.shuffle(%[[MASK8]], %[[MASK8]] : !cir.vector<8 x !cir.int<s, 1>>) [#cir.int<0> : !s32i, #cir.int<1> : !s32i, #cir.int<2> : !s32i, #cir.int<3> : !s32i] : !cir.vector<4 x !cir.int<s, 1>>
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx512.mask.cmp.pd.256" %[[A]], %[[B]], %[[PRED]], %[[MASKV]] : (!cir.vector<4 x !cir.double>, !cir.vector<4 x !cir.double>, !s32i, !cir.vector<4 x !cir.int<s, 1>>) -> !cir.vector<4 x !cir.int<s, 1>>

// LLVM-LABEL: @test_cmppd256_mask
// LLVM:         %[[A:.*]] = load <4 x double>, ptr %{{.*}}, align 32
// LLVM:         %[[B:.*]] = load <4 x double>, ptr %{{.*}}, align 32
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[CMP:.*]] = call <4 x i1> @llvm.x86.avx512.mask.cmp.pd.256(<4 x double> %[[A]], <4 x double> %[[B]], i32 24, <4 x i1> %{{.*}})
// LLVM:         %[[WIDE:.*]] = shufflevector <4 x i1> %[[CMP]], <4 x i1> zeroinitializer, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 5, i32 6, i32 7>
// LLVM:         %[[RESULT:.*]] = bitcast <8 x i1> %[[WIDE]] to i8

unsigned char test_cmppd512_mask(v8df a, v8df b, unsigned char mask) {
  return __builtin_ia32_cmppd512_mask(a, b, 25, mask, 8);
}

// CIR-LABEL: @test_cmppd512_mask
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !cir.double>>, !cir.vector<8 x !cir.double>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !cir.double>>, !cir.vector<8 x !cir.double>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<25> : !s32i
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u8i>, !u8i
// CIR:         %[[ROUND:.*]] = cir.const #cir.int<8> : !s32i
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx512.mask.cmp.pd.512" %[[A]], %[[B]], %[[PRED]], %[[MASKV]], %[[ROUND]] : (!cir.vector<8 x !cir.double>, !cir.vector<8 x !cir.double>, !s32i, !cir.vector<8 x !cir.int<s, 1>>, !s32i) -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.cast bitcast %[[CMP]] : !cir.vector<8 x !cir.int<s, 1>> -> !u8i

// LLVM-LABEL: @test_cmppd512_mask
// LLVM:         %[[A:.*]] = load <8 x double>, ptr %{{.*}}, align 64
// LLVM:         %[[B:.*]] = load <8 x double>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[CMP:.*]] = call <8 x i1> @llvm.x86.avx512.mask.cmp.pd.512(<8 x double> %[[A]], <8 x double> %[[B]], i32 25, <8 x i1> %{{.*}}, i32 8)
// LLVM:         %[[RESULT:.*]] = bitcast <8 x i1> %[[CMP]] to i8
