// RUN: printf 'source-root:8:1:8:52:%s|_Z14selected_valueIiE\nsource-root:8:1:8:52:%s|_Z14selected_valueIlE\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir

// Concrete variable-template specializations share their pattern's exact source
// span. The span owns a set of exact symbols rather than one arbitrary symbol.

template <class T> constexpr bool selected_value = true;
static_assert(selected_value<int>);
static_assert(selected_value<long>);

// CHECK-DAG: cir.global{{.*}} @_Z14selected_valueIiE = #true
// CHECK-DAG: cir.global{{.*}} @_Z14selected_valueIlE = #true
