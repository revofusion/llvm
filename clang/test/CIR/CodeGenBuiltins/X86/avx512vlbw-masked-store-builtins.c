// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512bw -target-feature +avx512vl -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -target-feature +avx512f -target-feature +avx512bw -target-feature +avx512vl -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef char v16qi __attribute__((vector_size(16)));
typedef char v64qi __attribute__((vector_size(64)));
typedef short v32hi __attribute__((vector_size(64)));
typedef short v8hi __attribute__((vector_size(16)));
typedef float v8sf __attribute__((vector_size(32)));
typedef float v16sf __attribute__((vector_size(64)));

void test_storedquqi128_mask(v16qi *ptr, v16qi value, unsigned short mask) {
  __builtin_ia32_storedquqi128_mask(ptr, value, mask);
}

// CIR-LABEL: @test_storedquqi128_mask
// CIR:         %[[PTR:.*]] = cir.load {{.*}}, !cir.ptr<!cir.vector<16 x !s8i>>
// CIR:         %[[VALUE:.*]] = cir.load {{.*}}, !cir.vector<16 x !s8i>
// CIR:         %[[MASK:.*]] = cir.load {{.*}}, !u16i
// CIR:         %[[MASK_VEC:.*]] = cir.cast bitcast %[[MASK]] : !u16i -> !cir.vector<16 x !cir.int<s, 1>>
// CIR:         cir.call_llvm_intrinsic "masked.store" %[[VALUE]], %[[PTR]], %[[MASK_VEC]] : (!cir.vector<16 x !s8i>, !cir.ptr<!cir.vector<16 x !s8i>>, !cir.vector<16 x !cir.int<s, 1>>){{.*}}

// LLVM-LABEL: @test_storedquqi128_mask
// LLVM:         %[[PTR:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:         %[[VALUE:.*]] = load <16 x i8>, ptr %{{.*}}, align 16
// LLVM:         %[[MASK:.*]] = load i16, ptr %{{.*}}, align 2
// LLVM:         %[[MASK_VEC:.*]] = bitcast i16 %[[MASK]] to <16 x i1>
// LLVM:         call void @llvm.masked.store.v16i8.p0(<16 x i8> %[[VALUE]], ptr %[[PTR]], <16 x i1> %[[MASK_VEC]])

void test_storedquqi512_mask(v64qi *ptr, v64qi value,
                             unsigned long long mask) {
  __builtin_ia32_storedquqi512_mask(ptr, value, mask);
}

// CIR-LABEL: @test_storedquqi512_mask
// CIR:         %[[PTR:.*]] = cir.load {{.*}}, !cir.ptr<!cir.vector<64 x !s8i>>
// CIR:         %[[VALUE:.*]] = cir.load {{.*}}, !cir.vector<64 x !s8i>
// CIR:         %[[MASK:.*]] = cir.load {{.*}}, !u64i
// CIR:         %[[MASK_VEC:.*]] = cir.cast bitcast %[[MASK]] : !u64i -> !cir.vector<64 x !cir.int<s, 1>>
// CIR:         cir.call_llvm_intrinsic "masked.store" %[[VALUE]], %[[PTR]], %[[MASK_VEC]] : (!cir.vector<64 x !s8i>, !cir.ptr<!cir.vector<64 x !s8i>>, !cir.vector<64 x !cir.int<s, 1>>){{.*}}

// LLVM-LABEL: @test_storedquqi512_mask
// LLVM:         %[[PTR:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:         %[[VALUE:.*]] = load <64 x i8>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i64, ptr %{{.*}}, align 8
// LLVM:         %[[MASK_VEC:.*]] = bitcast i64 %[[MASK]] to <64 x i1>
// LLVM:         call void @llvm.masked.store.v64i8.p0(<64 x i8> %[[VALUE]], ptr %[[PTR]], <64 x i1> %[[MASK_VEC]])

void test_storedquhi512_mask(v32hi *ptr, v32hi value, unsigned int mask) {
  __builtin_ia32_storedquhi512_mask(ptr, value, mask);
}

// CIR-LABEL: @test_storedquhi512_mask
// CIR:         %[[PTR:.*]] = cir.load {{.*}}, !cir.ptr<!cir.vector<32 x !s16i>>
// CIR:         %[[VALUE:.*]] = cir.load {{.*}}, !cir.vector<32 x !s16i>
// CIR:         %[[MASK:.*]] = cir.load {{.*}}, !u32i
// CIR:         %[[MASK_VEC:.*]] = cir.cast bitcast %[[MASK]] : !u32i -> !cir.vector<32 x !cir.int<s, 1>>
// CIR:         cir.call_llvm_intrinsic "masked.store" %[[VALUE]], %[[PTR]], %[[MASK_VEC]] : (!cir.vector<32 x !s16i>, !cir.ptr<!cir.vector<32 x !s16i>>, !cir.vector<32 x !cir.int<s, 1>>){{.*}}

