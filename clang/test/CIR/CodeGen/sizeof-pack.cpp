// RUN: %clang_cc1 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s

template <class... Args>
int count(Args...) {
  return static_cast<int>(sizeof...(Args));
}

int use() { return count(1, 2, 3); }

// CIR-LABEL: cir.func{{.*}} @_Z5countIJiiiEEiDpT_
// CIR: #cir.int<3> : !s32i
