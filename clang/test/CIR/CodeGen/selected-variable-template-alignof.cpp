// RUN: printf 'symbol-usr:59:_ZN4dawn31alignof_if_defined_else_defaultIT_XT0_EDTatS1_EEEc:@N@dawn@VP>2#T#Nl@alignof_if_defined_else_default>#t0.0##\n' > %t.selected
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.selected -skip-function-bodies %s -o %t.cir 2>&1 | FileCheck %s --check-prefix=NONEMITTABLE

using size_t = __SIZE_TYPE__;

namespace dawn {
template <typename T, size_t Default, typename = size_t>
constexpr size_t alignof_if_defined_else_default = Default;

template <typename T, size_t Default>
constexpr size_t
    alignof_if_defined_else_default<T, Default, decltype(alignof(T))> =
        alignof(T);
} // namespace dawn

// A variable-template partial specialization is a dependent declaration
// pattern, not a storage-owning specialization. An exact external producer
// selection must reject it deterministically instead of evaluating its
// dependent initializer or silently omitting it.

// NONEMITTABLE: error: failed to emit exact selected declaration symbol '_ZN4dawn31alignof_if_defined_else_defaultIT_XT0_EDTatS1_EEE': one identity-authenticated CIR definition was required
