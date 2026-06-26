// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s

struct Pair {
  int first;
  int second;
};

int capture_binding(Pair p) {
  auto [x, y] = p;
  auto l = [x]() { return x + 1; };
  return l();
}

// CHECK: cir.func {{.*}} @_ZZ15capture_binding4PairENK3$_0clEv(
// CHECK:   %[[THIS:.*]] = cir.load {{.*}} : !cir.ptr<!cir.ptr<!rec_anon{{.*}}>>, !cir.ptr<!rec_anon{{.*}}>
// CHECK:   %[[X:.*]] = cir.get_member %[[THIS]][0] {name = "x"} : !cir.ptr<!rec_anon{{.*}}> -> !cir.ptr<!s32i>
// CHECK:   %[[X_VALUE:.*]] = cir.load align(4) %[[X]] : !cir.ptr<!s32i>, !s32i
// CHECK:   %[[ONE:.*]] = cir.const #cir.int<1> : !s32i
// CHECK:   cir.binop(add, %[[X_VALUE]], %[[ONE]]) nsw : !s32i
