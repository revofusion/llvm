// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx -target-feature +sse4.1 -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx -target-feature +sse4.1 -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef float v8sf __attribute__((vector_size(32)));
typedef double v2df __attribute__((vector_size(16)));
typedef double v4df __attribute__((vector_size(32)));

v8sf test_cmpps256(v8sf a, v8sf b) {
  return __builtin_ia32_cmpps256(a, b, 17);
}

// CIR-LABEL: @test_cmpps256
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !cir.float>>, !cir.vector<8 x !cir.float>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !cir.float>>, !cir.vector<8 x !cir.float>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<17> : !s8i
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx.cmp.ps.256" %[[A]], %[[B]], %[[PRED]] : (!cir.vector<8 x !cir.float>, !cir.vector<8 x !cir.float>, !s8i) -> !cir.vector<8 x !cir.float>

// LLVM-LABEL: @test_cmpps256
// LLVM:         %[[A:.*]] = load <8 x float>, ptr %{{.*}}, align 32
// LLVM:         %[[B:.*]] = load <8 x float>, ptr %{{.*}}, align 32
// LLVM:         %[[CMP:.*]] = call <8 x float> @llvm.x86.avx.cmp.ps.256(<8 x float> %[[A]], <8 x float> %[[B]], i8 17)

v4df test_cmppd256(v4df a, v4df b) {
  return __builtin_ia32_cmppd256(a, b, 18);
}

// CIR-LABEL: @test_cmppd256
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !cir.double>>, !cir.vector<4 x !cir.double>
// CIR:         %[[B:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !cir.double>>, !cir.vector<4 x !cir.double>
// CIR:         %[[PRED:.*]] = cir.const #cir.int<18> : !s8i
// CIR:         %[[CMP:.*]] = cir.call_llvm_intrinsic "x86.avx.cmp.pd.256" %[[A]], %[[B]], %[[PRED]] : (!cir.vector<4 x !cir.double>, !cir.vector<4 x !cir.double>, !s8i) -> !cir.vector<4 x !cir.double>

// LLVM-LABEL: @test_cmppd256
// LLVM:         %[[A:.*]] = load <4 x double>, ptr %{{.*}}, align 32
// LLVM:         %[[B:.*]] = load <4 x double>, ptr %{{.*}}, align 32
// LLVM:         %[[CMP:.*]] = call <4 x double> @llvm.x86.avx.cmp.pd.256(<4 x double> %[[A]], <4 x double> %[[B]], i8 18)

v8sf test_roundps256(v8sf a) {
  return __builtin_ia32_roundps256(a, 3);
}

// CIR-LABEL: @test_roundps256
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<8 x !cir.float>>, !cir.vector<8 x !cir.float>
// CIR:         %[[MODE:.*]] = cir.const #cir.int<3> : !s32i
// CIR:         %[[ROUND:.*]] = cir.call_llvm_intrinsic "x86.avx.round.ps.256" %[[A]], %[[MODE]] : (!cir.vector<8 x !cir.float>, !s32i) -> !cir.vector<8 x !cir.float>

// LLVM-LABEL: @test_roundps256
// LLVM:         %[[A:.*]] = load <8 x float>, ptr %{{.*}}, align 32
// LLVM:         %[[ROUND:.*]] = call <8 x float> @llvm.x86.avx.round.ps.256(<8 x float> %[[A]], i32 3)

v2df test_roundpd(v2df a) {
  return __builtin_ia32_roundpd(a, 2);
}

// CIR-LABEL: @test_roundpd
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<2 x !cir.double>>, !cir.vector<2 x !cir.double>
// CIR:         %[[MODE:.*]] = cir.const #cir.int<2> : !s32i
// CIR:         %[[ROUND:.*]] = cir.call_llvm_intrinsic "x86.sse41.round.pd" %[[A]], %[[MODE]] : (!cir.vector<2 x !cir.double>, !s32i) -> !cir.vector<2 x !cir.double>

// LLVM-LABEL: @test_roundpd
// LLVM:         %[[A:.*]] = load <2 x double>, ptr %{{.*}}, align 16
// LLVM:         %[[ROUND:.*]] = call <2 x double> @llvm.x86.sse41.round.pd(<2 x double> %[[A]], i32 2)

v4df test_roundpd256(v4df a) {
  return __builtin_ia32_roundpd256(a, 1);
}

// CIR-LABEL: @test_roundpd256
// CIR:         %[[A:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !cir.double>>, !cir.vector<4 x !cir.double>
// CIR:         %[[MODE:.*]] = cir.const #cir.int<1> : !s32i
// CIR:         %[[ROUND:.*]] = cir.call_llvm_intrinsic "x86.avx.round.pd.256" %[[A]], %[[MODE]] : (!cir.vector<4 x !cir.double>, !s32i) -> !cir.vector<4 x !cir.double>

// LLVM-LABEL: @test_roundpd256
// LLVM:         %[[A:.*]] = load <4 x double>, ptr %{{.*}}, align 32
// LLVM:         %[[ROUND:.*]] = call <4 x double> @llvm.x86.avx.round.pd.256(<4 x double> %[[A]], i32 1)
