// RUN: %clang_cc1 -std=c11 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: cir-opt %t.cir --verify-roundtrip -o %t-roundtrip.cir
// RUN: FileCheck --input-file=%t-roundtrip.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c11 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s --check-prefix=LLVM
// RUN: %clang_cc1 -std=c11 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=OGCG
// RUN: %clang_cc1 -std=c11 -triple i386-unknown-linux-gnu -fclangir -emit-cir %s -o %t-i386.cir
// RUN: cir-opt %t-i386.cir --verify-roundtrip -o %t-i386-roundtrip.cir
// RUN: FileCheck --input-file=%t-i386-roundtrip.cir %s --check-prefix=CIR-I386
// RUN: %clang_cc1 -std=c11 -triple i386-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-i386-cir.ll
// RUN: FileCheck --input-file=%t-i386-cir.ll %s --check-prefix=LLVM-I386
// RUN: %clang_cc1 -std=c11 -triple i386-unknown-linux-gnu -emit-llvm %s -o %t-i386.ll
// RUN: FileCheck --input-file=%t-i386.ll %s --check-prefix=OGCG-I386

struct AtomicS1 {
  short x, y, z;
};

_Atomic(struct AtomicS1) nonatomic_to_atomic(struct AtomicS1 input) {
  return input;
}

// CIR: cir.global{{.*}}@__const.nonatomic_to_atomic.__retval = #cir.zero : !rec_anon_struct
// CIR-LABEL: cir.func{{.*}}nonatomic_to_atomic
// CIR-NOT: cir.atomic.
// CIR: cir.get_global @__const.nonatomic_to_atomic.__retval
// CIR: cir.copy
// CIR: cir.get_member %{{.*}}[0] {name = ""}
// CIR: cir.copy
// CIR: cir.return
// LLVM-LABEL: define{{.*}}nonatomic_to_atomic
// LLVM-NOT: atomicrmw
// LLVM: ret
// OGCG-LABEL: define{{.*}}nonatomic_to_atomic
// OGCG: ret
// CIR-I386-LABEL: cir.func{{.*}}nonatomic_to_atomic
// CIR-I386: cir.get_member %{{.*}}[0] {name = ""}
// CIR-I386: cir.copy
// CIR-I386: cir.return
// LLVM-I386-LABEL: define{{.*}}nonatomic_to_atomic
// LLVM-I386-NOT: atomicrmw
// LLVM-I386: ret
// OGCG-I386-LABEL: define{{.*}}nonatomic_to_atomic
// OGCG-I386: ret

struct AtomicS1 atomic_to_nonatomic(_Atomic(struct AtomicS1) *input) {
  return *input;
}

// CIR-LABEL: cir.func{{.*}}atomic_to_nonatomic
// CIR-NOT: cir.atomic.
// CIR: cir.get_member %{{.*}}[0]
// CIR: cir.copy
// CIR: cir.return
// LLVM-LABEL: define{{.*}}atomic_to_nonatomic
// LLVM-NOT: atomicrmw
// LLVM: ret
// OGCG-LABEL: define{{.*}}atomic_to_nonatomic
// OGCG: load atomic
// OGCG: ret
// CIR-I386-LABEL: cir.func{{.*}}atomic_to_nonatomic
// CIR-I386-NOT: cir.atomic.
// CIR-I386: cir.return

struct NestedAtomic {
  _Atomic(struct AtomicS1) value;
};

struct AtomicS1 nested_reverse(struct NestedAtomic *input) {
  return input->value;
}

// CIR-LABEL: cir.func{{.*}}nested_reverse
// CIR-NOT: cir.atomic.
// CIR: cir.get_member %{{.*}}[0]
// CIR: cir.get_member %{{.*}}[0]
// CIR: cir.copy
// CIR: cir.return
// LLVM-LABEL: define{{.*}}nested_reverse
// LLVM-NOT: atomicrmw
// LLVM: ret
// OGCG-LABEL: define{{.*}}nested_reverse
// OGCG: load atomic
// OGCG: ret

_Atomic(struct AtomicS1) *get_atomic_source(void);

void discard_reverse_conversion(void) {
  (void)*get_atomic_source();
}

// CIR-LABEL: cir.func{{.*}}discard_reverse_conversion
// CIR: cir.call{{.*}}get_atomic_source
// CIR-NOT: cir.atomic.
// CIR: cir.return