// LLVM-LABEL: @test_storedquhi512_mask
// LLVM:         %[[PTR:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:         %[[VALUE:.*]] = load <32 x i16>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i32, ptr %{{.*}}, align 4
// LLVM:         %[[MASK_VEC:.*]] = bitcast i32 %[[MASK]] to <32 x i1>
// LLVM:         call void @llvm.masked.store.v32i16.p0(<32 x i16> %[[VALUE]], ptr %[[PTR]], <32 x i1> %[[MASK_VEC]])

void test_storedquhi128_mask(v8hi *ptr, v8hi value, unsigned char mask) {
  __builtin_ia32_storedquhi128_mask(ptr, value, mask);
}

// CIR-LABEL: @test_storedquhi128_mask
// CIR:         %[[PTR:.*]] = cir.load {{.*}}, !cir.ptr<!cir.vector<8 x !s16i>>
// CIR:         %[[VALUE:.*]] = cir.load {{.*}}, !cir.vector<8 x !s16i>
// CIR:         %[[MASK:.*]] = cir.load {{.*}}, !u8i
// CIR:         %[[MASK_VEC:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         cir.call_llvm_intrinsic "masked.store" %[[VALUE]], %[[PTR]], %[[MASK_VEC]] : (!cir.vector<8 x !s16i>, !cir.ptr<!cir.vector<8 x !s16i>>, !cir.vector<8 x !cir.int<s, 1>>){{.*}}

// LLVM-LABEL: @test_storedquhi128_mask
// LLVM:         %[[PTR:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:         %[[VALUE:.*]] = load <8 x i16>, ptr %{{.*}}, align 16
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[MASK_VEC:.*]] = bitcast i8 %[[MASK]] to <8 x i1>
// LLVM:         call void @llvm.masked.store.v8i16.p0(<8 x i16> %[[VALUE]], ptr %[[PTR]], <8 x i1> %[[MASK_VEC]])

void test_storeups256_mask(v8sf *ptr, v8sf value, unsigned char mask) {
  __builtin_ia32_storeups256_mask(ptr, value, mask);
}

// CIR-LABEL: @test_storeups256_mask
// CIR:         %[[PTR:.*]] = cir.load {{.*}}, !cir.ptr<!cir.vector<8 x !cir.float>>
// CIR:         %[[VALUE:.*]] = cir.load {{.*}}, !cir.vector<8 x !cir.float>
// CIR:         %[[MASK:.*]] = cir.load {{.*}}, !u8i
// CIR:         %[[MASK_VEC:.*]] = cir.cast bitcast %[[MASK]] : !u8i -> !cir.vector<8 x !cir.int<s, 1>>
// CIR:         cir.call_llvm_intrinsic "masked.store" %[[VALUE]], %[[PTR]], %[[MASK_VEC]] : (!cir.vector<8 x !cir.float>, !cir.ptr<!cir.vector<8 x !cir.float>>, !cir.vector<8 x !cir.int<s, 1>>){{.*}}

// LLVM-LABEL: @test_storeups256_mask
// LLVM:         %[[PTR:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:         %[[VALUE:.*]] = load <8 x float>, ptr %{{.*}}, align 32
// LLVM:         %[[MASK:.*]] = load i8, ptr %{{.*}}, align 1
// LLVM:         %[[MASK_VEC:.*]] = bitcast i8 %[[MASK]] to <8 x i1>
// LLVM:         call void @llvm.masked.store.v8f32.p0(<8 x float> %[[VALUE]], ptr %[[PTR]], <8 x i1> %[[MASK_VEC]])

void test_storeups512_mask(float *ptr, v16sf value, unsigned short mask) {
  __builtin_ia32_storeups512_mask(ptr, value, mask);
}

// CIR-LABEL: @test_storeups512_mask
// CIR:         %[[PTR:.*]] = cir.load {{.*}}, !cir.ptr<!cir.float>
// CIR:         %[[VALUE:.*]] = cir.load {{.*}}, !cir.vector<16 x !cir.float>
// CIR:         %[[MASK:.*]] = cir.load {{.*}}, !u16i
// CIR:         %[[MASK_VEC:.*]] = cir.cast bitcast %[[MASK]] : !u16i -> !cir.vector<16 x !cir.int<s, 1>>
// CIR:         cir.call_llvm_intrinsic "masked.store" %[[VALUE]], %[[PTR]], %[[MASK_VEC]] : (!cir.vector<16 x !cir.float>, !cir.ptr<!cir.float>, !cir.vector<16 x !cir.int<s, 1>>){{.*}}

// LLVM-LABEL: @test_storeups512_mask
// LLVM:         %[[PTR:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:         %[[VALUE:.*]] = load <16 x float>, ptr %{{.*}}, align 64
// LLVM:         %[[MASK:.*]] = load i16, ptr %{{.*}}, align 2
// LLVM:         %[[MASK_VEC:.*]] = bitcast i16 %[[MASK]] to <16 x i1>
// LLVM:         call void @llvm.masked.store.v16f32.p0(<16 x float> %[[VALUE]], ptr %[[PTR]], <16 x i1> %[[MASK_VEC]])
