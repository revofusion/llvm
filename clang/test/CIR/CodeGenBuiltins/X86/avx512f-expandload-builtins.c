// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef int v16si __attribute__((vector_size(64)));

v16si test_expandloadsi512_mask(const v16si *ptr, v16si passthru,
                                unsigned short mask) {
  return __builtin_ia32_expandloadsi512_mask(ptr, passthru, mask);
}

// CIR-LABEL: @test_expandloadsi512_mask
// CIR:         %[[PTR:.*]] = cir.load {{.*}}, !cir.ptr<!cir.vector<16 x !s32i>>
// CIR:         %[[PASSTHRU:.*]] = cir.load {{.*}}, !cir.vector<16 x !s32i>
// CIR:         %[[MASK:.*]] = cir.load {{.*}}, !u16i
// CIR:         %[[MASK_VEC:.*]] = cir.cast bitcast %[[MASK]] : !u16i -> !cir.vector<16 x !cir.int<s, 1>>
// CIR:         %[[RESULT:.*]] = cir.call_llvm_intrinsic "masked.expandload" %[[PTR]], %[[MASK_VEC]], %[[PASSTHRU]] : (!cir.ptr<!cir.vector<16 x !s32i>>, !cir.vector<16 x !cir.int<s, 1>>, !cir.vector<16 x !s32i>) -> !cir.vector<16 x !s32i>
// CIR:         cir.store %[[RESULT]], %[[RET_PTR:.*]] : !cir.vector<16 x !s32i>, !cir.ptr<!cir.vector<16 x !s32i>>
// CIR:         %[[RET:.*]] = cir.load %[[RET_PTR]] : !cir.ptr<!cir.vector<16 x !s32i>>, !cir.vector<16 x !s32i>
// CIR:         cir.return %[[RET]] : !cir.vector<16 x !s32i>

// LLVM-LABEL: @test_expandloadsi512_mask
// LLVM:         %[[PTR:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:         %[[PASSTHRU:.*]] = load <16 x i32>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i16, ptr %{{.*}}, align 2
// LLVM:         %[[MASK_VEC:.*]] = bitcast i16 %[[MASK]] to <16 x i1>
// LLVM:         %[[RESULT:.*]] = call <16 x i32> @llvm.masked.expandload.v16i32.p0(ptr %[[PTR]], <16 x i1> %[[MASK_VEC]], <16 x i32> %[[PASSTHRU]])
// LLVM:         store <16 x i32> %[[RESULT]], ptr %[[RET_PTR:.*]], align 64
// LLVM:         %[[RET:.*]] = load <16 x i32>, ptr %[[RET_PTR]], align 64
// LLVM:         ret <16 x i32> %[[RET]]
