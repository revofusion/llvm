// RUN: printf '_Z8selectedv\n_Z12selectedUptrv\n_ZN4MoveC1EOS_\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --implicit-check-not=@_Z9unrelatedv --input-file=%t.cir

struct Inner {
  ~Inner();
};

Inner::~Inner() {}

struct Outer {
  Inner inner;
};

template <typename T>
struct RawPtrTraits {
  using StorageType = T *;
};

template <typename T, typename Traits = RawPtrTraits<T>>
struct raw_uptr {
  typename Traits::StorageType ptr;
  Inner inner;
};

void selectedUptr() {
  raw_uptr<int> ptr;
}

struct MoveMember {
  MoveMember(MoveMember &&);
};

struct Move {
  MoveMember member;
  Move(Move &&) = default;
};

void unrelated() {}

void selected() {
  Outer outer;
}

// The selected root set includes the implicit Outer::~Outer cleanup edge even
// though that special member was never visited as a top-level declaration.
// Its synthesized body must survive selected-root filtering, while unrelated
// functions remain omitted.
// CHECK-DAG: cir.func{{.*}}@_Z8selectedv{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN5OuterD1Ev{{.*}} {
// The selected root directly references an implicit template-specialization
// destructor. Its body and the nested Inner destructor must be materialized.
// CHECK-DAG: cir.func{{.*}}@_Z12selectedUptrv{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN8raw_uptrIi{{.*}}D1Ev{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN5InnerD1Ev{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN4MoveC1EOS_{{.*}} {
