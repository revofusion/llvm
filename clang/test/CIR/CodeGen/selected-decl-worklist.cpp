// RUN: printf 'selected\n' > %t.symbol-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.symbol-roots -skip-function-bodies %s -o %t.symbol.cir
// RUN: FileCheck %s --check-prefixes=CHECK,SYMBOL --implicit-check-not=@_Z9unrelatedv --input-file=%t.symbol.cir
// RUN: printf 'usr:c:@F@selected\n' > %t.usr-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.usr-roots -skip-function-bodies %s -o %t.usr.cir
// RUN: FileCheck %s --check-prefixes=CHECK,USR --implicit-check-not=@_Z9unrelatedv --input-file=%t.usr.cir
// RUN: printf 'symbol-usr:5:stalec:@F@selected\nstale\n' > %t.symbol-usr-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.symbol-usr-roots -skip-function-bodies %s -o %t.symbol-usr.cir
// RUN: FileCheck %s --check-prefix=SYMBOL-USR --implicit-check-not=@_Z9unrelatedv --input-file=%t.symbol-usr.cir
// A symbol-USR pair and a separately selected ABI variant can share one
// constructor USR. Each exact symbol must retain only its own definition.
// RUN: printf 'symbol-usr:14:_ZN4LeafC1EOS_c:@S@Leaf@F@Leaf#&&$@S@Leaf#\n_ZN4LeafC2EOS_\n' > %t.ctor-collision-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.ctor-collision-roots -skip-function-bodies %s -o %t.ctor-collision.cir
// RUN: FileCheck %s --check-prefix=CTOR-COLLISION --input-file=%t.ctor-collision.cir
// Anonymous lambda discriminators must be lexical-order deterministic, not
// first-mangling-order. The selected ctor/dtor name the SECOND lambda in
// lambdaCarrier as $_1 even though selected emission mangles it first: the
// first lambda is only called directly and is never a selected root.
// RUN: printf '_ZN7DiscBoxIlEC2IRZ13lambdaCarriervE3$_1EEOT_\n_ZN8DiscSinkIZ13lambdaCarriervE3$_1ED2Ev\nparse-symbol:_Z13lambdaCarrierv\nparse-usr:c:@F@lambdaCarrier#\n' > %t.disc-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.disc-roots -skip-function-bodies %s -o %t.disc.cir
// RUN: FileCheck %s --check-prefix=DISC --input-file=%t.disc.cir
// RUN: printf '_Z7missingv\nusr:c:@F@missing\n' > %t.missing-roots
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.missing-roots -skip-function-bodies %s -o %t.missing.cir 2>&1 | FileCheck %s --check-prefix=MISSING
// RUN: printf '_Z10discoveredIiEiv\nparse-usr:c:@F@parse_only#\nparse-usr:c:@FT@>1#Tdiscovered#I#\n' > %t.parse-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.parse-roots -skip-function-bodies %s -o %t.parse.cir
// RUN: FileCheck %s --check-prefix=PARSE --implicit-check-not=@_Z10parse_onlyv --implicit-check-not=@_Z9unrelatedv --input-file=%t.parse.cir
// A parse-only dependent defaulted method is metadata for future
// specializations, not a Sema definition request. Pair it with an exact
// definition root because parse-only metadata cannot anchor selected-decl mode.
// RUN: printf 'selected\nparse-usr:c:@ST>1#T@DependentMove@F@operator=#&&>@ST>1#T@DependentMove1t0.0#\n' > %t.dependent.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.dependent.roots -skip-function-bodies %s -o /dev/null
// The Chromium gaps were a constrained free-function specialization and a
// member function-template specialization of a concrete class template.
// Select both by exact USR while retaining their patterns only for parsing.
// RUN: printf 'parse-usr:c:@F@observedCheck<#I#I#$@S@ObservedFunctor>#I#I#S0_#\nparse-usr:c:@FT@>3#T#T#TobservedCheck#t0.0#t0.1#t0.2#I#\nusr:c:@F@observedCheck<#I#I#$@S@ObservedFunctor>#I#I#S0_#\nparse-usr:c:@S@ObservedFunctorTraits>#$@S@ObservedFunctor@F@Invoke<#S0_#p1I>#&&S0_#&&I#S\nparse-usr:c:@ST>1#T@ObservedFunctorTraits@FT@>2#T#pTInvoke#&&t1.0#P&&t1.1#I#S\nusr:c:@S@ObservedFunctorTraits>#$@S@ObservedFunctor@F@Invoke<#S0_#p1I>#&&S0_#&&I#S\n' > %t.observed-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.observed-roots -skip-function-bodies %s -o %t.observed.cir
// RUN: FileCheck %s --check-prefix=OBSERVED --implicit-check-not='ast_decl_usr = "c:@FT@>3#T#T#TobservedCheck#t0.0#t0.1#t0.2#I#"' --implicit-check-not='ast_decl_usr = "c:@ST>1#T@ObservedFunctorTraits@FT@>2#T#pTInvoke#&&t1.0#P&&t1.1#I#S"' --implicit-check-not='ast_decl_usr = "c:@S@ObservedFunctorTraits>#$@S@ObservedFunctor@F@Invoke<#S0_#p1L>#&&S0_#&&L#S"' --input-file=%t.observed.cir
// A symbol-selected and a USR-selected function-template specialization each
// materialize only their exact same-spelling FunctionDecl and body closure.
// RUN: printf '_Z6rootedIiEiT_\nparse-usr:c:@F@observeInt#\n' > %t.template-symbol-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.template-symbol-roots -skip-function-bodies %s -o %t.template-symbol.cir
// RUN: FileCheck %s --check-prefix=TEMPLATE-SYMBOL --implicit-check-not=@_Z6rootedIlEiT_ --implicit-check-not=@_Z4leafIlEiT_ --input-file=%t.template-symbol.cir
// RUN: printf 'usr:c:@F@rooted<#L>#L#\nparse-usr:c:@F@observeLong#\n' > %t.template-usr-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.template-usr-roots -skip-function-bodies %s -o %t.template-usr.cir
// RUN: FileCheck %s --check-prefix=TEMPLATE-USR --implicit-check-not=@_Z6rootedIiEiT_ --implicit-check-not=@_Z4leafIiEiT_ --input-file=%t.template-usr.cir
// A libc++-shaped inline ABI namespace can hide a template dependency behind
// an ordinary exact root. The dependency is not selected by spelling: it must
// enter the exact declaration worklist while the root body is emitted.
// RUN: printf 'libcxxSelected\nparse-usr:c:@N@std@N@__Cr@FT@>1#TlibraryLeaf#t0.0#\n' > %t.libcxx-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.libcxx-roots -skip-function-bodies %s -o %t.libcxx.cir
// RUN: FileCheck %s --check-prefix=LIBCXX --implicit-check-not=@_ZNSt4__Cr11libraryLeafIlEEiT_ --implicit-check-not=@_ZNSt4__Cr15libcxxUnrelated --input-file=%t.libcxx.cir
// A libc++ for_each specialization whose function-object argument is a local
// capturing lambda must retain the canonical instantiated FunctionDecl
// identity when selected-closure emission rewrites its worklist entry to the
// defining redeclaration.
// RUN: printf 'nestedLibcxxSelected\n' > %t.nested-libcxx-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.nested-libcxx-roots %s -o %t.nested-libcxx.cir
// RUN: FileCheck %s --check-prefix=NESTED-LIBCXX --input-file=%t.nested-libcxx.cir
// A selected specialization can be nested below an unselected class-template
// member and can itself take that member's local lambda as a template argument.
// The exact function and lambda identities must materialize their ABI symbols
// without first emitting the enclosing member body.
// RUN: printf 'usr:c:@F@selectedNestedCall<#I#I#$@S@NestedBox>#I@F@at#I#I#1@Sa>#I#I#S0_#\nlambda-usr:0:30:c:@ST>1#T@NestedBox@F@at#I#I#1c:@ST>1#T@NestedBox@F@at#I#I#1@Sa@F@operator()#I#I#1\nparse-symbol:_ZNK9NestedBoxIiE2atEii\nparse-usr:c:@ST>1#T@NestedBox@F@at#I#I#1\n' > %t.nested-lambda-symbol-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.nested-lambda-symbol-roots -skip-function-bodies %s -o %t.nested-lambda-symbol.cir
// RUN: FileCheck %s --check-prefix=NESTED-LAMBDA-SYMBOL --implicit-check-not=@_ZNK9NestedBoxIiE2atEii --input-file=%t.nested-lambda-symbol.cir


