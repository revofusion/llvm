// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -mconstructor-aliases -fno-rtti -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -mconstructor-aliases -fno-rtti -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t-cir.ll %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -mconstructor-aliases -fno-rtti -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=OGCG --input-file=%t.ll %s

// Note: This test is using -fno-rtti so that we can delay implementation of that
//       handling. When rtti handling for vtables is implemented, that option
//       should be removed.

// A class with multiple inheritance requires an adjustor thunk in the second
// base's vtable slot: it must adjust 'this' from the Right subobject back to
// the most-derived Child object before forwarding to the real method.

class Left {
public:
  virtual void f();
};

class Right {
public:
  virtual void g();
};

class Child : public Left, public Right {
public:
  void f() override;
  void g() override;
};

void Left::f() {}
void Right::g() {}
void Child::f() {}
void Child::g() {}

// The thunk adjusts 'this' by the (negative) offset of Right within Child and
// tail-forwards to Child::g.

// CIR-LABEL: cir.func {{.*}}@_ZThn8_N5Child1gEv
// CIR:         %[[THIS:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!rec_Child>>, !cir.ptr<!rec_Child>
// CIR:         %[[BYTE:.*]] = cir.cast bitcast %[[THIS]] : !cir.ptr<!rec_Child> -> !cir.ptr<!u8i>
// CIR:         %[[OFF:.*]] = cir.const #cir.int<-8> : !s64i
// CIR:         %[[ADJ:.*]] = cir.ptr_stride %[[BYTE]], %[[OFF]] : (!cir.ptr<!u8i>, !s64i) -> !cir.ptr<!u8i>
// CIR:         %[[ADJC:.*]] = cir.cast bitcast %[[ADJ]] : !cir.ptr<!u8i> -> !cir.ptr<!rec_Child>
// CIR:         cir.call @_ZN5Child1gEv(%[[ADJC]]) : (!cir.ptr<!rec_Child>) -> ()

// LLVM-LABEL: define {{.*}}@_ZThn8_N5Child1gEv
// LLVM:         getelementptr i8, ptr {{.*}}, i64 -8
// LLVM:         call void @_ZN5Child1gEv

// OGCG-LABEL: define {{.*}}@_ZThn8_N5Child1gEv
// OGCG:         getelementptr inbounds i8, ptr {{.*}}, i64 -8
// OGCG:         {{(tail )?}}call void @_ZN5Child1gEv
