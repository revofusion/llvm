// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=LLVM

int leaf(int);
int outer(int);

int always_inline_calls(int x) {
  [[clang::always_inline]] outer(leaf(x));
  return outer(x);
}

// CIR-LABEL: cir.func{{.*}} @always_inline_calls(
// CIR:         %[[INLINE_ARG:[^ ]+]] = cir.call @leaf({{[^)]*}}) {always_inline} :
// CIR:         %{{[^ ]+}} = cir.call @outer(%[[INLINE_ARG]]) {always_inline} :
// CIR:         %{{[^ ]+}} = cir.call @outer({{[^)]*}}) :

// LLVM-LABEL: define{{.*}} i32 @always_inline_calls(
// LLVM:         %[[INLINE_ARG:[^ ]+]] = call{{.*}} i32 @leaf({{[^)]*}}) #[[ALWAYS_INLINE:[0-9]+]]
// LLVM:         %{{[^ ]+}} = call{{.*}} i32 @outer({{[^)]*}}) #[[ALWAYS_INLINE]]
// LLVM:         %{{[^ ]+}} = call{{.*}} i32 @outer({{[^)]*}}){{$}}

int musttail_calls(int x) {
  [[clang::musttail]] return outer(leaf(x));
}

// CIR-LABEL: cir.func{{.*}} @musttail_calls(
// CIR:         %[[TAIL_ARG:[^ ]+]] = cir.call @leaf({{[^)]*}}) :
// CIR:         %[[TAIL_RESULT:[^ ]+]] = cir.call @outer(%[[TAIL_ARG]]) musttail :

// LLVM-LABEL: define{{.*}} i32 @musttail_calls(
// LLVM:         %[[TAIL_ARG:[^ ]+]] = call{{.*}} i32 @leaf({{[^)]*}})
// LLVM:         %[[TAIL_RESULT:[^ ]+]] = musttail call{{.*}} i32 @outer({{[^)]*}})
// LLVM-NEXT:    ret i32 %[[TAIL_RESULT]]

// LLVM: attributes #[[ALWAYS_INLINE]] = { alwaysinline }