struct Payload {
  Payload(Payload &&);
};

struct Leaf {
  Payload payload;
  Leaf(Leaf &&) = default;
};

template <typename T>
struct Holder {
  T value;
  Holder(Holder &&) = default;
};

template <typename T>
struct DependentMove {
  T value;
  DependentMove &operator=(DependentMove &&) = default;
};

extern "C" Holder<Leaf> selected(Holder<Leaf> &&value) {
  return static_cast<Holder<Leaf> &&>(value);
}

void unrelated() {}

template <typename T>
int discovered() {
  return 7;
}

int parse_only() {
  return discovered<int>();
}

template <typename T>
int leaf(T value) {
  return value + 1;
}

template <typename T>
int rooted(T value) {
  return leaf(value);
}

int observeInt() { return rooted(1); }
int observeLong() { return rooted(1L); }

struct ObservedFunctor {
  int operator()(int value) const { return value + 1; }
};

template <typename T, typename U, typename F>
  requires requires(F failure, T lhs, U rhs) { failure(lhs + rhs); }
int observedCheck(T lhs, U rhs, F failure) {
  return lhs < rhs ? 0 : failure(lhs + rhs);
}

template <typename Functor>
struct ObservedFunctorTraits {
  template <typename RunFunctor, typename... RunArgs>
  static int Invoke(RunFunctor &&functor, RunArgs &&...args) {
    return static_cast<RunFunctor &&>(functor)(
        static_cast<RunArgs &&>(args)...);
  }
};

