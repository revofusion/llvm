// RUN: %clang_cc1 -std=c++11 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: cir-opt %t.cir --verify-roundtrip -o %t-roundtrip.cir
// RUN: FileCheck --input-file=%t-roundtrip.cir %s -check-prefix=CIR-ROUNDTRIP
// RUN: %clang_cc1 -std=c++11 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s -check-prefix=LLVM
// RUN: %clang_cc1 -std=c++11 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s -check-prefix=OGCG
// RUN: %clang_cc1 -std=c++11 -triple i386-unknown-linux-gnu -fclangir -emit-cir %s -o %t-i386.cir
// RUN: FileCheck --input-file=%t-i386.cir %s -check-prefix=CIR-I386
// RUN: %clang_cc1 -std=c++11 -triple i386-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-i386-cir.ll
// RUN: FileCheck --input-file=%t-i386-cir.ll %s -check-prefix=LLVM-I386
// RUN: %clang_cc1 -std=c++11 -triple i386-unknown-linux-gnu -emit-llvm %s -o %t-i386.ll
// RUN: FileCheck --input-file=%t-i386.ll %s -check-prefix=OGCG-I386

bool is_aligned_pointer(void *pointer, unsigned long alignment) {
  return __builtin_is_aligned(pointer, alignment);
}

// CIR-LABEL: cir.func{{.*}}is_aligned_pointer
// CIR: cir.cast ptr_to_int %{{.*}} : !cir.ptr<!void> -> !u64i
// CIR: cir.sub %{{.*}}, %{{.*}} : !u64i
// CIR: cir.and %{{.*}}, %{{.*}} : !u64i
// CIR: cir.cmp eq %{{.*}}, %{{.*}} : !u64i
// CIR-ROUNDTRIP: cir.cast ptr_to_int
// CIR-ROUNDTRIP: cir.sub
// CIR-ROUNDTRIP: cir.and
// CIR-ROUNDTRIP: cir.cmp eq
// LLVM-LABEL: define{{.*}}i1 @{{.*}}is_aligned_pointer
// LLVM: ptrtoint ptr %{{.*}} to i64
// LLVM: sub i64
// LLVM: and i64
// LLVM: icmp eq i64
// OGCG-LABEL: define{{.*}}i1 @{{.*}}is_aligned_pointer
// OGCG-DAG: ptrtoint ptr %{{.*}} to i64
// OGCG-DAG: sub i64
// OGCG: and i64
// OGCG: icmp eq i64

bool is_aligned_integer(unsigned long value, unsigned long alignment) {
  return __builtin_is_aligned(value, alignment);
}

// CIR-LABEL: cir.func{{.*}}is_aligned_integer
// CIR: cir.and %{{.*}}, %{{.*}} : !u64i
// LLVM-LABEL: define{{.*}}i1 @{{.*}}is_aligned_integer
// LLVM: and i64
// OGCG-LABEL: define{{.*}}i1 @{{.*}}is_aligned_integer
// OGCG: and i64

bool is_aligned_array(unsigned long alignment) {
  int values[4];
  return __builtin_is_aligned(values, alignment);
}

// CIR-LABEL: cir.func{{.*}}is_aligned_array
// CIR: cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!s32i x 4>> -> !cir.ptr<!s32i>
// CIR: cir.cast ptr_to_int %{{.*}} : !cir.ptr<!s32i> -> !u64i
// LLVM-LABEL: define{{.*}}i1 @{{.*}}is_aligned_array
// LLVM: ptrtoint ptr %{{.*}} to i64
// OGCG-LABEL: define{{.*}}i1 @{{.*}}is_aligned_array
// OGCG: ptrtoint ptr %{{.*}} to i64

bool is_aligned_cross_width(void *pointer, unsigned long long alignment) {
  return __builtin_is_aligned(pointer, alignment);
}

// CIR-I386-LABEL: cir.func{{.*}}is_aligned_cross_width
// CIR-I386: cir.cast ptr_to_int %{{.*}} : !cir.ptr<!void> -> !u32i
// CIR-I386: cir.cast integral %{{.*}} : !u64i -> !u32i
// CIR-I386: cir.sub %{{.*}}, %{{.*}} : !u32i
// CIR-I386: cir.and %{{.*}}, %{{.*}} : !u32i
// CIR-I386: cir.cmp eq %{{.*}}, %{{.*}} : !u32i
// LLVM-I386-LABEL: define{{.*}}i1 @{{.*}}is_aligned_cross_width
// LLVM-I386: ptrtoint ptr %{{.*}} to i32
// LLVM-I386: trunc i64 %{{.*}} to i32
// LLVM-I386: sub i32
// LLVM-I386: and i32
// LLVM-I386: icmp eq i32
// OGCG-I386-LABEL: define{{.*}}i1 @{{.*}}is_aligned_cross_width
// OGCG-I386-DAG: ptrtoint ptr %{{.*}} to i32
// OGCG-I386-DAG: trunc i64 %{{.*}} to i32
// OGCG-I386-DAG: sub i32
// OGCG-I386: and i32
// OGCG-I386: icmp eq i32

using AS1VoidPtr = void __attribute__((address_space(1))) *;
bool is_aligned_address_space(AS1VoidPtr pointer, unsigned alignment) {
  return __builtin_is_aligned(pointer, alignment);
}

// CIR-I386-LABEL: cir.func{{.*}}is_aligned_address_space
// CIR-I386: cir.cast ptr_to_int %{{.*}} : !cir.ptr<!void, target_address_space(1)> -> !u32i
// CIR-I386: cir.sub %{{.*}}, %{{.*}} : !u32i
// CIR-I386: cir.and %{{.*}}, %{{.*}} : !u32i
// CIR-I386: cir.cmp eq %{{.*}}, %{{.*}} : !u32i
// LLVM-I386-LABEL: define{{.*}}i1 @{{.*}}is_aligned_address_space
// LLVM-I386: ptrtoint ptr addrspace(1) %{{.*}} to i32
// LLVM-I386: icmp eq i32
// OGCG-I386-LABEL: define{{.*}}i1 @{{.*}}is_aligned_address_space
// OGCG-I386: ptrtoint ptr addrspace(1) %{{.*}} to i32
// OGCG-I386: icmp eq i32
