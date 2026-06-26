// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

struct Range {
  unsigned short first;
  unsigned short last;

  constexpr Range(unsigned short first, unsigned short last)
      : first(first), last(last) {}
};

int sink(Range);

template <Range R>
int call_sink() {
  return sink(R);
}

int test() {
  return call_sink<Range{3, 4}>();
}

// CHECK: cir.global{{.*}} constant {{.*}} = #cir.const_record<{#cir.int<3> : !u16i, #cir.int<4> : !u16i}> : !rec_Range
// CHECK-LABEL: cir.func{{.*}} @_Z9call_sink
// CHECK: %[[TMP:.*]] = cir.alloca !rec_Range
// CHECK: %[[ADDR:.*]] = cir.const #cir.global_view<@
// CHECK: cir.copy %[[ADDR]] to %[[TMP]]
// CHECK: %[[VALUE:.*]] = cir.load{{.*}} %[[TMP]]
// CHECK: cir.call @_Z4sink5Range(%[[VALUE]])
