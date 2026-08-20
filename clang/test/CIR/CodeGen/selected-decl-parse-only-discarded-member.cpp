// RUN: printf 'parse-usr:c:@F@selected#\nparse-symbol:_ZN6HelperIZ8selectedvE3$_0FivEE3RunERKS0_\n_ZN6HelperIZ8selectedvE3$_0FivEE4BindEOS0_\n_ZN6HelperIZ8selectedvE3$_0FivEE7RunOnceEOS0_\n_ZZ8selectedvEN3$_0clEv\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --check-prefix=SELECTED --input-file=%t.cir

// A parse-only selector preserves the dependent member body for discovery. It
// is not an emission root: the exact selected Bind body takes the discarded
// constexpr arm and ODR-uses RunOnce, never Run(const F&).
template <typename Signature, typename R, typename F, typename... Args>
static constexpr bool kHasConstCallOperator = false;
template <typename R, typename F, typename... Args>
static constexpr bool
    kHasConstCallOperator<R (F::*)(Args...) const, R, F, Args...> = true;

template <class P, class F> int BindRepeating(P, F&&) { return 1; }
template <class P, class F> int BindOnce(P, F&&) { return 2; }

template <class Lambda, class Signature> struct Helper;
template <class Lambda, class R, class... Args>
struct Helper<Lambda, R(Args...)> {
  using F = Lambda;

  static R Run(const F& f, Args... args) {
    return f(static_cast<Args&&>(args)...);
  }
  static R RunOnce(F&& f, Args... args) {
    return f(static_cast<Args&&>(args)...);
  }
  static auto Bind(Lambda&& lambda) {
    if constexpr (kHasConstCallOperator<decltype(&F::operator()), R, F,
                                        Args...>)
      return BindRepeating(&Run, static_cast<Lambda&&>(lambda));
    else
      return BindOnce(&RunOnce, static_cast<Lambda&&>(lambda));
  }
};

int selected() {
  int value = 0;
  auto mutable_lambda = [&]() mutable { return ++value; };
  return Helper<decltype(mutable_lambda), int()>::Bind(
      static_cast<decltype(mutable_lambda)&&>(mutable_lambda));
}

// SELECTED-DAG: _ZN6HelperIZ8selectedvE3$_0FivEE4BindEOS0_ = "_ZN6HelperIZ8selectedvE3$_0FivEE4BindEOS0_"
// SELECTED-DAG: _ZN6HelperIZ8selectedvE3$_0FivEE7RunOnceEOS0_ = "_ZN6HelperIZ8selectedvE3$_0FivEE7RunOnceEOS0_"
// SELECTED-DAG: cir.func{{.*}} @_ZN6HelperIZ8selectedvE3$_0FivEE4BindEOS0_{{.*}} {
// SELECTED-DAG: cir.func{{.*}} @_ZN6HelperIZ8selectedvE3$_0FivEE7RunOnceEOS0_{{.*}} {
// SELECTED-NOT: @_ZN6HelperIZ8selectedvE3$_0FivEE3RunERKS0_