int forceObservedSpecializations =
    observedCheck(1, 2, ObservedFunctor{}) +
    ObservedFunctorTraits<ObservedFunctor>::Invoke(ObservedFunctor{}, 3) +
    ObservedFunctorTraits<ObservedFunctor>::Invoke(ObservedFunctor{}, 4L);

namespace std {
inline namespace __Cr {
template <typename T>
int libraryLeaf(T value) {
  return value + 1;
}

template <typename Iterator, typename Function>
Function for_each(Iterator first, Iterator last, Function function) {
  for (; first != last; ++first)
    function(*first);
  return function;
}

template <typename T>
struct LibraryBox {
  T value;
};

extern "C" int libcxxSelected(LibraryBox<int> box) {
  return libraryLeaf(box.value);
}

int libcxxUnrelated(LibraryBox<long> box) {
  return libraryLeaf(box.value);
}
} // namespace __Cr
} // namespace std

extern "C" int nestedLibcxxSelected(int *first, int *last, int upper_bound) {
  std::__Cr::for_each(first, last, [upper_bound](int &value) {
    if (value > upper_bound)
      value = upper_bound;
  });
  return first == last ? upper_bound : *first;
}
template <typename A, typename B, typename F>
int selectedNestedCall(A lhs, B rhs, F failure) {
  return lhs < rhs ? 0 : failure(lhs, rhs);
}

template <typename T>
struct NestedBox {
  int at(int lhs, int rhs) const {
    return selectedNestedCall(
        lhs, rhs, [](int first, int second) -> int { return first + second; });
  }
};

int deferredSelectedNestedBoxSize = sizeof(NestedBox<int>);

