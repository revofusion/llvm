// RUN: printf '_Z16selectedTemplateIiEiT_\n_ZN6HolderIiED1Ev\n_Z22selectedLambdaTemplateIiEiT_\n_ZZ22selectedLambdaTemplateIiEiT_ENKUlvE_clEv\n_Z15selectEnclosingIiEiT_\n_ZN10LateMemberIiE7convertIiEEiT_\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir --implicit-check-not=@_Z17unrelatedTemplateIiEiT_

// Force an implicit free function-template specialization into the AST before
// the selected-declaration end-of-translation-unit sweep.
template <typename T>
int selectedTemplate(T value) {
  return value;
}

int forceTemplate = selectedTemplate(1);

template <typename T>
int unrelatedTemplate(T value) {
  return value;
}

int forceUnrelatedTemplate = unrelatedTemplate(2);

template <typename T>
struct Holder {
  ~Holder() {}
};

template struct Holder<int>;

// The body of this specialization is parsed only when its exact enclosing
// root is prepared. The unused lambda is not a call dependency, so its exact
// call-operator root must be discovered by the next selected-root frontier.
template <typename T>
int selectedLambdaTemplate(T value) {
  auto nested = [value] { return value + 1; };
  return value;
}

int forceSelectedLambda = selectedLambdaTemplate(3);

template <typename T>
struct LateMember {
  template <typename U>
  int convert(U value) {
    return value;
  }
};

// Parsing the enclosing specialization introduces the exact convert<int>
// specialization in an unevaluated operand. It is not a call dependency.
template <typename T>
int selectEnclosing(T value) {
  using Selected = decltype(&LateMember<T>::template convert<int>);
  return sizeof(Selected) == 0 ? value : value;
}

int forceSelectedMember = selectEnclosing(4);

// The producer publishes a single exact definition binding for each late
// declaration and lambda selector. This is stronger than merely observing a
// same-spelling cir.func in the output.
// CHECK: cir.selected_decl_root_definitions = {{.*}}_Z16selectedTemplateIiEiT_ = "_Z16selectedTemplateIiEiT_"{{.*}}_ZN10LateMemberIiE7convertIiEEiT_ = "_ZN10LateMemberIiE7convertIiEEiT_"{{.*}}_ZZ22selectedLambdaTemplateIiEiT_ENKUlvE_clEv = "_ZZ22selectedLambdaTemplateIiEiT_ENKUlvE_clEv"{{.*}}

// CHECK-DAG: cir.func{{.*}} @_Z16selectedTemplateIiEiT_

// CHECK-DAG: cir.func{{.*}} @_ZN6HolderIiED1Ev
// CHECK-DAG: cir.func{{.*}} @_ZN6HolderIiED2Ev

// CHECK-DAG: cir.func{{.*}} @_Z22selectedLambdaTemplateIiEiT_
// CHECK-DAG: cir.func{{.*}} @_ZZ22selectedLambdaTemplateIiEiT_ENKUlvE_clEv
// CHECK-DAG: cir.func{{.*}} @_Z15selectEnclosingIiEiT_
// CHECK-DAG: cir.func{{.*}} @_ZN10LateMemberIiE7convertIiEEiT_
