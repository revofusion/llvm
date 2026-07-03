// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++14 -fclangir -emit-cir %s -o - | FileCheck %s

// Every class here is non-dynamic (no virtual functions anywhere in the
// hierarchy), so only the virtual-base constructor machinery (is_most_derived
// threading, the vbtable-globals subsystem, and the runtime-conditional
// virtual-base-init guard) is exercised, not vfptr handling.
//
// CHECK lines below were authored by diffing this exact source's CIR output
// against classic (non-CIR) CodeGen's `-emit-llvm` output for the same
// source, per this feature's own design discipline: any CIR/classic
// disagreement here is a bug in the port, not a stylistic difference.

struct V {
  int v;
  V();
};
struct A : virtual V {
  int a;
  A();
};
A::A() : a(1) {}

// A Ctor_Base call site (from C's ctor below) must pass a literal 0, not the
// caller's own is_most_derived value.
struct C : A {
  int c;
  C();
};
C::C() : c(2) {}

// A delegating constructor must forward its own is_most_derived value
// rather than deciding a new one (and skips the vbase guard entirely: it
// never reaches the virtual-base-initializer code path at all).
struct B : virtual V {
  B();
  B(int);
};
B::B(int) {}
B::B() : B(1) {}

// Multiple inheritance with the vbptr at a nonzero offset: the vbtable's
// first entry (-vbPtrOffset) must be negative.
struct M {
  int m;
};
struct D : M, virtual V {
  D();
};
D::D() {}

void force() {
  A a;
  C c;
  B b;
  D d;
}

// CHECK-DAG: cir.global {{.*}}@"??_8A@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<16> : !s32i]>
// CHECK-DAG: cir.global {{.*}}@"??_8C@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<24> : !s32i]>
// CHECK-DAG: cir.global {{.*}}@"??_8B@@7B@" = #cir.const_array<[#cir.int<0> : !s32i, #cir.int<8> : !s32i]>
// CHECK-DAG: cir.global {{.*}}@"??_8D@@7B@" = #cir.const_array<[#cir.int<-8> : !s32i, #cir.int<8> : !s32i]>

// A::A(): takes is_most_derived, guards vbptr store + V's ctor call on it.
// CHECK-LABEL: cir.func {{.*}}@"??0A@@QEAA@XZ"
// CHECK:  [[ISMD_A:%[0-9]+]] = cir.load{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK:  [[ZERO_A:%[0-9]+]] = cir.const #cir.int<0> : !s32i
// CHECK:  [[COND_A:%[0-9]+]] = cir.cmp(ne, [[ISMD_A]], [[ZERO_A]]) : !s32i, !cir.bool
// CHECK:  cir.if [[COND_A]] {
// CHECK:    cir.get_global @"??_8A@@7B@"
// CHECK:    cir.call @"??0V@@QEAA@XZ"
// CHECK:  }

// C::C(): its own guard, then calls A's ctor with a literal 0 (Ctor_Base),
// not a loaded/forwarded value.
// CHECK-LABEL: cir.func {{.*}}@"??0C@@QEAA@XZ"
// CHECK:  [[ISMD_C:%[0-9]+]] = cir.load{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK:  [[ZERO_C:%[0-9]+]] = cir.const #cir.int<0> : !s32i
// CHECK:  [[COND_C:%[0-9]+]] = cir.cmp(ne, [[ISMD_C]], [[ZERO_C]]) : !s32i, !cir.bool
// CHECK:  cir.if [[COND_C]] {
// CHECK:    cir.get_global @"??_8C@@7B@"
// CHECK:    cir.call @"??0V@@QEAA@XZ"
// CHECK:  }
// CHECK:  [[CBASE:%[0-9]+]] = cir.const #cir.int<0> : !s32i
// CHECK:  cir.call @"??0A@@QEAA@XZ"(%{{.*}}, [[CBASE]])

// B(): delegating ctor -- no vbase guard at all, just forwards its own
// loaded is_most_derived value to B(int).
// CHECK-LABEL: cir.func {{.*}}@"??0B@@QEAA@XZ"
// CHECK-NOT: cir.if
// CHECK:  [[FWD:%[0-9]+]] = cir.load{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK:  cir.call @"??0B@@QEAA@H@Z"(%{{.*}}, %{{.*}}, [[FWD]])
