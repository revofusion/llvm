// RUN: printf '_Z17IsStrictlyBaseOfVIiiE\n_ZN3Any6TypeIdIiE2IdE\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir --implicit-check-not=@_Z9unrelatedv

template <class B, class D>
constexpr bool IsStrictlyBaseOfV = false;

struct Any {
  template <class T> struct TypeId { static const char Id; };
};
template <class T> const char Any::TypeId<T>::Id = 0;

// The exact selected variable-template specializations are formed only while
// parsing this unselected function body. Selected CIR must retain the complete
// semantic AST used by the authority pass even though it emits only exact roots.
bool instantiate_selected_variables() {
  return IsStrictlyBaseOfV<int, int> || Any::TypeId<int>::Id;
}
int unrelated() { return 1; }

// CHECK-DAG: cir.selected_decl_root_definitions = {
// CHECK-DAG: _Z17IsStrictlyBaseOfVIiiE = "_Z17IsStrictlyBaseOfVIiiE"
// CHECK-DAG: _ZN3Any6TypeIdIiE2IdE = "_ZN3Any6TypeIdIiE2IdE"
// CHECK-DAG: cir.global constant linkonce_odr comdat @_Z17IsStrictlyBaseOfVIiiE = #false
// CHECK-DAG: cir.global constant linkonce_odr comdat @_ZN3Any6TypeIdIiE2IdE = #cir.int<0>
