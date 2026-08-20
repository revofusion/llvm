
struct Payload {
  ~Payload();
};
struct DefaultedOut {
  Payload payload;
  ~DefaultedOut();
};

DefaultedOut::~DefaultedOut() = default;


int selected_nested() {
  auto outer = [] {
    return [](int value) { return value + 1; };
  };
  return outer()(41);
}

void local() {
  struct ScopedEvent {
    Payload payload;
  };
  ScopedEvent event;
}
// RUN: printf 'parse-usr:c:@F@selected_nested#\nparse-usr:c:selected-decl-local-special-members.cpp@182@F@selected_nested#@Sa@F@operator()#1\nparse-usr:c:selected-decl-local-special-members.cpp@209@F@selected_nested#@Sa@F@operator()#1@Sa@F@operator()#I#1\nlambda-usr:0:82:c:selected-decl-local-special-members.cpp@182@F@selected_nested#@Sa@F@operator()#1c:selected-decl-local-special-members.cpp@209@F@selected_nested#@Sa@F@operator()#1@Sa@F@operator()#I#1\nparse-usr:c:@F@local#\n_ZZ5localvEN11ScopedEventD2Ev\n_ZN12DefaultedOutD2Ev\nparse-usr:c:@S@LambdaCaptureKinds@F@run#I#\nlambda-usr:1:32:c:@S@LambdaCaptureKinds@F@run#I#c:@S@LambdaCaptureKinds@F@run#I#@Sa@F@operator()#1\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --check-prefix=SELECTED --implicit-check-not=@_Z15selected_nestedv --implicit-check-not=@_Z5localv --implicit-check-not=@_ZN18LambdaCaptureKinds3runEi --implicit-check-not=@_ZZN18LambdaCaptureKinds3runEiENKUlvE_clEv --input-file=%t.cir

struct LambdaCaptureKinds {
  int member;

  int run(int input) {
    auto capture_this = [this]() { return member; };
    auto capture_copy = [*this]() { return member; };
    return capture_this() + capture_copy() + input;
  }
};

// Parse-only enclosing functions authenticate discovery but are not emission
// roots. The concrete nested lambda is selected by its exact declaration USR,
// structural context USR, and context-local lambda index, and binds to the one
// producer symbol that owns its body.
// SELECTED-DAG: "lambda-usr:0:82:c:selected-decl-local-special-members.cpp@182@F@selected_nested#@Sa@F@operator()#1c:selected-decl-local-special-members.cpp@209@F@selected_nested#@Sa@F@operator()#1@Sa@F@operator()#I#1" = "{{[^"]+}}"
// SELECTED-DAG: _ZZ5localvEN11ScopedEventD2Ev = "_ZZ5localvEN11ScopedEventD2Ev"
// SELECTED-DAG: _ZN12DefaultedOutD2Ev = "_ZN12DefaultedOutD2Ev"
// SELECTED-DAG: "lambda-usr:1:32:c:@S@LambdaCaptureKinds@F@run#I#c:@S@LambdaCaptureKinds@F@run#I#@Sa@F@operator()#1" = "{{[^"]+}}"
// SELECTED-DAG: cir.func{{.*}}ast_decl_usr = "c:selected-decl-local-special-members.cpp@209@F@selected_nested#@Sa@F@operator()#1@Sa@F@operator()#I#1"{{.*}} {

// The local class's implicit, non-trivial destructor is materialized by Sema
// in its owning local DeclContext and emitted once under the requested ABI
// variant. Its enclosing function remains parse-only.
// SELECTED-DAG: cir.func{{.*}} @_ZZ5localvEN11ScopedEventD2Ev{{.*}} {

// An out-of-line defaulted definition is the exact owning redeclaration even
// before Sema attaches its synthesized body; the earlier declaration must not
// win canonical-worklist deduplication.
// SELECTED-DAG: cir.func{{.*}} @_ZN12DefaultedOutD2Ev{{.*}} {

// A member-owned lambda is recreated by parsing its exact semantic owner.
// Clang gives both operators below the same declaration USR; the structural
// context-local index selects only the second body while the first remains
// absent, and the enclosing method stays discovery-only.
// SELECTED-DAG: cir.func{{.*}}ast_decl_usr = "c:@S@LambdaCaptureKinds@F@run#I#@Sa@F@operator()#1"{{.*}}ast_lambda_index = 1 : i32{{.*}} {
