// RUN: printf 'source-root:7:1:7:46:%s|_ZN7fixture6IsSameIiiEE\npattern-usr:43:_ZN7fixture3Any6TypeIdIPKNS_8FunctionEE2IdEc:@N@fixture@S@Any@ST>1#T@TypeId@Id\nsource-root:17:1:18:29:%s|_ZN7fixture5StateIiE3PtrE\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir --implicit-check-not=@_Z9unrelatedv

namespace fixture {
template <class T, class U>
constexpr bool IsSame = sizeof(T) == sizeof(U);

struct Function;
struct Any {
  template <class T> struct TypeId { static const char Id; };
};
template <class T>
const char Any::TypeId<T>::Id = 0;

template <class T> struct State { static thread_local int *Ptr; };
template <class T>
thread_local int *State<T>::Ptr;
const char *force_type_id_instantiation =
    &Any::TypeId<const Function *>::Id;
int **force_state_instantiation = &State<int>::Ptr;
} // namespace fixture

int unrelated() { return 1; }

// CHECK-DAG: _ZN7fixture6IsSameIiiEE =
// CHECK-COUNT-1: _ZN7fixture3Any6TypeIdIPKNS_8FunctionEE2IdE =
// CHECK-DAG: _ZN7fixture5StateIiE3PtrE =
// CHECK: ast_variable_template_dependent_initializer
