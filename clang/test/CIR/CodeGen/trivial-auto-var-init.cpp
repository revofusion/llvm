// RUN: %clang_cc1 -std=c++20 -triple arm64-apple-macosx -fclangir -emit-cir -ftrivial-auto-var-init=zero %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

struct Pair {
  int first;
  int second;
};

int scalar() {
  int value;
  return value;
}

int aggregate() {
  Pair value = {1, 2};
  return value.second;
}

// CHECK: cir.global {{.*}}#cir.const_record
// CHECK-LABEL: cir.func {{.*}}scalar
// CHECK: %[[ZERO:.*]] = cir.const #cir.int<0>
// CHECK: cir.store{{.*}} %[[ZERO]]
// CHECK-LABEL: cir.func {{.*}}aggregate
// CHECK: cir.copy