template <typename T>
struct DiscBox {
  int state;
  template <typename F>
  DiscBox(F &&f) : state(f(1)) {}
};

template <typename T>
struct DiscSink {
  T stored;
  DiscSink(T &&value) : stored(static_cast<T &&>(value)) {}
  ~DiscSink() {}
};

int lambdaCarrier() {
  auto first = [](int v) { return v + 1; };
  auto second = [](int v) { return v + 2; };
  int direct = first(1);
  DiscBox<long> boxed(second);
  DiscSink<decltype(second)> sink(static_cast<decltype(second) &&>(second));
  return direct + boxed.state + sink.stored(3);
}



// Defining the selected function discovers the defaulted Holder constructor;
// defining that constructor discovers Leaf's defaulted constructor. The exact
// dependency frontier must be materialized without rewalking live template
// specialization collections.
// SYMBOL: cir.selected_decl_root_definitions = {selected = "selected"}
// USR: cir.selected_decl_root_definitions = {"usr:c:@F@selected" = "selected"}
// SYMBOL-USR: cir.selected_decl_root_definitions = {stale = "selected"}
// CTOR-COLLISION: cir.selected_decl_root_definitions = {_ZN4LeafC1EOS_ = "_ZN4LeafC1EOS_", _ZN4LeafC2EOS_ = "_ZN4LeafC2EOS_"}
// DISC-DAG: cir.func{{.*}} @_ZN7DiscBoxIlEC2IRZ13lambdaCarriervE3$_1EEOT_{{.*}} {
// DISC-DAG: cir.func{{.*}} @_ZN8DiscSinkIZ13lambdaCarriervE3$_1ED2Ev{{.*}} {
// SYMBOL-USR-DAG: cir.func{{.*}} @selected{{.*}} {
// CHECK-DAG: cir.func{{.*}} @selected{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN6HolderI4LeafEC2EOS1_{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN6HolderI4LeafEC1EOS1_{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN4LeafC2EOS_{{.*}} {
// CHECK-DAG: cir.func{{.*}} @_ZN4LeafC1EOS_{{.*}} {

// Each requested selector records exactly the cir.func that owns its body and
// the definition carries the matching producer specialization identity.
// OBSERVED-DAG: "usr:c:@F@observedCheck<#I#I#$@S@ObservedFunctor>#I#I#S0_#" = "{{[^"]+}}"
// OBSERVED-DAG: "usr:c:@S@ObservedFunctorTraits>#$@S@ObservedFunctor@F@Invoke<#S0_#p1I>#&&S0_#&&I#S" = "{{[^"]+}}"
// OBSERVED-DAG: cir.func{{.*}}ast_decl_specialization_identity = {{.*}}usr = "c:@F@observedCheck<#I#I#$@S@ObservedFunctor>#I#I#S0_#"{{.*}} {
// OBSERVED-DAG: cir.func{{.*}}ast_decl_specialization_identity = {{.*}}usr = "c:@S@ObservedFunctorTraits>#$@S@ObservedFunctor@F@Invoke<#S0_#p1I>#&&S0_#&&I#S"{{.*}} {

// The selected specialization's selector binds its exact body, and its body
// materializes the matching leaf specialization as a transitive dependency.
// TEMPLATE-SYMBOL: cir.selected_decl_root_definitions = {_Z6rootedIiEiT_ = "_Z6rootedIiEiT_"}
// TEMPLATE-SYMBOL-DAG: cir.func{{.*}} @_Z6rootedIiEiT_{{.*}} {
// TEMPLATE-SYMBOL-DAG: cir.func{{.*}} @_Z4leafIiEiT_{{.*}} {
// TEMPLATE-USR: cir.selected_decl_root_definitions = {"usr:c:@F@rooted<#L>#L#" = "_Z6rootedIlEiT_"}
// TEMPLATE-USR-DAG: cir.func{{.*}} @_Z6rootedIlEiT_{{.*}} {
// TEMPLATE-USR-DAG: cir.func{{.*}} @_Z4leafIlEiT_{{.*}} {

