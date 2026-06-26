// RUN: %clang_cc1 -triple arm64-apple-macosx15.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

struct Pair {
  unsigned first;
  unsigned second;
};

union Value {
  signed char tag;
  Pair pair;
};

Value value = {.pair = {1, 2}};

// CHECK: !rec_Pair = !cir.record<struct "Pair" {!u32i, !u32i}>
// CHECK: !rec_Value = !cir.record<union "Value" {!s8i, !rec_Pair}>
// CHECK: cir.global external @value = #cir.const_record<{#cir.int<0> : !s8i, #cir.const_record<{#cir.int<1> : !u32i, #cir.int<2> : !u32i}> : !rec_Pair}> : !rec_Value
