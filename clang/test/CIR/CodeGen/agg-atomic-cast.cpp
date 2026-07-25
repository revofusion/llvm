// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR

struct S {
  int data[4];
};

struct Padded {
  char data[3];
};

void non_atomic_to_atomic_cast() {
  S s;
  _Atomic(S) as = s;
  Padded padded;
  _Atomic(Padded) padded_atomic = padded;
}

// CIR-LABEL: cir.func{{.*}}non_atomic_to_atomic_cast
// CIR: %[[S_ADDR:.*]] = cir.alloca "s" {{.*}} : !cir.ptr<!rec_S>
// CIR: %[[AS_ADDR:.*]] = cir.alloca "as" {{.*}} init : !cir.ptr<!rec_S>
// CIR: %[[PADDED_ADDR:.*]] = cir.alloca "padded" {{.*}} : !cir.ptr<!rec_Padded>
// CIR: %[[PADDED_ATOMIC_ADDR:.*]] = cir.alloca "padded_atomic" {{.*}} : !cir.ptr<!rec_anon_struct>
// CIR: cir.copy %[[S_ADDR]] to %[[AS_ADDR]] : !cir.ptr<!rec_S>
// CIR: %[[PADDED_VALUE_ADDR:.*]] = cir.get_member %[[PADDED_ATOMIC_ADDR]][0] {name = ""} : !cir.ptr<!rec_anon_struct> -> !cir.ptr<!rec_Padded>
// CIR: cir.copy %[[PADDED_ADDR]] to %[[PADDED_VALUE_ADDR]] : !cir.ptr<!rec_Padded>
