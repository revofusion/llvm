// RUN: printf 'symbol-usr:59:_ZN4dawn31alignof_if_defined_else_defaultIT_XT0_EDTatS1_EEEc:@N@dawn@VP>2#T#Nl@alignof_if_defined_else_default>#t0.0##\n' > %t.selected
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.selected -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir --check-prefix=DEPENDENT
// RUN: printf '_ZN5cppgc8internal16IsAnyMemberTypeVINS0_11BasicMemberIT_T0_T1_T2_T3_EEEE\n' > %t.constant
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.constant -skip-function-bodies %s -o %t.constant.cir
// RUN: FileCheck %s --input-file=%t.constant.cir --check-prefix=CONSTANT

using size_t = __SIZE_TYPE__;

namespace dawn {
template <typename T, size_t Default, typename = size_t>
constexpr size_t alignof_if_defined_else_default = Default;

template <typename T, size_t Default>
constexpr size_t
    alignof_if_defined_else_default<T, Default, decltype(alignof(T))> =
        alignof(T);
} // namespace dawn

namespace cppgc::internal {
template <typename T, typename WeaknessTag, typename WriteBarrierPolicy,
          typename CheckingPolicy, typename StorageType>
struct BasicMember {};

template <typename T>
constexpr bool IsAnyMemberTypeV = false;

template <typename T, typename WeaknessTag, typename WriteBarrierPolicy,
          typename CheckingPolicy, typename StorageType>
constexpr bool IsAnyMemberTypeV<
    BasicMember<T, WeaknessTag, WriteBarrierPolicy, CheckingPolicy,
                StorageType>> = true;
} // namespace cppgc::internal

// A variable-template partial specialization is a dependent declaration
// pattern, not linker-owned storage. Exact selection preserves its typed
// definition as available_externally and marks its instantiation-dependent
// initializer explicitly instead of fabricating a concrete value.
// DEPENDENT: cir.selected_decl_root_definitions = {
// DEPENDENT-SAME: _ZN4dawn31alignof_if_defined_else_defaultIT_XT0_EDTatS1_EEE = "_ZN4dawn31alignof_if_defined_else_defaultIT_XT0_EDTatS1_EEE"
// DEPENDENT: cir.global constant available_externally @_ZN4dawn31alignof_if_defined_else_defaultIT_XT0_EDTatS1_EEE = #cir.undef
// DEPENDENT-SAME: ast_variable_template_dependent_initializer

// A selected dependent pattern with a non-dependent constant initializer owns
// an exact typed definition fact even though it does not own linker storage.
// CONSTANT: cir.selected_decl_root_definitions = {
// CONSTANT-SAME: _ZN5cppgc8internal16IsAnyMemberTypeVINS0_11BasicMemberIT_T0_T1_T2_T3_EEEE = "_ZN5cppgc8internal16IsAnyMemberTypeVINS0_11BasicMemberIT_T0_T1_T2_T3_EEEE"
// CONSTANT: cir.global constant available_externally @_ZN5cppgc8internal16IsAnyMemberTypeVINS0_11BasicMemberIT_T0_T1_T2_T3_EEEE = #true
