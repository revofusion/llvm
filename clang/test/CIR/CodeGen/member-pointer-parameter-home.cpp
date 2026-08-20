// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s

// Parameter homes bypass ordinary automatic-variable allocation. Inherited
// constructors synthesize an unnamed member-pointer parameter, but its alloca
// must still retain the exact target class carried by the ParmVarDecl type.
template <class Target>
struct Driver {
  explicit Driver(Target target) : target_(target) {}
  Target target_;
};

template <class Target>
struct DerivedDriver : Driver<Target> {
  using Driver<Target>::Driver;
};

struct Fixture {
  void test(int);
};

using Method = void (Fixture::*)(int);

void construct() { DerivedDriver<Method> driver(&Fixture::test); }

// CHECK-LABEL: cir.func{{.*}}DerivedDriver{{.*}}special_member
// CHECK: cir.alloca ""{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Fixture"}
// CHECK: cir.store %arg1
