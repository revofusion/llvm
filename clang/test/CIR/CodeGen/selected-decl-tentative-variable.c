// RUN: printf 'usr:c:@selected_values\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir --implicit-check-not=@unrelated_values

int selected_values[4];
int unrelated_values[4];

// CHECK: cir.global external @selected_values =
