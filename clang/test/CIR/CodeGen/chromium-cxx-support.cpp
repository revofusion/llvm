// RUN: %clang_cc1 -std=c++20 -triple arm64-apple-macosx -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

enum BoolBacked : bool { FalseValue, TrueValue };

bool equal_bool_backed(BoolBacked lhs, BoolBacked rhs) {
  return lhs == rhs;
}

[[noreturn]] void abort_now();

void no_merge_statement(bool condition) {
  if (condition)
    [[clang::nomerge]] abort_now();
}

struct [[clang::trivial_abi]] TrivialAbiArg {
  void *value;
  ~TrivialAbiArg();
};

struct DelegateTarget {
  explicit DelegateTarget(TrivialAbiArg arg);
};

DelegateTarget::DelegateTarget(TrivialAbiArg arg) {}

// CHECK-LABEL: cir.func {{.*}}equal_bool_backed
// CHECK: cir.cmp eq
// CHECK-LABEL: cir.func {{.*}}no_merge_statement
// CHECK: cir.call {{.*}}abort_now
// CHECK-LABEL: cir.func {{.*}}DelegateTargetC1
// CHECK: cir.cleanup.scope
// CHECK: cir.store {{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK: cir.call {{.*}}DelegateTargetC2
// CHECK: cleanup normal
// CHECK: cir.call {{.*}}TrivialAbiArgD1
