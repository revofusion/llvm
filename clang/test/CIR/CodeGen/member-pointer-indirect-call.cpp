// RUN: %clang_cc1 -triple arm64-apple-macosx15.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

struct C {
  int value;
  int add(int x) { return value + x; }
};

using Method = int (C::*)(int);
Method table[1] = {&C::add};

int call(C *c, int i) { return (c->*table[i])(3); }

// CHECK: !rec_anon_struct = !cir.record<struct  {!cir.ptr<!void>, !s64i}>
// CHECK: cir.global external @table
// CHECK: cir.get_element
// CHECK: cir.extract_member {{.*}}[0] : !rec_anon_struct
// CHECK: cir.extract_member {{.*}}[1] : !rec_anon_struct
// CHECK: cir.ptr_stride
// CHECK: cir.call %
