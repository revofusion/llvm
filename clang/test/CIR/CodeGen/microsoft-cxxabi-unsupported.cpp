// RUN: not %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++14 -fclangir -emit-cir %s -o - 2>&1 | FileCheck %s

struct A {
  virtual int f();
};

int call(A *a) { return a->f(); }

// CHECK: ClangIR code gen Not Yet Implemented: Microsoft C++ ABI virtual function pointer
// CHECK-NOT: UNREACHABLE
// CHECK-NOT: Assertion