// The exact C-linkage selector owns one authenticated definition even inside
// an inline ABI namespace. Emitting that body discovers the concrete int
// specialization; the same-spelling long specialization stays outside the
// selected closure.
// LIBCXX: cir.selected_decl_root_definitions = {libcxxSelected = "libcxxSelected"}
// LIBCXX-DAG: cir.func{{.*}} @libcxxSelected{{.*}} {
// LIBCXX-DAG: cir.func{{.*}} @_ZNSt4__Cr11libraryLeafIiEEiT_{{.*}} {

// The concrete for_each endpoint and its canonical specialization declaration
// carry the same producer USR. The specialization tuple also retains the exact
// template pattern and endpoint symbol; no symbol or qualified-name recovery
// is needed to choose a FunctionDecl.
// NESTED-LIBCXX: cir.func{{.*}} @[[FOREACH:[^ (]*for_each[^ (]*]](
// NESTED-LIBCXX-SAME: ast_decl_linkage_name = "[[FOREACH]]"
// NESTED-LIBCXX-SAME: ast_decl_specialization_identity = {mangled_name = "[[FOREACH]]", poi = "{{[^"]*}}", template_pattern_usr = "[[FOREACH_PATTERN:c:[^"]*for_each[^"]*]]", usr = "[[FOREACH_USR:c:[^"]*for_each[^"]*]]"}
// NESTED-LIBCXX-SAME: ast_decl_usr = "[[FOREACH_USR]]"
// NESTED-LAMBDA-SYMBOL-DAG: cir.func{{.*}} @_ZZNK9NestedBoxIiE2atEiiENKUliiE_clEii({{.*}}ast_decl_linkage_name = "_ZZNK9NestedBoxIiE2atEiiENKUliiE_clEii"{{.*}}ast_decl_specialization_identity = {mangled_name = "_ZZNK9NestedBoxIiE2atEiiENKUliiE_clEii", template_pattern_usr = "c:@ST>1#T@NestedBox@F@at#I#I#1@Sa@F@operator()#I#I#1", usr = "c:@S@NestedBox>#I@F@at#I#I#1@Sa@F@operator()#I#I#1"}{{.*}}ast_decl_usr = "c:@S@NestedBox>#I@F@at#I#I#1@Sa@F@operator()#I#I#1"{{.*}} {
// NESTED-LAMBDA-SYMBOL-DAG: cir.func{{.*}} @_Z18selectedNestedCallIiiZNK9NestedBoxIiE2atEiiEUliiE_EiT_T0_T1_({{.*}}ast_decl_linkage_name = "_Z18selectedNestedCallIiiZNK9NestedBoxIiE2atEiiEUliiE_EiT_T0_T1_"{{.*}}ast_decl_specialization_identity = {mangled_name = "_Z18selectedNestedCallIiiZNK9NestedBoxIiE2atEiiEUliiE_EiT_T0_T1_", poi = "{{[^"]+}}", template_pattern_usr = "c:@FT@>3#T#T#TselectedNestedCall#t0.0#t0.1#t0.2#I#", usr = "c:@F@selectedNestedCall<#I#I#$@S@NestedBox>#I@F@at#I#I#1@Sa>#I#I#S0_#"}{{.*}}ast_decl_usr = "c:@F@selectedNestedCall<#I#I#$@S@NestedBox>#I@F@at#I#I#1@Sa>#I#I#S0_#"{{.*}} {


// A parse-only declaration can discover an exact selected specialization
// without becoming an emission root itself. The template pattern is also
// parse-only, so its specialization has a body when CIR materializes it.
// PARSE: cir.func{{.*}} @_Z10discoveredIiEiv{{.*}} {

// MISSING-DAG: error: failed to emit exact selected declaration symbol '_Z7missingv': one identity-authenticated CIR definition was required
// MISSING-DAG: error: failed to emit exact selected declaration USR 'c:@F@missing': one identity-authenticated CIR definition was required
