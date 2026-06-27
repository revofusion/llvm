// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR

enum class Result { Continue, Revisit };

void attributed_case(Result result, int *it) {
  switch (result) {
  [[likely]] case Result::Continue:
    ++*it;
    break;
  case Result::Revisit:
    break;
  }
}

// CIR-LABEL: cir.func{{.*}} @_Z15attributed_case6ResultPi
// CIR: cir.switch
// CIR: cir.case(equal, [#cir.int<0> : !s32i]) {
// CIR: cir.break
// CIR: cir.case(equal, [#cir.int<1> : !s32i]) {
// CIR: cir.break
