// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef int int4 __attribute__((ext_vector_type(4)));
typedef bool bool4 __attribute__((ext_vector_type(4)));

extern "C" int reduce_and_int(int4 value) {
  return __builtin_reduce_and(value);
}

// CIR-LABEL: cir.func{{.*}} @reduce_and_int
// CIR:         %[[INT_VALUE:.*]] = cir.load{{.*}} : !cir.ptr<!cir.vector<4 x !s32i>>, !cir.vector<4 x !s32i>
// CIR:         %[[INT_REDUCED:.*]] = cir.call_llvm_intrinsic "vector.reduce.and" %[[INT_VALUE]] : (!cir.vector<4 x !s32i>) -> !s32i
// CIR:         cir.return %{{.*}} : !s32i
// LLVM-LABEL: define{{.*}} i32 @reduce_and_int
// LLVM:         %[[INT_REDUCED:.*]] = call i32 @llvm.vector.reduce.and.v4i32(<4 x i32> %{{.*}})
// LLVM:         ret i32 %{{.*}}

extern "C" bool reduce_and_bool(int4 value) {
  return __builtin_reduce_and(__builtin_convertvector(value, bool4));
}

// CIR-LABEL: cir.func{{.*}} @reduce_and_bool
// CIR:         %[[BOOL_VALUE:.*]] = cir.cast int_to_bool %{{.*}} : !cir.vector<4 x !s32i> -> !cir.vector<4 x !cir.bool>
// CIR:         %[[BOOL_REDUCED:.*]] = cir.call_llvm_intrinsic "vector.reduce.and" %[[BOOL_VALUE]] : (!cir.vector<4 x !cir.bool>) -> !cir.bool
// CIR:         cir.return %{{.*}} : !cir.bool
// LLVM-LABEL: define{{.*}} i1 @reduce_and_bool
// LLVM:         %[[BOOL_VALUE:.*]] = icmp ne <4 x i32> %{{.*}}, zeroinitializer
// LLVM:         %[[BOOL_REDUCED:.*]] = call i1 @llvm.vector.reduce.and.v4i1(<4 x i1> %[[BOOL_VALUE]])
// LLVM:         ret i1 %{{.*}}

extern "C" void nontemporal_store_scalar(const int *src, int *dst) {
  __builtin_nontemporal_store(*src, dst);
}

// The value must be loaded from src and stored directly to dst; the builtin
// must not turn into a load from dst or an ordinary store through src.
// CIR-LABEL: cir.func{{.*}} @nontemporal_store_scalar
// CIR:         %[[SCALAR_SRC:.*]] = cir.load{{.*}} %{{.*}} : !cir.ptr<!cir.ptr<!s32i>>, !cir.ptr<!s32i>
// CIR:         %[[SCALAR_VALUE:.*]] = cir.load align(4) %[[SCALAR_SRC]] : !cir.ptr<!s32i>, !s32i
// CIR:         %[[SCALAR_DST:.*]] = cir.load{{.*}} %{{.*}} : !cir.ptr<!cir.ptr<!s32i>>, !cir.ptr<!s32i>
// CIR-NOT:     cir.load{{.*}} %[[SCALAR_DST]]
// CIR-NOT:     cir.store{{.*}} %{{.*}}, %[[SCALAR_SRC]]
// CIR:         cir.store nontemporal align(4) %[[SCALAR_VALUE]], %[[SCALAR_DST]] : !s32i, !cir.ptr<!s32i>
// CIR:         cir.return
// LLVM-LABEL: define{{.*}} void @nontemporal_store_scalar
// LLVM:         %[[SCALAR_SRC:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:         %[[SCALAR_VALUE:.*]] = load i32, ptr %[[SCALAR_SRC]], align 4
// LLVM:         %[[SCALAR_DST:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM-NOT:     load i32, ptr %[[SCALAR_DST]]
// LLVM-NOT:     store i32 {{.*}}, ptr %[[SCALAR_SRC]]
// LLVM:         store i32 %[[SCALAR_VALUE]], ptr %[[SCALAR_DST]], align 4, !nontemporal ![[NT:[0-9]+]]
// LLVM:         ret void

extern "C" void nontemporal_store_vector(const int4 *src, int4 *dst) {
  __builtin_nontemporal_store(*src, dst);
}

// CIR-LABEL: cir.func{{.*}} @nontemporal_store_vector
// CIR:         %[[VECTOR_SRC:.*]] = cir.load{{.*}} %{{.*}} : !cir.ptr<!cir.ptr<!cir.vector<4 x !s32i>>>, !cir.ptr<!cir.vector<4 x !s32i>>
// CIR:         %[[VECTOR_VALUE:.*]] = cir.load align(16) %[[VECTOR_SRC]] : !cir.ptr<!cir.vector<4 x !s32i>>, !cir.vector<4 x !s32i>
// CIR:         %[[VECTOR_DST:.*]] = cir.load{{.*}} %{{.*}} : !cir.ptr<!cir.ptr<!cir.vector<4 x !s32i>>>, !cir.ptr<!cir.vector<4 x !s32i>>
// CIR-NOT:     cir.load{{.*}} %[[VECTOR_DST]]
// CIR-NOT:     cir.store{{.*}} %{{.*}}, %[[VECTOR_SRC]]
// CIR:         cir.store nontemporal align(16) %[[VECTOR_VALUE]], %[[VECTOR_DST]] : !cir.vector<4 x !s32i>, !cir.ptr<!cir.vector<4 x !s32i>>
// CIR:         cir.return
// LLVM-LABEL: define{{.*}} void @nontemporal_store_vector
// LLVM:         %[[VECTOR_SRC:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM:         %[[VECTOR_VALUE:.*]] = load <4 x i32>, ptr %[[VECTOR_SRC]], align 16
// LLVM:         %[[VECTOR_DST:.*]] = load ptr, ptr %{{.*}}, align 8
// LLVM-NOT:     load <4 x i32>, ptr %[[VECTOR_DST]]
// LLVM-NOT:     store <4 x i32> {{.*}}, ptr %[[VECTOR_SRC]]
// LLVM:         store <4 x i32> %[[VECTOR_VALUE]], ptr %[[VECTOR_DST]], align 16, !nontemporal ![[NT]]
// LLVM:         ret void

// LLVM: ![[NT]] = !{i32 1}
