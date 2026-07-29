// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512fp16 -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512fp16 -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef short v32hi __attribute__((vector_size(64)));
typedef _Float16 v32hf __attribute__((vector_size(64)));

v32hf test_vcvtw2ph512_mask(v32hi input, v32hf passthrough, unsigned mask) {
  return __builtin_ia32_vcvtw2ph512_mask(input, passthrough, mask, 8);
}

// CIR-LABEL: @test_vcvtw2ph512_mask
// CIR:         %[[INPUT:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<32 x !s16i>>, !cir.vector<32 x !s16i>
// CIR:         %[[PASSTHROUGH:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<32 x !cir.f16>>, !cir.vector<32 x !cir.f16>
// CIR:         %[[MASK:.*]] = cir.load {{.*}} : !cir.ptr<!u32i>, !u32i
// CIR:         %[[ROUND:.*]] = cir.const #cir.int<8> : !s32i
// CIR:         %[[CONVERTED:.*]] = cir.call_llvm_intrinsic "x86.avx512.sitofp.round" %[[INPUT]], %[[ROUND]] : (!cir.vector<32 x !s16i>, !s32i) -> !cir.vector<32 x !cir.f16>
// CIR:         %[[MASKV:.*]] = cir.cast bitcast %[[MASK]] : !u32i -> !cir.vector<32 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.vec.ternary(%[[MASKV]], %[[CONVERTED]], %[[PASSTHROUGH]]) : !cir.vector<32 x !cir.int<s, 1>>, !cir.vector<32 x !cir.f16>

// LLVM-LABEL: @test_vcvtw2ph512_mask
// LLVM:         %[[INPUT:.*]] = load <32 x i16>, ptr %{{.*}}, align 64
// LLVM:         %[[PASSTHROUGH:.*]] = load <32 x half>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i32, ptr %{{.*}}, align 4
// LLVM:         %[[CONVERTED:.*]] = call <32 x half> @llvm.x86.avx512.sitofp.round.v32f16.v32i16(<32 x i16> %[[INPUT]], i32 8)
// LLVM:         %[[MASK_BITS:.*]] = bitcast i32 %[[MASK]] to <32 x i1>
// LLVM:         %[[MASKV:.*]] = icmp ne <32 x i1> %[[MASK_BITS]], zeroinitializer
// LLVM:         %[[RESULT:.*]] = select <32 x i1> %[[MASKV]], <32 x half> %[[CONVERTED]], <32 x half> %[[PASSTHROUGH]]
