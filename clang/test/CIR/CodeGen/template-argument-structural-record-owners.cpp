// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir %s -o - | FileCheck %s

// A record nested under a function type is exact template-argument ownership,
// just like a direct record argument. Two sibling closures from one macro
// argument therefore own distinct Traits specializations and base layouts.
template <class T>
struct Base {};
template <class T>
struct Traits;
template <class R, class A>
struct Traits<R (*)(A)> : Base<A> {};
template <class... T>
struct TypeList {};
template <unsigned N, class List, class... Accum>
struct Take {
  int value;
};
template <unsigned N, class T, class... List, class... Accum>
struct Take<N, TypeList<T, List...>, Accum...>
    : Take<N - 1, TypeList<List...>, Accum..., T> {};


void consume(const void*);
template <class F>
void use(F) {
  Traits<void (*)(F)> value;
  Take<1, TypeList<F&, int>> taken;
  taken.value = 0;
  consume(&value);
}
#define USE_TWICE(fn) (use(fn), use(fn))
void structural_record_owners() { USE_TWICE([] {}); }

// CHECK: cir.record_decl_identities = {
// CHECK-SAME: "Take<1{{[^"]*}}>" = "[[TAKE_FIRST:c:[^"]+@S@Take>[^"]+#argowners:[^"]+]]"
// CHECK-NOT: "Take<1{{[^"]*}}>.0" = "[[TAKE_FIRST]]"
// CHECK-SAME: "Take<1{{[^"]*}}>.0" = "[[TAKE_SECOND:c:[^"]+@S@Take>[^"]+#argowners:[^"]+]]"
// CHECK-SAME: "Traits<void (*)((lambda at {{.*}}))>" = "[[FIRST:c:[^"]+@S@Traits>[^"]+#argowners:[^"]+]]"
// CHECK-NOT: "Traits<void (*)((lambda at {{.*}}))>.0" = "[[FIRST]]"
// CHECK-SAME: "Traits<void (*)((lambda at {{.*}}))>.0" = "[[SECOND:c:[^"]+@S@Traits>[^"]+#argowners:[^"]+]]"
// CHECK: cir.base_class_addr
// CHECK-SAME: ast_base_record_usr = "{{[^"]+#argowners:[^"]+}}"
// CHECK-SAME: ast_derived_record_usr = "{{[^"]+#argowners:[^"]+}}"
