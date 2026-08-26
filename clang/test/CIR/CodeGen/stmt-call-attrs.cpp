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

// These dependent calls begin as unresolved overload sets whose candidates
// become signature-identical after substitution. The selected canonical
// specialization, not the surviving source overload set, owns the call edge.
template <typename T>
int freeIdentityOverload(T) {
  return 1;
}

template <typename T>
int freeIdentityOverload(T *) {
  return 2;
}

template <typename T>
int callFreeIdentity(T value) {
  return freeIdentityOverload(value);
}

int instantiateFreeIdentity(int *value) {
  return callFreeIdentity(value);
}

// IDENTITY-LABEL: cir.func{{.*}} @_Z16callFreeIdentityIPiEiT_(
// IDENTITY-NOT: ast_callee_usr = "c:@F@freeIdentityOverload<#*I>#S0_#"
// IDENTITY: cir.call @_Z20freeIdentityOverloadIiEiPT_({{[^)]*}}) {ast_callee_usr = "c:@F@freeIdentityOverload<#I>#*I#"} :
// IDENTITY-NOT: ast_callee_usr = "c:@F@freeIdentityOverload<#*I>#S0_#"

struct MemberIdentityOverloads {
  template <typename T>
  int pick(T) {
    return 3;
  }

  template <typename T>
  int pick(T *) {
    return 4;
  }

  template <typename T>
  int call(T value) {
    return pick(value);
  }
};

int instantiateMemberIdentity(MemberIdentityOverloads &overloads, int *value) {
  return overloads.call(value);
}

// IDENTITY-LABEL: cir.func{{.*}} @_ZN23MemberIdentityOverloads4callIPiEEiT_(
// IDENTITY-NOT: ast_callee_usr = "c:@S@MemberIdentityOverloads@F@pick<#*I>#S0_#"
// IDENTITY: cir.call @_ZN23MemberIdentityOverloads4pickIiEEiPT_({{[^)]*}}) {ast_callee_usr = "c:@S@MemberIdentityOverloads@F@pick<#I>#*I#"} :
// IDENTITY-NOT: ast_callee_usr = "c:@S@MemberIdentityOverloads@F@pick<#*I>#S0_#"

int sharedAsmIdentity(int) asm("shared_asm_identity");
long sharedAsmIdentity(long) asm("shared_asm_identity");

template <typename T>
int callSharedAsmIdentity(T value) {
  return sharedAsmIdentity(value);
}

int instantiateSharedAsmIdentity(int value) {
  return callSharedAsmIdentity(value);
}

// Both overload declarations have the same linker symbol, so the call's
// semantic identity must come from Clang's selected canonical declaration.
// IDENTITY-LABEL: cir.func{{.*}} @_Z21callSharedAsmIdentityIiEiT_(
// IDENTITY-NOT: ast_callee_usr = "c:@F@sharedAsmIdentity#L#"
// IDENTITY: cir.call @shared_asm_identity({{[^)]*}}) {ast_callee_usr = "c:@F@sharedAsmIdentity#I#"} :
// IDENTITY-NOT: ast_callee_usr = "c:@F@sharedAsmIdentity#L#"

// V8's CompareC<T> blocker has several visible operator templates in its
// dependent source lookup, but concrete integral specializations select the
// built-in operator. The concrete CIR body is the resolved control.
template <typename T>
bool operator==(T *, const T *);

template <typename T>
bool operator==(const T *, T *);

template <typename T>
bool compareIdentity(T lhs, T rhs) {
  return lhs == rhs;
}

bool instantiateCompareIdentity(int lhs, int rhs) {
  return compareIdentity(lhs, rhs);
}

// IDENTITY-LABEL: cir.func{{.*}} @_Z15compareIdentityIiEbT_S0_(
// IDENTITY-NOT: cir.call
// IDENTITY: cir.cmp eq
// IDENTITY-NOT: cir.call
// IDENTITY: } loc(

// Blink's reflector blocker passes an overloaded member address as a
// dependent non-type template argument. Substitution selects the (int, int)
// method, so even the call through the concrete MemFunc specialization must
// carry that method's exact canonical declaration identity.
struct ReflectIdentityElement {
  void setAttribute(int, int);
  void setAttribute(int, long);
  void setAttribute(long, int);
};

template <typename ArgType,
          void (ReflectIdentityElement::*MemFunc)(int, ArgType)>
int performReflectIdentity(ReflectIdentityElement *receiver, ArgType value) {
  (receiver->*MemFunc)(0, value);
  return value;
}

template <typename T>
int callReflectIdentity(ReflectIdentityElement *receiver, T value) {
  return performReflectIdentity<T, &ReflectIdentityElement::setAttribute>(
      receiver, value);
}

int instantiateReflectIdentity(ReflectIdentityElement *receiver, int value) {
  return callReflectIdentity(receiver, value);
}

// IDENTITY-LABEL: cir.func{{.*}} @_Z22performReflectIdentityIiTnM22ReflectIdentityElementFviT_EXadL_ZNS0_12setAttributeEiiEEEiPS0_S1_(
// IDENTITY-NOT: ast_callee_usr = "c:@S@ReflectIdentityElement@F@setAttribute#I#L#"
// IDENTITY-NOT: ast_callee_usr = "c:@S@ReflectIdentityElement@F@setAttribute#L#I#"
// IDENTITY: cir.call %{{[^ (]+}}({{[^)]*}}) {ast_callee_usr = "c:@S@ReflectIdentityElement@F@setAttribute#I#I#"} :
// IDENTITY-NOT: ast_callee_usr = "c:@S@ReflectIdentityElement@F@setAttribute#I#L#"
// IDENTITY-NOT: ast_callee_usr = "c:@S@ReflectIdentityElement@F@setAttribute#L#I#"
// IDENTITY: } loc(
// IDENTITY-LABEL: cir.func{{.*}} @_Z19callReflectIdentityIiEiP22ReflectIdentityElementT_(
// IDENTITY-NOT: ast_callee_usr = "c:@S@ReflectIdentityElement@F@setAttribute#I#L#"
// IDENTITY-NOT: ast_callee_usr = "c:@S@ReflectIdentityElement@F@setAttribute#L#I#"
// IDENTITY: cir.call @_Z22performReflectIdentityIiTnM22ReflectIdentityElementFviT_EXadL_ZNS0_12setAttributeEiiEEEiPS0_S1_({{[^)]*}}) {ast_callee_usr = "c:@F@performReflectIdentity<#I#@S@ReflectIdentityElement@F@setAttribute#I#I#>#*$@S@ReflectIdentityElement#I#"} :
