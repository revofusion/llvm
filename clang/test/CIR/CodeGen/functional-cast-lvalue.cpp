// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s

using IntRef = int &;

void functional_cast_lvalue(int &x) {
  IntRef{x} = 7;
}

// CHECK: cir.func{{.*}} @_Z22functional_cast_lvalueRi(%[[ARG:.*]]: !cir.ptr<!s32i>
// CHECK: %[[X_ADDR:.*]] = cir.alloca !cir.ptr<!s32i>, !cir.ptr<!cir.ptr<!s32i>>, ["x", init, const]
// CHECK: cir.store %[[ARG]], %[[X_ADDR]]
// CHECK: %[[SEVEN:.*]] = cir.const #cir.int<7> : !s32i
// CHECK: %[[X:.*]] = cir.load %[[X_ADDR]]
// CHECK: cir.store{{.*}} %[[SEVEN]], %[[X]] : !s32i, !cir.ptr<!s32i>
