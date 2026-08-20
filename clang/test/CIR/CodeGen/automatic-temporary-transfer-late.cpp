// RUN: echo "_Z13use_bind_oncev" > %t.roots
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o - | FileCheck %s

struct Callback {
  Callback();
  Callback(Callback &&);
  ~Callback();
  int state;
};

struct FirstFactory {
  Callback operator()() const;
};
struct SecondFactory {
  Callback operator()() const;
};

template <class Factory> int bind_once(Factory factory) {
  auto callback = factory();
  return callback.state;
}

int use_bind_once() {
  return bind_once(FirstFactory{}) + bind_once(SecondFactory{});
}

// CHECK-LABEL: cir.func{{.*}} @_Z9bind_onceI12FirstFactoryEiT_
// CHECK: cir.alloca "callback"
// CHECK-SAME: ast_automatic_object_identity = {
// CHECK-SAME: declaration_ordinal = [[FIRST_ORDINAL:[0-9]+]] : i64
// CHECK-SAME: declaration_usr = "[[FIRST_USR:[^"]+]]"
// CHECK-SAME: ast_temporary_object_identities = [
// CHECK-SAME: transferred_to_automatic_decl_ordinal = [[FIRST_ORDINAL]] : i64
// CHECK-SAME: transferred_to_automatic_decl_usr = "[[FIRST_USR]]"
//
// CHECK-LABEL: cir.func{{.*}} @_Z9bind_onceI13SecondFactoryEiT_
// CHECK: cir.alloca "callback"
// CHECK-SAME: ast_automatic_object_identity = {
// CHECK-SAME: declaration_ordinal = [[SECOND_ORDINAL:[0-9]+]] : i64
// CHECK-SAME: declaration_usr = "[[SECOND_USR:[^"]+]]"
// CHECK-SAME: ast_temporary_object_identities = [
// CHECK-SAME: transferred_to_automatic_decl_ordinal = [[SECOND_ORDINAL]] : i64
// CHECK-SAME: transferred_to_automatic_decl_usr = "[[SECOND_USR]]"
