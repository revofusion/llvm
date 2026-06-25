// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s

int *launder_int(int *p) {
  return __builtin_launder(p);
}

// CHECK-LABEL: cir.func {{.*}}@_Z11launder_intPi
// CHECK-NOT: __builtin_launder
// CHECK: cir.return %{{.*}} : !cir.ptr<!s32i>
