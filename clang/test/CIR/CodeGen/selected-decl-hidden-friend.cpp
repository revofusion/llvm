// RUN: printf '_ZN5fxcrt8selectedERKNS_18StringViewTemplateIcEEPKc\n_ZN5fxcrteqERKNS_18StringViewTemplateIcEEPKc\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --implicit-check-not=@_ZN5fxcrt9unrelatedEv --input-file=%t.cir

namespace fxcrt {

template <typename CharType>
struct StringViewTemplate {
  CharType value;

  friend bool operator==(const StringViewTemplate &lhs, const CharType *ptr) {
    return ptr && lhs.value == *ptr;
  }
};

bool selected(const StringViewTemplate<char> &lhs, const char *ptr) {
  return lhs == ptr;
}

bool unrelated() { return false; }

} // namespace fxcrt

// The concrete hidden friend is a child of FriendDecl inside the instantiated
// class-template specialization, not a method or function-template
// specialization. Exact selected-symbol matching must still emit its body.
// CHECK-DAG: cir.func{{.*}}@_ZN5fxcrt8selectedERKNS_18StringViewTemplateIcEEPKc{{.*}} {
// CHECK-DAG: cir.func{{.*}}@_ZN5fxcrteqERKNS_18StringViewTemplateIcEEPKc{{.*}} {
