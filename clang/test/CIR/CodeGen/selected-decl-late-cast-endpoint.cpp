// RUN: printf 'usr:c:@F@selected_cast<#I>#*v#\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --implicit-check-not='cir.func' --implicit-check-not=@_Z9unrelatedv --input-file=%t.cir

template <typename T>
struct LateEndpoint;

// Convert this specialization while its endpoint is still provisional. The
// later explicit instantiation completes it while parser/Sema scope is valid.
LateEndpoint<int> *provisional_endpoint;

template <typename T>
void *selected_cast(void *);

// Form the exact specialization from the declaration-only template pattern.
// Sema instantiates its body from the later defining redeclaration below.
using SelectedCastType = decltype(&selected_cast<int>);

template <typename T>
void *selected_cast(void *value) {
  return static_cast<void *>(static_cast<LateEndpoint<T> *>(value));
}

template <typename T>
struct LateEndpoint {
  int field;
};

template struct LateEndpoint<int>;

void unrelated() {}

// The selected body must consume the already-complete endpoint definition and
// bind that exact schema to the same producer USR carried by the cast. CIRGen
// must not request late Sema completion from HandleTranslationUnit.
// CHECK: !rec_LateEndpoint3Cint3E = !cir.struct<"LateEndpoint<int>" {!s32i}>
// CHECK: module {{.*}}cir.record_decl_identities = {"LateEndpoint<int>" = "[[ENDPOINT_USR:[^"]+]]"}
// CHECK-SAME: cir.selected_decl_root_definitions = {"usr:c:@F@selected_cast<#I>#*v#" = "_Z13selected_castIiEPvS0_"}
// CHECK-COUNT-1: cir.func{{.*}} @_Z13selected_castIiEPvS0_
// CHECK-SAME: ast_decl_usr = "c:@F@selected_cast<#I>#*v#"
// CHECK: cir.cast bitcast {{.*}} -> !cir.ptr<!rec_LateEndpoint3Cint3E> {ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: result_record_schema = !rec_LateEndpoint3Cint3E
// CHECK-SAME: result_record_usr = "[[ENDPOINT_USR]]"
