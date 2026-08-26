// RUN: printf 'source-root:10:1:10:1:%s|_Z13selected_rootIiE\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir

#define DECLARE_ROOT_AND_METHOD()                                            \
  template <class T> inline constexpr int selected_root = sizeof(T);        \
  template <class T> struct Dependent {                                     \
    static int method() { return sizeof(T); }                               \
  };
DECLARE_ROOT_AND_METHOD()

int consume_root = selected_root<int>;

// CHECK: cir.selected_decl_root_definitions = {_Z13selected_rootIiE = "_Z13selected_rootIiE"}
// CHECK: cir.global{{.*}} @_Z13selected_rootIiE
// CHECK-NOT: @_ZN9DependentIiE6methodEv
