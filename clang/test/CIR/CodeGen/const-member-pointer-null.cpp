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

// CHECK: !rec_ExtensionInfo = !cir.record<struct "ExtensionInfo" {!cir.bool, !cir.data_member<!cir.bool in !rec_Extensions>}>
// CHECK: cir.global external @empty = #cir.const_record<{#false, #cir.data_member<null> : !cir.data_member<!cir.bool in !rec_Extensions>}> : !rec_ExtensionInfo
// CHECK: cir.global external @enabled = #cir.const_record<{#true, #cir.data_member<0> : !cir.data_member<!cir.bool in !rec_Extensions>}> : !rec_ExtensionInfo
