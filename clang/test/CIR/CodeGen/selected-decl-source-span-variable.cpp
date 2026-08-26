// RUN: printf 'source-root:13:1:13:1:%s|_ZL29InitializeSlotIndexesPassFlag\n_Z25initializeSlotIndexesPassv\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir

#define DEFINE_PASS(NAME)                                                     \
  static int Initialize##NAME##PassFlag;                                      \
  static int NAME##OtherFlag;                                                 \
  static void initialize##NAME##PassOnce() {                                  \
    ++Initialize##NAME##PassFlag;                                             \
  }                                                                            \
  void initialize##NAME##Pass() { initialize##NAME##PassOnce(); }

DEFINE_PASS(SlotIndexes)

// One macro invocation owns two variables and two functions with the same
// expansion span. A variable source-root must bind only its exact producer
// symbol, not sibling declarations reached through that span.
// CHECK: cir.selected_decl_root_definitions = {{.*}}_ZL29InitializeSlotIndexesPassFlag = "_ZL29InitializeSlotIndexesPassFlag"
// CHECK: cir.global{{.*}} @_ZL29InitializeSlotIndexesPassFlag = #cir.int<0>
// CHECK-NOT: @_ZL20SlotIndexesOtherFlag
// CHECK: cir.func{{.*}} @_Z25initializeSlotIndexesPassv
