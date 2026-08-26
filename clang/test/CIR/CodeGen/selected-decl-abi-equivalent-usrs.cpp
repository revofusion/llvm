// RUN: printf '_Z8from_extRKDv4_f\n_Z8from_gnuRKDv4_f\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir

template <class To, class From>
To exact_bit_cast(const From &from) {
  return __builtin_bit_cast(To, from);
}

using GNUVector = float __attribute__((vector_size(16)));
using ExtVector = float __attribute__((ext_vector_type(4)));

GNUVector from_ext(const ExtVector &value) {
  return exact_bit_cast<GNUVector>(value);
}
ExtVector from_gnu(const GNUVector &value) {
  return exact_bit_cast<ExtVector>(value);
}

// GNU vector_size and ext_vector_type are distinct exact Clang types and have
// distinct specialization USRs, but the Itanium ABI deliberately gives both
// specializations the same linkage name. Selected CIR keeps one ABI body and
// authenticates both source declarations on it.
// CHECK-COUNT-1: cir.func{{.*}} @_Z14exact_bit_castIDv4_fDv4_fET_RKT0_(
// CHECK-SAME: ast_decl_usr = "[[PRIMARY:c:[^"]+]]"
// CHECK-SAME: ast_decl_usr_alternatives = ["[[PRIMARY]]", "[[ALTERNATE:c:[^"]+]]"]
