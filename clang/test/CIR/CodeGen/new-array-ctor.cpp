// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t-cir.ll %s

// Test that `new T[n]` with a non-trivial element constructor lowers to a
// constructor loop over the allocated elements (rather than hitting the
// "emitNewArrayInitializer: ctor initializer" NYI).

struct S {
  S();
  int x;
};

void test_new_array_ctor(unsigned long n) {
  S *p = new S[n];
}

// CHECK: cir.func{{.*}} @_Z19test_new_array_ctorm
// A pointer-typed slot tracks the current element being constructed.
// CHECK:   %[[CUR_SLOT:.*]] = cir.alloca !cir.ptr<!rec_S>, !cir.ptr<!cir.ptr<!rec_S>>, ["arrayinit.ctor.cur"
// The allocation goes through _Znam, then we walk every element and call the
// constructor in a do-while loop until the current pointer reaches the end.
// CHECK:   cir.call @_Znam
// CHECK:   cir.store{{.*}}, %[[CUR_SLOT]]
// CHECK:   cir.if {{.*}} {
// CHECK:     cir.do {
// CHECK:       %[[CUR:.*]] = cir.load{{.*}} %[[CUR_SLOT]]
// CHECK:       cir.call @_ZN1SC1Ev(%[[CUR]])
// CHECK:       %[[NEXT:.*]] = cir.ptr_stride
// CHECK:       cir.store{{.*}} %[[NEXT]], %[[CUR_SLOT]]
// CHECK:       cir.yield
// CHECK:     } while {
// CHECK:       %[[CUR2:.*]] = cir.load{{.*}} %[[CUR_SLOT]]
// CHECK:       %[[CMP:.*]] = cir.cmp(ne, %[[CUR2]],
// CHECK:       cir.condition(%[[CMP]])
// CHECK:     }
// CHECK:   }
// CHECK:   cir.return

// LLVM: define{{.*}} void @_Z19test_new_array_ctorm
// LLVM:   call ptr @_Znam
// LLVM:   call void @_ZN1SC1Ev
