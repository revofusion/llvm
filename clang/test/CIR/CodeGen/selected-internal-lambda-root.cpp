// Anonymous-namespace lambdas at namespace scope (the Chromium BASE_FEATURE
// shape) carry no lambda mangling number, so their $_N discriminator comes
// from the mangler's anonymous-struct id counter. That counter previously
// minted null-owner ids from AnonStructIds.size() in mangling-request order:
// mangling the function-local lambda inside consumeLocal first inserted one
// map entry and shifted the namespace-scope lambdas from $_0/$_1 to $_1/$_2.
// A selected-decl manifest records the exact exporter symbol in one
// compilation and demands it from this one, whose request order differs; the
// discriminator must therefore be a function of the AST (lexical order), not
// of mangling order.
//
// Select the lexically first namespace-scope lambda under its AST-derived
// symbol $_0 while a function-owned lambda is mangled ahead of it.
// RUN: printf 'parse-symbol:_ZN6syncer12_GLOBAL__N_112consumeLocalEi\nparse-symbol:_ZZN6syncer12_GLOBAL__N_112consumeLocalEiENK3$_0clEv\nparse-symbol:_ZNK6syncer12_GLOBAL__N_13$_0clEv\nparse-symbol:_ZNK6syncer12_GLOBAL__N_13$_1clEv\nparse-symbol:_ZN6syncer8touchAllEi\n_ZN6syncer12_GLOBAL__N_112consumeLocalEi\n_ZNK6syncer12_GLOBAL__N_13$_0clEv\n_ZN6syncer8touchAllEi\n' > %t.name-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.name-roots -skip-function-bodies %s -o %t.name.cir
// RUN: FileCheck %s --check-prefix=NAME --input-file=%t.name.cir
//
// Selecting the lexically second namespace-scope lambda under $_1 must bind
// the state lambda (returns int), never the shifted name lambda (returns
// const char *): the exact symbol is declaration identity, not a counter.
// RUN: printf 'parse-symbol:_ZN6syncer12_GLOBAL__N_112consumeLocalEi\nparse-symbol:_ZZN6syncer12_GLOBAL__N_112consumeLocalEiENK3$_0clEv\nparse-symbol:_ZNK6syncer12_GLOBAL__N_13$_0clEv\nparse-symbol:_ZNK6syncer12_GLOBAL__N_13$_1clEv\nparse-symbol:_ZN6syncer8touchAllEi\n_ZN6syncer12_GLOBAL__N_112consumeLocalEi\n_ZNK6syncer12_GLOBAL__N_13$_1clEv\n_ZN6syncer8touchAllEi\n' > %t.state-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.state-roots -skip-function-bodies %s -o %t.state.cir
// RUN: FileCheck %s --check-prefix=STATE --input-file=%t.state.cir
//
// A constrained call inside a still-unparsed body eagerly instantiates
// traitHolds<lambda> and demands its mangled name before bindingCarrier's
// body is attached; the bind lambda previously stole per-function id 0 from
// the two lexically earlier lambdas (Chromium base::BindRepeating +
// BindImplWouldSucceed shape). The lexically first lambda must own $_0.
// RUN: printf 'parse-symbol:_ZN6syncer14bindingCarrierEb\nparse-symbol:_ZZN6syncer14bindingCarrierEbENK3$_0clEv\nparse-symbol:_ZZN6syncer14bindingCarrierEbENK3$_1clEv\n_ZN6syncer14bindingCarrierEb\n_ZZN6syncer14bindingCarrierEbENK3$_0clEv\n' > %t.bind-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.bind-roots -skip-function-bodies %s -o %t.bind.cir
// RUN: FileCheck %s --check-prefix=BIND --input-file=%t.bind.cir

namespace syncer {
namespace {

int consumeLocal(int seed) {
  auto local = [seed] { return seed + 1; };
  return local();
}

constinit const char *kFeatureName = [] { return "Feat"; }();
constinit const int kFeatureState = [] { return 1; }();

}  // namespace

int touchAll(int seed) {
  return consumeLocal(seed) + kFeatureState + (kFeatureName[0] == 'F');
}
}  // namespace syncer

template <typename F> constexpr bool traitHolds = sizeof(F) > 0;
template <typename F>
  requires(traitHolds<F>)
int makeBinding(F f) {
  return sizeof(f) > 0;
}

namespace syncer {
int bindingCarrier(bool flag) {
  if (!flag)
    [&]() { return 1; }();
  if (!flag)
    [&]() { return 2; }();
  int bound = makeBinding([](const char *p) { return p != nullptr; });
  return bound;
}
}  // namespace syncer

// The function-owned lambda keeps its per-function id.
// NAME-DAG: cir.func {{.*}}@_ZZN6syncer12_GLOBAL__N_112consumeLocalEiENK3$_0clEv
// The namespace-scope name lambda owns the lexical id 0 and returns the
// feature-name string.
// NAME-DAG: cir.func {{.*}}@_ZNK6syncer12_GLOBAL__N_13$_0clEv({{.*}}) -> (!cir.ptr<!s8i> {{.*}})
// NAME-DAG: _ZNK6syncer12_GLOBAL__N_13$_0clEv = "_ZNK6syncer12_GLOBAL__N_13$_0clEv"

// The state lambda owns the lexical id 1 and returns int.
// STATE-DAG: cir.func {{.*}}@_ZNK6syncer12_GLOBAL__N_13$_1clEv({{.*}}) -> (!s32i {{.*}})
// STATE-DAG: _ZNK6syncer12_GLOBAL__N_13$_1clEv = "_ZNK6syncer12_GLOBAL__N_13$_1clEv"

// The lexically first lambda in bindingCarrier owns id 0 even though the
// makeBinding argument lambda's id was demanded first, mid-parse.
// BIND-DAG: cir.func {{.*}}@_ZZN6syncer14bindingCarrierEbENK3$_0clEv
// BIND-DAG: _ZZN6syncer14bindingCarrierEbENK3$_0clEv = "_ZZN6syncer14bindingCarrierEbENK3$_0clEv"
