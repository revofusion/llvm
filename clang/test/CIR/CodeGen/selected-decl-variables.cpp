// RUN: printf '_ZN5roots8selectedE\n_ZN12StaticHolder8selectedE\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir --implicit-check-not=@_ZN5roots10unrelatedE --implicit-check-not=@_ZN12StaticHolder10unrelatedE

namespace roots {
int selected = 7;
int unrelated = 9;
} // namespace roots

struct StaticHolder {
  static int selected;
  static int unrelated;
};

int StaticHolder::selected = 11;
int StaticHolder::unrelated = 13;

// CHECK-DAG: cir.global external @_ZN5roots8selectedE =
// CHECK-DAG: cir.global external @_ZN12StaticHolder8selectedE =
