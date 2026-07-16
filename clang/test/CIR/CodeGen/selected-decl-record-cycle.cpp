// RUN: echo _Z8selectedP1A > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o - | FileCheck %s

struct A;
struct B {
  A *a;
};
struct A {
  B *b;
};

A *selected(A *a) { return a; }
int unused() { return 7; }

// CHECK: !rec_B = !cir.struct<"B" {!cir.ptr<!cir.struct<"A" incomplete>>}>
// CHECK: !rec_A = !cir.struct<"A" {!cir.ptr<!rec_B>}>
// CHECK-LABEL: cir.func {{.*}} @_Z8selectedP1A(
// CHECK-NOT: @_Z6unusedv
