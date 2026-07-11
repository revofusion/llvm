// RUN: %clang_cc1 -std=c++20 -triple arm64-apple-macosx -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

struct AnnotatedField {
  constexpr AnnotatedField(const void *value) : value(value) {}
  const void *const value [[clang::annotate("raw_ptr_exclusion")]];
};

template <class T>
struct Wrapper {
  constexpr Wrapper(const T *value = nullptr) : field(value) {}
  AnnotatedField field;
};

template struct Wrapper<int>;

// CHECK-LABEL: cir.func {{.*}}AnnotatedFieldC2
// CHECK: cir.get_member {{.*}}{name = "value"}
// CHECK: cir.store
