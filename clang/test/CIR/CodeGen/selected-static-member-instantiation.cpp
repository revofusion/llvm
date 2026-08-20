// Selected-decl emission of class-template static data members shaped like
// libc++ basic_string<CharT>::npos: an in-class constant initializer plus an
// out-of-line template pattern definition. SharedString additionally carries
// an explicit instantiation declaration (extern template), so the library's
// explicit-instantiation-definition TU owns the strong symbol
// (C++ [temp.explicit]p13) and Sema records constant initialization only on
// the initializing in-class declaration, never on the instantiated
// out-of-line definition that CIR emits. Selected emission owns the exact
// evaluated initializer: the module must carry an available_externally
// definition for the extern-template member and a linkonce_odr definition for
// the ordinary implicit instantiation, never a declaration-only global.
// RUN: printf 'selectedRoot\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir
// A campaign manifest can also select the extern-template member itself as an
// exact root symbol; it must bind to the same available_externally definition.
// RUN: printf '_ZN12SharedStringIcE4nposE\nselectedRoot\n' > %t.var-roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.var-roots -skip-function-bodies %s -o %t.var.cir
// RUN: FileCheck %s --check-prefix=VAR-ROOT --input-file=%t.var.cir

template <typename T>
const T &smallMin(const T &lhs, const T &rhs) {
  return rhs < lhs ? rhs : lhs;
}

template <typename CharT>
struct SharedString {
  typedef unsigned long size_type;
  static const size_type npos = static_cast<size_type>(-1);
  size_type find(CharT needle) const;
  CharT data[4];
};

template <typename CharT>
const typename SharedString<CharT>::size_type SharedString<CharT>::npos;

template <typename CharT>
typename SharedString<CharT>::size_type
SharedString<CharT>::find(CharT needle) const {
  for (size_type index = 0; index != 4; ++index)
    if (data[index] == needle)
      return smallMin(index, npos);
  return npos;
}

extern template const SharedString<char>::size_type SharedString<char>::npos;

template <typename CharT>
struct OwnedString {
  typedef unsigned long size_type;
  static const size_type npos = static_cast<size_type>(-1);
  size_type find(CharT needle) const;
  CharT data[4];
};

template <typename CharT>
const typename OwnedString<CharT>::size_type OwnedString<CharT>::npos;

template <typename CharT>
typename OwnedString<CharT>::size_type
OwnedString<CharT>::find(CharT needle) const {
  for (size_type index = 0; index != 4; ++index)
    if (data[index] == needle)
      return smallMin(index, npos);
  return npos;
}

extern "C" unsigned long selectedRoot(SharedString<char> &shared,
                                      OwnedString<char> &owned, char needle) {
  return shared.find(needle) + owned.find(needle);
}

// The extern-template member keeps the exact evaluated constant while the
// linkage still marks the library TU as the strong-symbol owner.
// CHECK-DAG: cir.global {{.*}}available_externally @_ZN12SharedStringIcE4nposE = #cir.int<18446744073709551615> : !u64i
// The ordinary implicit instantiation stays an ODR-emittable definition.
// CHECK-DAG: cir.global {{.*}}linkonce_odr {{.*}}@_ZN11OwnedStringIcE4nposE = #cir.int<18446744073709551615> : !u64i
// CHECK-DAG: cir.func {{.*}}@selectedRoot

// VAR-ROOT: cir.selected_decl_root_definitions = {_ZN12SharedStringIcE4nposE = "_ZN12SharedStringIcE4nposE", selectedRoot = "selectedRoot"}
// VAR-ROOT-DAG: cir.global {{.*}}available_externally @_ZN12SharedStringIcE4nposE = #cir.int<18446744073709551615> : !u64i
// VAR-ROOT-DAG: cir.func {{.*}}@selectedRoot
