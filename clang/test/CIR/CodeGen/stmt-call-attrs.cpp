// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -fclangir-aeneas-metadata -emit-cir %s -o - | FileCheck %s --check-prefix=IDENTITY
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=LLVM

int nested(int);

struct Callables {
  int member(int);
  int operator()(int);
  int musttail_member(int);
  int musttail_operator(int);
};

extern "C" int always_inline_cxx(Callables &calls, int x) {
  [[clang::always_inline]] calls(calls.member(nested(x)));
  return calls.member(x);
}

// CIR-LABEL: cir.func{{.*}} @always_inline_cxx(
// CIR:         %{{[^ ]+}} = cir.call @_Z6nestedi({{[^)]*}}) {always_inline} :
// CIR:         %{{[^ ]+}} = cir.call @_ZN9Callables6memberEi({{[^)]*}}) {always_inline} :
// CIR:         %{{[^ ]+}} = cir.call @_ZN9CallablesclEi({{[^)]*}}) {always_inline} :
// CIR:         %{{[^ ]+}} = cir.call @_ZN9Callables6memberEi({{[^)]*}}) :
// IDENTITY-LABEL: cir.func{{.*}} @always_inline_cxx(
// IDENTITY: cir.call @_Z6nestedi({{[^)]*}}) {always_inline, ast_callee_usr = "c:@F@nested#I#"} :
// IDENTITY: cir.call @_ZN9Callables6memberEi({{[^)]*}}) {always_inline, ast_callee_usr = "c:@S@Callables@F@member#I#"} :
// IDENTITY: cir.call @_ZN9CallablesclEi({{[^)]*}}) {always_inline, ast_callee_usr = "c:@S@Callables@F@operator()#I#"} :
// IDENTITY: cir.call @_ZN9Callables6memberEi({{[^)]*}}) {ast_callee_usr = "c:@S@Callables@F@member#I#"} :

// LLVM-LABEL: define{{.*}} i32 @always_inline_cxx(
// LLVM:         %{{[^ ]+}} = call{{.*}} i32 @_Z6nestedi({{[^)]*}}) #[[ALWAYS_INLINE:[0-9]+]]
// LLVM:         %{{[^ ]+}} = call{{.*}} i32 @_ZN9Callables6memberEi({{.*}}) #[[ALWAYS_INLINE]]
// LLVM:         %{{[^ ]+}} = call{{.*}} i32 @_ZN9CallablesclEi({{.*}}) #[[ALWAYS_INLINE]]
// LLVM:         %{{[^ ]+}} = call{{.*}} i32 @_ZN9Callables6memberEi({{.*}}){{$}}

int Callables::musttail_member(int x) {
  [[clang::musttail]] return member(nested(x));
}

// CIR-LABEL: cir.func{{.*}} @_ZN9Callables15musttail_memberEi(
// CIR:         %[[MEMBER_ARG:[^ ]+]] = cir.call @_Z6nestedi({{[^)]*}}) :
// CIR:         %[[MEMBER_RESULT:[^ ]+]] = cir.call @_ZN9Callables6memberEi({{[^)]*}}) musttail :

// LLVM-LABEL: define{{.*}} i32 @_ZN9Callables15musttail_memberEi(
// LLVM:         %[[MEMBER_ARG:[^ ]+]] = call{{.*}} i32 @_Z6nestedi({{[^)]*}})
// LLVM:         %[[MEMBER_RESULT:[^ ]+]] = musttail call{{.*}} i32 @_ZN9Callables6memberEi({{.*}})
// LLVM-NEXT:    ret i32 %[[MEMBER_RESULT]]

int Callables::musttail_operator(int x) {
  [[clang::musttail]] return (*this)(nested(x));
}

// CIR-LABEL: cir.func{{.*}} @_ZN9Callables17musttail_operatorEi(
// CIR:         %[[OPERATOR_ARG:[^ ]+]] = cir.call @_Z6nestedi({{[^)]*}}) :
// CIR:         %[[OPERATOR_RESULT:[^ ]+]] = cir.call @_ZN9CallablesclEi({{[^)]*}}) musttail :

// LLVM-LABEL: define{{.*}} i32 @_ZN9Callables17musttail_operatorEi(
// LLVM:         %[[OPERATOR_ARG:[^ ]+]] = call{{.*}} i32 @_Z6nestedi({{[^)]*}})
// LLVM:         %[[OPERATOR_RESULT:[^ ]+]] = musttail call{{.*}} i32 @_ZN9CallablesclEi({{.*}})
// LLVM-NEXT:    ret i32 %[[OPERATOR_RESULT]]

// LLVM: attributes #[[ALWAYS_INLINE]] = { alwaysinline }
