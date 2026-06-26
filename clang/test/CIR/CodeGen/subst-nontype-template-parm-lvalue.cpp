// RUN: %clang_cc1 -triple arm64-apple-macosx14.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

int global;

template <int &Ref>
void assign_ref() {
  Ref = 7;
}

template void assign_ref<global>();

// CHECK: cir.func no_inline weak_odr @_Z10assign_refIL_Z6globalEEvv()
// CHECK: %[[VAL:.*]] = cir.const #cir.int<7>
// CHECK: %[[ADDR:.*]] = cir.get_global @global
// CHECK: cir.store align(4) %[[VAL]], %[[ADDR]]
