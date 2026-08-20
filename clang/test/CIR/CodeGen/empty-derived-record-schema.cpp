// An empty base subobject has no projected source field and no state to copy.
// The derived record therefore has a producer-owned empty object schema too.
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir %s -o - | FileCheck %s

struct EmptyBase {};
struct EmptyDerived : EmptyBase {};

void assign(EmptyDerived &destination, const EmptyDerived &source) {
  destination = source;
}

// CHECK: cir.empty_record_schemas = {EmptyBase, EmptyDerived}
// CHECK: special_member<#cir.cxx_assign<!rec_EmptyDerived, copy, trivial true>>
