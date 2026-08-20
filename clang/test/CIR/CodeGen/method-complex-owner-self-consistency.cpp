// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir %s -o - | FileCheck %s

struct Profile {};
namespace sessions {
struct Window {};
}
template <class T>
struct Alloc {};
template <class T, class A>
struct Vector {};
template <class Signature>
struct Callback {};
template <class C>
struct Mock {
  C *Get() { return nullptr; }
};
using ComplexCallback =
    Callback<void(Profile *,
                  const Vector<const sessions::Window *,
                               Alloc<const sessions::Window *>> &)>;
extern "C" int emit_complex_owner(Mock<ComplexCallback> &mock) {
  return mock.Get() != nullptr;
}

// Identity observation must not depend on whether nested Callback/Vector/Alloc
// specializations became complete before or after method emission. The module
// map and callable attribute name the exact same recursively owned class.
// CHECK: cir.record_decl_identities = {{.*}} = "[[OWNER:c:@S@Mock>[^"]+]]"
// CHECK: cir.func {{[^@]*}} @{{_ZN4Mock[^ ]+3GetEv}}
// CHECK-SAME: ast_method_callable_identity = {
// CHECK-SAME: method_declaring_class_usr = "[[OWNER]]"
