// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t.ll %s

typedef float float4 __attribute__((ext_vector_type(4)));

int nondet_int(int x) {
  return __builtin_nondeterministic_value(x);
}

// CIR-LABEL: cir.func {{.*}} @nondet_int
// CIR: %[[IPOISON:.*]] = cir.const #cir.poison : !s32i
// CIR-NEXT: %[[IFROZEN:.*]] = cir.freeze %[[IPOISON]] : !s32i
// CIR: cir.store %[[IFROZEN]],
// LLVM-LABEL: define {{.*}} i32 @nondet_int
// LLVM: %[[IFROZEN:.*]] = freeze i32 poison
// LLVM: store i32 %[[IFROZEN]],

float nondet_float(float x) {
  return __builtin_nondeterministic_value(x);
}

// CIR-LABEL: cir.func {{.*}} @nondet_float
// CIR: %[[FPOISON:.*]] = cir.const #cir.poison : !cir.float
// CIR-NEXT: %[[FFROZEN:.*]] = cir.freeze %[[FPOISON]] : !cir.float
// LLVM-LABEL: define {{.*}} float @nondet_float
// LLVM: freeze float poison

float4 nondet_vector(float4 x) {
  return __builtin_nondeterministic_value(x);
}

// CIR-LABEL: cir.func {{.*}} @nondet_vector
// CIR: %[[VPOISON:.*]] = cir.const #cir.poison : !cir.vector<4 x !cir.float>
// CIR-NEXT: %[[VFROZEN:.*]] = cir.freeze %[[VPOISON]] : !cir.vector<4 x !cir.float>
// LLVM-LABEL: define {{.*}} <4 x float> @nondet_vector
// LLVM: freeze <4 x float> poison

void diagnostic_marker(void) {
  __warn_memset_zero_len();
}

// CIR-LABEL: cir.func {{.*}} @diagnostic_marker
// CIR-NEXT: cir.return
// LLVM-LABEL: define {{.*}} void @diagnostic_marker
// LLVM-NEXT: ret void
