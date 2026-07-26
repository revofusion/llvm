// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +sse2 \
// RUN:   -target-feature +avx2 -target-feature +avx512f \
// RUN:   -target-feature +avx512bw -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +sse2 \
// RUN:   -target-feature +avx2 -target-feature +avx512f \
// RUN:   -target-feature +avx512bw -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t.ll %s

typedef char v16qi __attribute__((__vector_size__(16)));
typedef char v32qi __attribute__((__vector_size__(32)));
typedef char v64qi __attribute__((__vector_size__(64)));

v16qi left128(v16qi x) {
  return __builtin_ia32_pslldqi128_byteshift(x, 4);
}

// CIR-LABEL: cir.func {{.*}} @left128
// CIR: %[[L128_ZERO:.*]] = cir.const #cir.zero : !cir.vector<16 x !s8i>
// CIR: cir.vec.shuffle(%[[L128_ZERO]], %{{.*}} : !cir.vector<16 x !s8i>) [#cir.int<12> : !s32i, #cir.int<13> : !s32i, #cir.int<14> : !s32i, #cir.int<15> : !s32i, #cir.int<16> : !s32i
// LLVM-LABEL: define {{.*}} <16 x i8> @left128
// LLVM: shufflevector <16 x i8> zeroinitializer, <16 x i8> %{{.*}}, <16 x i32> <i32 12, i32 13, i32 14, i32 15, i32 16

v16qi right128(v16qi x) {
  return __builtin_ia32_psrldqi128_byteshift(x, 4);
}

// CIR-LABEL: cir.func {{.*}} @right128
// CIR: %[[R128_ZERO:.*]] = cir.const #cir.zero : !cir.vector<16 x !s8i>
// CIR: cir.vec.shuffle(%{{.*}}, %[[R128_ZERO]] : !cir.vector<16 x !s8i>) [#cir.int<4> : !s32i, #cir.int<5> : !s32i, #cir.int<6> : !s32i, #cir.int<7> : !s32i
// LLVM-LABEL: define {{.*}} <16 x i8> @right128
// LLVM: shufflevector <16 x i8> %{{.*}}, <16 x i8> zeroinitializer, <16 x i32> <i32 4, i32 5, i32 6, i32 7

v32qi left256(v32qi x) {
  return __builtin_ia32_pslldqi256_byteshift(x, 4);
}

// CIR-LABEL: cir.func {{.*}} @left256
// CIR: cir.vec.shuffle(%{{.*}}, %{{.*}} : !cir.vector<32 x !s8i>) [#cir.int<12> : !s32i, #cir.int<13> : !s32i, #cir.int<14> : !s32i, #cir.int<15> : !s32i, #cir.int<32> : !s32i{{.*}}#cir.int<43> : !s32i, #cir.int<28> : !s32i
// LLVM-LABEL: define {{.*}} <32 x i8> @left256
// LLVM: shufflevector <32 x i8> zeroinitializer, <32 x i8> %{{.*}}, <32 x i32> <i32 12, i32 13, i32 14, i32 15, i32 32,{{.*}}i32 43, i32 28

v32qi right256(v32qi x) {
  return __builtin_ia32_psrldqi256_byteshift(x, 4);
}

// CIR-LABEL: cir.func {{.*}} @right256
// CIR: cir.vec.shuffle(%{{.*}}, %{{.*}} : !cir.vector<32 x !s8i>) [#cir.int<4> : !s32i, #cir.int<5> : !s32i{{.*}}#cir.int<15> : !s32i, #cir.int<32> : !s32i{{.*}}#cir.int<35> : !s32i, #cir.int<20> : !s32i
// LLVM-LABEL: define {{.*}} <32 x i8> @right256
// LLVM: shufflevector <32 x i8> %{{.*}}, <32 x i8> zeroinitializer, <32 x i32> <i32 4, i32 5,{{.*}}i32 15, i32 32,{{.*}}i32 35, i32 20

v64qi left512(v64qi x) {
  return __builtin_ia32_pslldqi512_byteshift(x, 4);
}

// CIR-LABEL: cir.func {{.*}} @left512
// CIR: cir.vec.shuffle(%{{.*}}, %{{.*}} : !cir.vector<64 x !s8i>) [#cir.int<12> : !s32i{{.*}}#cir.int<75> : !s32i, #cir.int<28> : !s32i{{.*}}#cir.int<91> : !s32i, #cir.int<44> : !s32i
// LLVM-LABEL: define {{.*}} <64 x i8> @left512
// LLVM: shufflevector <64 x i8> zeroinitializer, <64 x i8> %{{.*}}, <64 x i32> <i32 12,{{.*}}i32 75, i32 28,{{.*}}i32 91, i32 44

v64qi right512(v64qi x) {
  return __builtin_ia32_psrldqi512_byteshift(x, 4);
}

// CIR-LABEL: cir.func {{.*}} @right512
// CIR: cir.vec.shuffle(%{{.*}}, %{{.*}} : !cir.vector<64 x !s8i>) [#cir.int<4> : !s32i{{.*}}#cir.int<67> : !s32i, #cir.int<20> : !s32i{{.*}}#cir.int<83> : !s32i, #cir.int<36> : !s32i
// LLVM-LABEL: define {{.*}} <64 x i8> @right512
// LLVM: shufflevector <64 x i8> %{{.*}}, <64 x i8> zeroinitializer, <64 x i32> <i32 4,{{.*}}i32 67, i32 20,{{.*}}i32 83, i32 36

v64qi too_far(v64qi x) {
  return __builtin_ia32_pslldqi512_byteshift(x, 16);
}

// CIR-LABEL: cir.func {{.*}} @too_far
// CIR-NOT: cir.vec.shuffle
// CIR: %[[TOO_FAR_ZERO:.*]] = cir.const #cir.zero : !cir.vector<64 x !s8i>
// CIR: cir.store %[[TOO_FAR_ZERO]],
// LLVM-NOT: shufflevector
// LLVM: store <64 x i8> zeroinitializer
