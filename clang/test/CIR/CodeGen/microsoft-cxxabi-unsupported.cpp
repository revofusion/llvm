// RUN: not %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++14 -fclangir -emit-cir %s -o - 2>&1 | FileCheck %s

struct V {
  V();
};
struct A : virtual V {
  A();
};

A::A() {}

// CHECK: ClangIR code gen Not Yet Implemented: Microsoft C++ ABI constructor with virtual bases
// CHECK-NOT: UNREACHABLE
// CHECK-NOT: Assertion
