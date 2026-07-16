// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s
//
// A record DAG is intentionally printed through each record's canonical alias.
// The alias collector must retain child edges only for deferred aliases: all
// record aliases below are non-deferred and the textual CIR remains unchanged.

struct Leaf {
  int value;
};

struct Left {
  Leaf leaf;
};

struct Right {
  Leaf leaf;
};

struct Root {
  Left left;
  Right right;
};

Root root;

// CHECK-DAG: !rec_Leaf = !cir.struct<"Leaf" {!s32i}>
// CHECK-DAG: !rec_Left = !cir.struct<"Left" {!rec_Leaf}>
// CHECK-DAG: !rec_Right = !cir.struct<"Right" {!rec_Leaf}>
// CHECK-DAG: !rec_Root = !cir.struct<"Root" {!rec_Left, !rec_Right}>
// CHECK: cir.global external @root = #cir.zero : !rec_Root
