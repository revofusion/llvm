// RUN: printf '_Z8selectedv\n_ZN12_GLOBAL__N_15LocalD1Ev\nparse-symbol:_Z8selectedv\nparse-symbol:_ZN9BindStateIiE6SelectEv\nparse-symbol:_ZN9BindStateIiE7DestroyEv\nparse-symbol:_ZN12_GLOBAL__N_15LocalD1Ev\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir

template <class T> struct BindState {
  static void Destroy() {}
  static void (*Select())() { return &Destroy; }
};

namespace {
struct Local {
  ~Local() {}
};
} // namespace

int selected() {
  Local local;
  auto destroy = BindState<int>::Select();
  destroy();
  return 0;
}

// CHECK-DAG: cir.func{{.*}} @_Z8selectedv{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN9BindStateIiE6SelectEv{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN9BindStateIiE7DestroyEv{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN12_GLOBAL__N_15LocalD1Ev{{.*}} {
