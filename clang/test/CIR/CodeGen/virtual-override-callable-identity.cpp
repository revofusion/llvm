// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir %s -o - | FileCheck %s

struct Root {
  virtual void echo() {}
};
struct Derived : Root {
  void echo() override {}
  void ordinary() {}
};
struct OtherRoot {
  virtual void shared() {}
};
struct AlternateRoot {
  virtual void shared() {}
};
struct MultiDerived : OtherRoot, AlternateRoot {
  void shared() override {}
};

extern "C" void emit_callable_identities(Derived* value) {
  value->Derived::echo();
  value->ordinary();
}

extern "C" void emit_multi_root_identity(MultiDerived* value) {
  value->MultiDerived::shared();
}

// The emitted override can be anchored to its canonical redeclaration, but
// its exact virtual root is owned by Root::echo and Root.
// CHECK: cir.func {{[^@]*}} @_ZN7Derived4echoEv
// CHECK-SAME: ast_method_callable_identity = {is_virtual = true
// CHECK-SAME: virtual_root_alternatives = [{declaring_class_usr = "[[ROOT_CLASS:[^"]+]]", method_usr = "[[ROOT_METHOD:[^"]+]]"}]
// CHECK-SAME: virtual_root_declaring_class_usr = "[[ROOT_CLASS]]"
// CHECK-SAME: virtual_root_method_usr = "[[ROOT_METHOD]]"

// A nonvirtual method owns no virtual-root pair; self is never synthesized as
// a root merely because the identity helper can walk zero override edges.
// CHECK: cir.func {{[^@]*}} @_ZN7Derived8ordinaryEv
// CHECK-SAME: ast_method_callable_identity = {is_virtual = false, method_declaring_class_usr = "{{[^"]+}}", method_symbol = @_ZN7Derived8ordinaryEv, method_usr = "{{[^"]+}}"}


// One final overrider can own two unrelated vtable slots. Preserve the exact
// sorted alternatives instead of choosing one root or encoding absence.
// CHECK: cir.func {{[^@]*}} @_ZN12MultiDerived6sharedEv
// CHECK-SAME: ast_method_callable_identity = {is_virtual = true
// CHECK-SAME: virtual_root_alternatives = [{declaring_class_usr = "[[ALT_CLASS:[^"]+]]", method_usr = "[[ALT_METHOD:[^"]+]]"}, {declaring_class_usr = "[[OTHER_CLASS:[^"]+]]", method_usr = "[[OTHER_METHOD:[^"]+]]"}]
// CHECK-NOT: virtual_root_declaring_class_usr