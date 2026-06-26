// RUN: %clang_cc1 -triple arm64-apple-macosx15.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

struct Extensions {
  bool ext;
};

using ExtensionBool = bool Extensions::*;

struct ExtensionInfo {
  bool requestable = false;
  ExtensionBool member = nullptr;
};

ExtensionInfo empty;
ExtensionInfo enabled = {true, &Extensions::ext};

// CHECK: !rec_ExtensionInfo$cxxabi = !cir.record<struct "ExtensionInfo$cxxabi" {!cir.bool, !s64i}>
// CHECK: cir.global external @empty = #cir.const_record<{#false, #cir.int<-1> : !s64i}> : !rec_ExtensionInfo$cxxabi
// CHECK: cir.global external @enabled = #cir.const_record<{#true, #cir.int<0> : !s64i}> : !rec_ExtensionInfo$cxxabi
