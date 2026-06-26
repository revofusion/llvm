// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s

struct S {
  void foo(int);
};

template <auto F>
void call(S *s) {
  (s->*F)(3);
}

void use(S *s) {
  call<&S::foo>(s);
}

// CHECK: cir.func private @_ZN1S3fooEi(!cir.ptr<!rec_S>, !s32i)
// CHECK: cir.func{{.*}} @_Z4callITnDaXadL_ZN1S3fooEiEEEvPS0_(%[[ARG:.*]]: !cir.ptr<!rec_S>
// CHECK: %[[S_ADDR:.*]] = cir.alloca !cir.ptr<!rec_S>, !cir.ptr<!cir.ptr<!rec_S>>, ["s", init]
// CHECK: cir.store %[[ARG]], %[[S_ADDR]]
// CHECK: %[[THIS:.*]] = cir.load{{.*}} %[[S_ADDR]]
// CHECK: %[[THREE:.*]] = cir.const #cir.int<3> : !s32i
// CHECK: cir.call @_ZN1S3fooEi(%[[THIS]], %[[THREE]]) : (!cir.ptr<!rec_S>, !s32i) -> ()
