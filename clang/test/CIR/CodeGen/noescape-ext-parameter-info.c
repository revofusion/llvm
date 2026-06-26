// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s

void takes_noescape(int *__attribute__((noescape)) p);

void f(int *p) {
  takes_noescape(p);
}

// CHECK: cir.func private @takes_noescape(!cir.ptr<!s32i>)
// CHECK: cir.call @takes_noescape(%{{[0-9]+}}) : (!cir.ptr<!s32i>) -> ()
