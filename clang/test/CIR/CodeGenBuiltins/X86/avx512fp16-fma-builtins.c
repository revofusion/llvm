// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512fp16 -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512fp16 -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef _Float16 v32hf __attribute__((vector_size(64)));

v32hf test_vfmaddph512_mask(v32hf a, v32hf b, v32hf c, unsigned mask) {
  return __builtin_ia32_vfmaddph512_mask(a, b, c, mask, 4);
}

// CIR-LABEL: @test_vfmaddph512_mask
// CIR:         %[[A:.*]] = cir.load {{.*}}, !cir.vector<32 x !cir.f16>
// CIR:         %[[B:.*]] = cir.load {{.*}}, !cir.vector<32 x !cir.f16>
// CIR:         %[[C:.*]] = cir.load {{.*}}, !cir.vector<32 x !cir.f16>
// CIR:         %[[MASK:.*]] = cir.load {{.*}}, !u32i
// CIR:         %[[FMA:.*]] = cir.call_llvm_intrinsic "fma" %[[A]], %[[B]], %[[C]] : (!cir.vector<32 x !cir.f16>, !cir.vector<32 x !cir.f16>, !cir.vector<32 x !cir.f16>) -> !cir.vector<32 x !cir.f16>
// CIR:         %[[MASK_VEC:.*]] = cir.cast bitcast %[[MASK]] : !u32i -> !cir.vector<32 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.vec.ternary(%[[MASK_VEC]], %[[FMA]], %[[A]]) : !cir.vector<32 x !cir.int<s, 1>>, !cir.vector<32 x !cir.f16>
// CIR:         cir.store %[[RESULT]], %{{.*}} : !cir.vector<32 x !cir.f16>, !cir.ptr<!cir.vector<32 x !cir.f16>>
// CIR:         %[[RET:.*]] = cir.load %{{.*}} : !cir.ptr<!cir.vector<32 x !cir.f16>>, !cir.vector<32 x !cir.f16>
// CIR:         cir.return %[[RET]] : !cir.vector<32 x !cir.f16>

// LLVM-LABEL: @test_vfmaddph512_mask
// LLVM:         %[[A:.*]] = load <32 x half>, ptr %{{.*}}, align 64
// LLVM:         %[[B:.*]] = load <32 x half>, ptr %{{.*}}, align 64
// LLVM:         %[[C:.*]] = load <32 x half>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i32, ptr %{{.*}}, align 4
// LLVM:         %[[FMA:.*]] = call <32 x half> @llvm.fma.v32f16(<32 x half> %[[A]], <32 x half> %[[B]], <32 x half> %[[C]])
// LLVM:         %[[MASK_BITS:.*]] = bitcast i32 %[[MASK]] to <32 x i1>
// LLVM:         %[[MASK_VEC:.*]] = icmp ne <32 x i1> %[[MASK_BITS]], zeroinitializer
// LLVM:         %[[RESULT:.*]] = select <32 x i1> %[[MASK_VEC]], <32 x half> %[[FMA]], <32 x half> %[[A]]
// LLVM:         store <32 x half> %[[RESULT]], ptr %[[RET_PTR:.*]], align 64
// LLVM:         %[[RET:.*]] = load <32 x half>, ptr %[[RET_PTR]], align 64
// LLVM:         ret <32 x half> %[[RET]]

v32hf test_vfmaddph512_mask_round(v32hf a, v32hf b, v32hf c,
                                  unsigned mask) {
  return __builtin_ia32_vfmaddph512_mask(a, b, c, mask, 8);
}

// CIR-LABEL: @test_vfmaddph512_mask_round
// CIR:         %[[A:.*]] = cir.load {{.*}}, !cir.vector<32 x !cir.f16>
// CIR:         %[[B:.*]] = cir.load {{.*}}, !cir.vector<32 x !cir.f16>
// CIR:         %[[C:.*]] = cir.load {{.*}}, !cir.vector<32 x !cir.f16>
// CIR:         %[[MASK:.*]] = cir.load {{.*}}, !u32i
// CIR:         %[[ROUND:.*]] = cir.const #cir.int<8> : !s32i
// CIR:         %[[FMA:.*]] = cir.call_llvm_intrinsic "x86.avx512fp16.vfmadd.ph.512" %[[A]], %[[B]], %[[C]], %[[ROUND]] : (!cir.vector<32 x !cir.f16>, !cir.vector<32 x !cir.f16>, !cir.vector<32 x !cir.f16>, !s32i) -> !cir.vector<32 x !cir.f16>
// CIR:         %[[MASK_VEC:.*]] = cir.cast bitcast %[[MASK]] : !u32i -> !cir.vector<32 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.vec.ternary(%[[MASK_VEC]], %[[FMA]], %[[A]]) : !cir.vector<32 x !cir.int<s, 1>>, !cir.vector<32 x !cir.f16>
// CIR:         cir.store %[[RESULT]], %{{.*}} : !cir.vector<32 x !cir.f16>, !cir.ptr<!cir.vector<32 x !cir.f16>>
// CIR:         %[[RET:.*]] = cir.load %{{.*}} : !cir.ptr<!cir.vector<32 x !cir.f16>>, !cir.vector<32 x !cir.f16>
// CIR:         cir.return %[[RET]] : !cir.vector<32 x !cir.f16>

// LLVM-LABEL: @test_vfmaddph512_mask_round
// LLVM:         %[[A:.*]] = load <32 x half>, ptr %{{.*}}, align 64
// LLVM:         %[[B:.*]] = load <32 x half>, ptr %{{.*}}, align 64
// LLVM:         %[[C:.*]] = load <32 x half>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i32, ptr %{{.*}}, align 4
// LLVM:         %[[FMA:.*]] = call <32 x half> @llvm.x86.avx512fp16.vfmadd.ph.512(<32 x half> %[[A]], <32 x half> %[[B]], <32 x half> %[[C]], i32 8)
// LLVM:         %[[MASK_BITS:.*]] = bitcast i32 %[[MASK]] to <32 x i1>
// LLVM:         %[[MASK_VEC:.*]] = icmp ne <32 x i1> %[[MASK_BITS]], zeroinitializer
// LLVM:         %[[RESULT:.*]] = select <32 x i1> %[[MASK_VEC]], <32 x half> %[[FMA]], <32 x half> %[[A]]
// LLVM:         store <32 x half> %[[RESULT]], ptr %[[RET_PTR:.*]], align 64
// LLVM:         %[[RET:.*]] = load <32 x half>, ptr %[[RET_PTR]], align 64
// LLVM:         ret <32 x half> %[[RET]]
