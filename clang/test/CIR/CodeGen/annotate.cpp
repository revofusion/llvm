// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s --check-prefix=LLVM
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=OGCG

int gvar __attribute__((annotate("global_var_anno")));
int gvar_args __attribute__((annotate("withargs", "extra", 42)));

__attribute__((annotate("func_anno"))) int foo(int x) { return x + 1; }

__attribute__((annotate("func_anno_args", "fa", 7))) void bar(void) {}

// The annotation is recorded as an attribute on each cir.global / cir.func, and
// the module carries an aggregated cir.global_annotations attribute that the
// CIR-to-LLVM lowering turns into @llvm.global.annotations.

// CIR: cir.global_annotations
// CIR-SAME: ["gvar", #cir.annotation<name = "global_var_anno", args = []>]
// CIR-SAME: ["gvar_args", #cir.annotation<name = "withargs", args = ["extra", 42 : i32]>]
// CIR-SAME: ["_Z3fooi", #cir.annotation<name = "func_anno", args = []>]
// CIR-SAME: ["_Z3barv", #cir.annotation<name = "func_anno_args", args = ["fa", 7 : i32]>]

// CIR: cir.global external @gvar = #cir.int<0> : !s32i [#cir.annotation<name = "global_var_anno", args = []>]
// CIR: cir.global external @gvar_args = #cir.int<0> : !s32i [#cir.annotation<name = "withargs", args = ["extra", 42 : i32]>]
// CIR: cir.func {{.*}}@_Z3fooi({{.*}}) -> !s32i [#cir.annotation<name = "func_anno", args = []>]
// CIR: cir.func {{.*}}@_Z3barv() [#cir.annotation<name = "func_anno_args", args = ["fa", 7 : i32]>]

// The CIR-to-LLVM lowering emits the same @llvm.global.annotations appending
// global of { ptr, ptr, ptr, i32, ptr } structs that classic codegen emits.

// LLVM: @llvm.global.annotations = appending global [4 x { ptr, ptr, ptr, i32, ptr }]
// LLVM-SAME: ptr @gvar
// LLVM-SAME: ptr @gvar_args
// LLVM-SAME: ptr @_Z3fooi
// LLVM-SAME: ptr @_Z3barv
// LLVM-SAME: section "llvm.metadata"

// OGCG: @llvm.global.annotations = appending global [4 x { ptr, ptr, ptr, i32, ptr }]
// OGCG-SAME: ptr @gvar
// OGCG-SAME: ptr @gvar_args
// OGCG-SAME: ptr @_Z3fooi
// OGCG-SAME: ptr @_Z3barv
// OGCG-SAME: section "llvm.metadata"
