// RUN: %clang_cc1 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s

struct Ptr {
  int *p;
  operator int *() const { return p; }
};

int load(Ptr &ptr) { return *ptr; }
// CIR-LABEL: cir.func{{.*}} @_Z4loadR3Ptr
// CIR: cir.call @_ZNK3PtrcvPiEv
// CIR: cir.load

void store(Ptr &ptr, int value) { *ptr = value; }
// CIR-LABEL: cir.func{{.*}} @_Z5storeR3Ptri
// CIR: cir.call @_ZNK3PtrcvPiEv
// CIR: cir.store
