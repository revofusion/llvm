// RUN: not %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++14 -fclangir -emit-cir %s -o - 2>&1 | FileCheck %s

struct V {
  V();
  ~V();
};
struct B : virtual V {
  ~B();
};

B::~B() {}

// CHECK: ClangIR code gen Not Yet Implemented: Microsoft C++ ABI destructor with virtual bases
// CHECK-NOT: UNREACHABLE
// CHECK-NOT: Assertion
