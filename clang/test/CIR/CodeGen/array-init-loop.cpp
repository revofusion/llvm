// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s

int capture_array() {
  int a[2] = {1, 2};
  auto l = [a] { return a[0] + a[1]; };
  return l();
}

// CHECK: cir.func{{.*}} @_Z13capture_arrayv()
// CHECK: %[[IDX_ADDR:.*]] = cir.alloca !u64i, !cir.ptr<!u64i>, ["arrayinit.index"]
// CHECK: %[[CAPTURE_ARRAY:.*]] = cir.get_member {{.*}} {name = "a"}
// CHECK: %[[CAPTURE_BEGIN:.*]] = cir.cast array_to_ptrdecay %[[CAPTURE_ARRAY]]
// CHECK: %[[ZERO:.*]] = cir.const #cir.int<0> : !u64i
// CHECK: cir.store{{.*}} %[[ZERO]], %[[IDX_ADDR]]
// CHECK: %[[END:.*]] = cir.const #cir.int<2> : !u64i
// CHECK: cir.do {
// CHECK: %[[IDX:.*]] = cir.load{{.*}} %[[IDX_ADDR]]
// CHECK: %[[STRIDE:.*]] = cir.cast integral %[[IDX]] : !u64i -> !s64i
// CHECK: %[[DST_ELEM:.*]] = cir.ptr_stride %[[CAPTURE_BEGIN]], %[[STRIDE]]
// CHECK: %[[SRC_ELEM:.*]] = cir.get_element {{.*}}[%[[IDX]] : !u64i]
// CHECK: %[[VAL:.*]] = cir.load{{.*}} %[[SRC_ELEM]]
// CHECK: cir.store{{.*}} %[[VAL]], %[[DST_ELEM]]
// CHECK: %[[ONE:.*]] = cir.const #cir.int<1> : !u64i
// CHECK: %[[NEXT:.*]] = cir.binop(add, %[[IDX]], %[[ONE]]) nuw : !u64i
// CHECK: cir.store{{.*}} %[[NEXT]], %[[IDX_ADDR]]
// CHECK: cir.yield
// CHECK: } while {
// CHECK: %[[COND_IDX:.*]] = cir.load{{.*}} %[[IDX_ADDR]]
// CHECK: cir.cmp(ne, %[[COND_IDX]], %[[END]]) : !u64i, !cir.bool
// CHECK: cir.condition
// CHECK: }
