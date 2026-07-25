// RUN: printf '_Z8selectedv\n_Z12selectedUptrv\n_ZN4MoveC1EOS_\n_Z15cxx_identity_fnIiEN13cxx_enable_ifIXeqstT_Li4EEiE4typeES1_\n_Z11selectedVttv\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --implicit-check-not=@_Z9unrelatedv --input-file=%t.cir
// RUN: printf '_ZN11VirtualBaseD1Ev\n_ZN14VirtualDerivedD1Ev\n' > %t.overlap.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.overlap.roots -skip-function-bodies %s -o %t.overlap.cir
// RUN: FileCheck %s --check-prefix=OVERLAP --input-file=%t.overlap.cir
// RUN: printf '_ZN11DtorDerivedC1Ei\n_ZN11DtorDerivedD1Ev\n' > %t.structor-overlap.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.structor-overlap.roots -skip-function-bodies %s -o %t.structor-overlap.cir
// RUN: FileCheck %s --check-prefix=STRUCTOR-OVERLAP --input-file=%t.structor-overlap.cir

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

struct VirtualBase {
  virtual ~VirtualBase() = default;
};

struct VirtualDerived : virtual VirtualBase {
  VirtualDerived() = default;
  ~VirtualDerived() override = default;
};

struct FurtherDerived : VirtualDerived {
  FurtherDerived() = default;
  ~FurtherDerived() override = default;
};

struct DtorMember {
  DtorMember(int);
  ~DtorMember();
};

struct DtorBase {
  DtorBase(int);
  virtual ~DtorBase() = default;
};

struct DtorDerived final : DtorBase {
  DtorDerived(int value) : DtorBase(value), member(value) {}
  ~DtorDerived() override = default;
  DtorMember member;
};

void selectedVtt() {
  FurtherDerived value;
}

void unrelated() {}

template <bool B, class T = void>
struct cxx_enable_if {};

template <class T>
struct cxx_enable_if<true, T> {
  using type = T;
};

template <class T>
typename cxx_enable_if<sizeof(T) == 4, int>::type cxx_identity_fn(T value) {
  return value + 1;
}

template int cxx_identity_fn<int>(int);

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
// CHECK-DAG: cir.func{{.*}}@_Z15cxx_identity_fnIiEN13cxx_enable_ifIXeqstT_Li4EEiE4typeES1_{{.*}} {
// A base constructor with virtual bases has a hidden VTT argument after
// `this`; source-type metadata must stay on arg0 rather than shifting to VTT.
// CHECK-DAG: cir.func{{.*}} @_ZN14VirtualDerivedC2Ev(%arg0: {{.*}}cir.ast_source_type{{.*}}, %arg1: !cir.ptr<!cir.ptr<!void>>
// Selected virtual-destructor roots retain the complete ABI closure, including
// deleting and virtual-adjustment thunk variants.
// CHECK-DAG: cir.func{{.*}}@_ZN14VirtualDerivedD2Ev{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN14VirtualDerivedD1Ev{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN14VirtualDerivedD0Ev{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZTv0_n24_N14VirtualDerivedD1Ev{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZTv0_n24_N14VirtualDerivedD0Ev{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN14FurtherDerivedD2Ev{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN14FurtherDerivedD1Ev{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN14FurtherDerivedD0Ev{{.*}} {

// Selecting both ends of one destructor dependency edge must not recursively
// regenerate the base destructor while the derived body is under construction.
// OVERLAP-COUNT-1: cir.func{{.*}}@_ZN11VirtualBaseD2Ev{{.*}} {
// OVERLAP-COUNT-1: cir.func{{.*}}@_ZN14VirtualDerivedD2Ev{{.*}} {


// Emitting a constructor may recursively materialize destructor dependencies.
// The nested definition must restore the surrounding CIRGenFunction so that a
// separately selected destructor root cannot re-enter the constructor body.
// STRUCTOR-OVERLAP-COUNT-1: cir.func{{.*}}@_ZN11DtorDerivedC1Ei{{.*}} {
// STRUCTOR-OVERLAP-COUNT-1: cir.func{{.*}}@_ZN11DtorDerivedD1Ev{{.*}} {