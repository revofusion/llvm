// RUN: printf '_Z8selectedv\n_Z12selectedUptrv\n_ZN4MoveC1EOS_\n_Z15cxx_identity_fnIiEN13cxx_enable_ifIXeqstT_Li4EEiE4typeES1_\n_Z11selectedVttv\n_ZN10VTableLeafC1Ev\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --implicit-check-not=@_Z9unrelatedv --input-file=%t.cir
// RUN: FileCheck %s --check-prefix=VTABLE-CLOSURE --input-file=%t.cir
// RUN: printf '_ZN11VirtualBaseD1Ev\n_ZN14VirtualDerivedD1Ev\n' > %t.overlap.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.overlap.roots -skip-function-bodies %s -o %t.overlap.cir
// RUN: FileCheck %s --check-prefix=OVERLAP --input-file=%t.overlap.cir
// RUN: printf '_ZN11DtorDerivedC1Ei\n_ZN11DtorDerivedD1Ev\n' > %t.structor-overlap.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.structor-overlap.roots -skip-function-bodies %s -o %t.structor-overlap.cir
// RUN: FileCheck %s --check-prefix=STRUCTOR-OVERLAP --input-file=%t.structor-overlap.cir
// The retained Chromium failures referenced non-virtual D1 entry points for
// named classes in anonymous namespaces.  Select only the body that creates
// those objects; D1/D2 are its exact Itanium ABI companion closure, while D0
// and the unrelated anonymous class remain absent.
// RUN: printf 'anonymousDtorSelected\n' > %t.anonymous-dtor.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.anonymous-dtor.roots -skip-function-bodies %s -o %t.anonymous-dtor.cir
// RUN: FileCheck %s --check-prefix=ANONYMOUS-DTOR --implicit-check-not=@_ZN7network12_GLOBAL__N_124TestCookieChangeListenerD0Ev --implicit-check-not=@_ZN7network12_GLOBAL__N_117UnrelatedListenerD --input-file=%t.anonymous-dtor.cir


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

struct VTableBase {
  virtual ~VTableBase() = default;
};

struct VTableLeaf final : VTableBase {
  VTableLeaf();
};

VTableLeaf::VTableLeaf() = default;

void selectedVtt() {
  FurtherDerived value;
}


namespace network {
namespace {
struct TestCookieChangeListener {
  ~TestCookieChangeListener() { value = 0; }
  int value = 1;
};

struct EchoFakeWithFilter {
  ~EchoFakeWithFilter() { value = 0; }
  int value = 1;
};

struct UnrelatedListener {
  ~UnrelatedListener() { value = 0; }
  int value = 1;
};
} // namespace

extern "C" int anonymousDtorSelected() {
  TestCookieChangeListener first;
  EchoFakeWithFilter second;
  return first.value + second.value;
}
} // namespace network

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
// A selected constructor can defer its vtable until end-of-TU emission. The
// exact deleting-destructor declaration referenced only by that vtable must
// join the dependency fixed point and be emitted once.
// VTABLE-CLOSURE-COUNT-1: cir.func{{.*}}@_ZN10VTableLeafC1Ev{{.*}} {
// VTABLE-CLOSURE-COUNT-1: cir.func{{.*}}@_ZN10VTableLeafD0Ev{{.*}} {

// Selecting both ends of one destructor dependency edge must not recursively
// regenerate the base destructor while the derived body is under construction.
// OVERLAP-COUNT-1: cir.func{{.*}}@_ZN11VirtualBaseD2Ev{{.*}} {
// OVERLAP-COUNT-1: cir.func{{.*}}@_ZN14VirtualDerivedD2Ev{{.*}} {


// Emitting a constructor may recursively materialize destructor dependencies.
// The nested definition must restore the surrounding CIRGenFunction so that a
// separately selected destructor root cannot re-enter the constructor body.
// STRUCTOR-OVERLAP-COUNT-1: cir.func{{.*}}@_ZN11DtorDerivedC1Ei{{.*}} {
// STRUCTOR-OVERLAP-COUNT-1: cir.func{{.*}}@_ZN11DtorDerivedD1Ev{{.*}} {

// One exact root references two anonymous-namespace destructors.  The producer
// emits the complete and base entry points once for each concrete
// CXXDestructorDecl; the non-virtual deleting variants and unreferenced class
// are outside the selected ABI dependency closure.
// ANONYMOUS-DTOR: cir.selected_decl_root_definitions = {anonymousDtorSelected = "anonymousDtorSelected"}
// ANONYMOUS-DTOR-COUNT-1: cir.func{{.*}}@_ZN7network12_GLOBAL__N_118EchoFakeWithFilterD2Ev{{.*}}abi_dtor_variant = "base"{{.*}}ast_decl_usr = "[[ECHO_DTOR_USR:[^"]+]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[ECHO_CLASS:[^"]+]]"{{.*}}method_symbol = @_ZN7network12_GLOBAL__N_118EchoFakeWithFilterD2Ev{{.*}}method_usr = "[[ECHO_DTOR_USR]]"{{.*}} {
// ANONYMOUS-DTOR-COUNT-1: cir.func{{.*}}@_ZN7network12_GLOBAL__N_118EchoFakeWithFilterD1Ev{{.*}}abi_dtor_variant = "complete"{{.*}}ast_decl_usr = "[[ECHO_DTOR_USR]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[ECHO_CLASS]]"{{.*}}method_symbol = @_ZN7network12_GLOBAL__N_118EchoFakeWithFilterD1Ev{{.*}}method_usr = "[[ECHO_DTOR_USR]]"{{.*}} {
// ANONYMOUS-DTOR-COUNT-1: cir.func{{.*}}@_ZN7network12_GLOBAL__N_124TestCookieChangeListenerD2Ev{{.*}}abi_dtor_variant = "base"{{.*}}ast_decl_usr = "[[COOKIE_DTOR_USR:[^"]+]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[COOKIE_CLASS:[^"]+]]"{{.*}}method_symbol = @_ZN7network12_GLOBAL__N_124TestCookieChangeListenerD2Ev{{.*}}method_usr = "[[COOKIE_DTOR_USR]]"{{.*}} {
// ANONYMOUS-DTOR-COUNT-1: cir.func{{.*}}@_ZN7network12_GLOBAL__N_124TestCookieChangeListenerD1Ev{{.*}}abi_dtor_variant = "complete"{{.*}}ast_decl_usr = "[[COOKIE_DTOR_USR]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[COOKIE_CLASS]]"{{.*}}method_symbol = @_ZN7network12_GLOBAL__N_124TestCookieChangeListenerD1Ev{{.*}}method_usr = "[[COOKIE_DTOR_USR]]"{{.*}} {