// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s

namespace {
template <class T>
struct Observer {
  virtual void changed(T) {}
};

template <class T>
void consume(void (Observer<T>::*)(T)) {}

template <class T>
void selectMethod() {
  consume<T>(&Observer<T>::changed);
}
} // namespace

extern "C" void instantiateMethodIdentity() {
  selectMethod<int>();
  Observer<int> observer;
  observer.Observer<int>::changed(1);
}

// A concrete class-template method's FuncOp owns the same exact method,
// declaring-class, and virtual-root identities as constants naming it.
// CHECK: cir.const {{.*}}ast_member_function_pointer_constant = {is_virtual = true, kind = "method", method_declaring_class_usr = "[[CLASS:[^"]+]]", method_symbol = @[[METHOD:_ZN[^, ]+]], method_usr = "[[METHOD_USR:[^"]+]]", {{.*}}virtual_root_method_usr = "[[ROOT_USR:[^"]+]]"}
// CHECK: cir.func {{.*}} @[[METHOD]](
// CHECK-SAME: attributes {{.*}}ast_method_callable_identity = {is_virtual = true, method_declaring_class_usr = "[[CLASS]]", method_symbol = @[[METHOD]], method_usr = "[[METHOD_USR]]", {{.*}}virtual_root_method_usr = "[[ROOT_USR]]"}
