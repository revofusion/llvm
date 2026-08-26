// RUN: printf '_Z8selectedv\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir --implicit-check-not=declval

template <typename T>
T &&declval() {
  static_assert(sizeof(T) == 0, "an unevaluated callee must not be emitted");
}

template <typename T>
int evaluatedDependency(T value) {
  using Probe = decltype(declval<T>());
  return value;
}

int selected() { return evaluatedDependency(1); }

// The selected dependency closure follows only potentially-evaluated call
// edges. The decltype operand remains an AST identity fact, never a runtime
// definition request.
// CHECK-DAG: cir.func{{.*}} @_Z8selectedv{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_Z19evaluatedDependencyIiEiT_{{.*}} {
