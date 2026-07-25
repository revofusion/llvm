// RUN: printf 'selected\n' > %t.symbol-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.symbol-roots -skip-function-bodies %s -o %t.symbol.cir
// RUN: FileCheck %s --implicit-check-not=@_Z9unrelatedv --input-file=%t.symbol.cir
// RUN: printf 'usr:c:@F@selected\n' > %t.usr-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.usr-roots -skip-function-bodies %s -o %t.usr.cir
// RUN: FileCheck %s --implicit-check-not=@_Z9unrelatedv --input-file=%t.usr.cir
// RUN: printf '_Z7missingv\nusr:c:@F@missing\n' > %t.missing-roots
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.missing-roots -skip-function-bodies %s -o %t.missing.cir 2>&1 | FileCheck %s --check-prefix=MISSING

struct Payload {
  Payload(Payload &&);
};

struct Leaf {
  Payload payload;
  Leaf(Leaf &&) = default;
};

template <typename T>
struct Holder {
  T value;
  Holder(Holder &&) = default;
};

extern "C" Holder<Leaf> selected(Holder<Leaf> &&value) {
  return static_cast<Holder<Leaf> &&>(value);
}

void unrelated() {}

// Defining the selected function discovers the defaulted Holder constructor;
// defining that constructor discovers Leaf's defaulted constructor. The exact
// dependency frontier must be materialized without rewalking live template
// specialization collections.
// CHECK-DAG: cir.func{{.*}} @selected{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN6HolderI4LeafEC2EOS1_{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN6HolderI4LeafEC1EOS1_{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN4LeafC2EOS_{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN4LeafC1EOS_{{.*}} {

// MISSING-DAG: error: failed to emit exact selected declaration symbol '_Z7missingv': no CIR definition was produced
// MISSING-DAG: error: failed to emit exact selected declaration USR 'c:@F@missing': no CIR definition was produced
