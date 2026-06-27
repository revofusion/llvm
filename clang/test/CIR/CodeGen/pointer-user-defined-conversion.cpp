// RUN: %clang_cc1 -std=c++11 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c++11 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s --check-prefix=LLVM

struct pointer_like {
  const char *p;
  operator const char *() const { return p; }
};

int test_index(pointer_like value, unsigned index) {
  return value[index];
}

const char *test_addr(pointer_like value, unsigned index) {
  return &value[index];
}

// CIR-LABEL: cir.func {{.*}} @_Z10test_index12pointer_likej
// CIR: cir.call @_ZNK12pointer_likecvPKcEv
// CIR: cir.load

// CIR-LABEL: cir.func {{.*}} @_Z9test_addr12pointer_likej
// CIR: cir.call @_ZNK12pointer_likecvPKcEv
// CIR: cir.return

// LLVM-LABEL: define{{.*}} @_Z10test_index12pointer_likej
// LLVM: call ptr @_ZNK12pointer_likecvPKcEv
// LLVM: load i8

// LLVM-LABEL: define{{.*}} @_Z9test_addr12pointer_likej
// LLVM: call ptr @_ZNK12pointer_likecvPKcEv
// LLVM: ret ptr
