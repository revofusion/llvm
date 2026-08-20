// A selected static data member can be declared while its record type is still
// incomplete and receive its exact initializer from an out-of-line definition
// after the type is completed. Streaming top-level emission must not query
// definition-owned record properties at the declaration; selected-variable
// traversal will revisit the exact definition after parsing completes.
// RUN: printf '_ZN15IncompleteOwner5valueE\n_ZN13CompleteOwner5valueE\n_ZN20DeclarationOnlyOwner6kValueE\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++14 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir

struct IncompletePayload;
struct IncompleteOwner {
  static const IncompletePayload value;
};

struct IncompletePayload {
  int field;
};

const IncompletePayload IncompleteOwner::value = {9};

struct CompletePayload {
  int field;
};

struct CompleteOwner {
  static const CompletePayload value;
};

const CompletePayload CompleteOwner::value = {11};

struct DeclarationOnlyOwner {
  static const int kValue = 13;
};

// CHECK: cir.selected_decl_root_definitions = {
// CHECK-DAG: _ZN15IncompleteOwner5valueE = "_ZN15IncompleteOwner5valueE"
// CHECK-DAG: _ZN13CompleteOwner5valueE = "_ZN13CompleteOwner5valueE"
// CHECK-DAG: _ZN20DeclarationOnlyOwner6kValueE = "_ZN20DeclarationOnlyOwner6kValueE"
// CHECK-DAG: cir.global constant external @_ZN15IncompleteOwner5valueE = #cir.const_record<{#cir.int<9>}> : !rec_IncompletePayload
// CHECK-DAG: cir.global constant external @_ZN13CompleteOwner5valueE = #cir.const_record<{#cir.int<11>}> : !rec_CompletePayload
// CHECK-DAG: cir.global constant available_externally @_ZN20DeclarationOnlyOwner6kValueE = #cir.int<13> : !s32i
