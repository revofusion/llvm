// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++14 -fclangir -emit-cir %s -o - | FileCheck %s

#pragma detect_mismatch("cir_msvc_basic", "1")

extern void sink(int *);

struct A {
  int x;
  static const int n = 1;
  int m();
};

int f() { return 1; }

int g(A *a) { return a->x; }

const int *h() { return &A::n; }

int A::m() { return x; }

struct C {
  int x;
  C(int);
};

C::C(int v) : x(v) {}

int makeC() {
  C c(7);
  return c.x;
}

struct D {
  int x;
  ~D();
};

D::~D() { sink(&x); }

// CHECK: cir.triple = "x86_64-pc-windows-msvc"
// CHECK-DAG: cir.global {{.*}}@"?n@A@@2HB"
// CHECK-DAG: cir.func {{.*}}@"?f@@YAHXZ"
// CHECK-DAG: cir.func {{.*}}@"?g@@YAHPEAUA@@@Z"
// CHECK-DAG: cir.func {{.*}}@"?h@@YAPEBHXZ"
// CHECK-DAG: cir.func {{.*}}@"?m@A@@QEAAHXZ"
// CHECK-DAG: cir.func {{.*}}@"??0C@@QEAA@H@Z"{{.*}} -> !cir.ptr<!rec_C>
// CHECK-DAG: cir.func {{.*}}@"?makeC@@YAHXZ"
// CHECK: cir.call @"??0C@@QEAA@H@Z"
// CHECK-DAG: cir.func {{.*}}@"??1D@@QEAA@XZ"
// CHECK: cir.get_member
