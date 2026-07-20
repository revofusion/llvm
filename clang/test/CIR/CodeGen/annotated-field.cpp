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

struct EmptyField {};

struct ZeroSizeOwner {
  [[no_unique_address]] EmptyField empty;
};

extern "C" EmptyField *zero_size_field_address(ZeroSizeOwner *owner) {
  return &owner->empty;
}

// CHECK-LABEL: cir.func {{.*}}AnnotatedFieldC2
// CHECK: cir.get_member {{.*}}{ast_declaring_record_usr = "c:@S@AnnotatedField", ast_member_decl_usr = "c:@S@AnnotatedField@FI@value", ast_member_offset_bits = 0 : i64, name = "value"}
// CHECK: cir.store

// CHECK-LABEL: cir.func {{.*}}zero_size_field_address
// CHECK: cir.cast bitcast {{.*}} loc(#[[FIELD_LOC:loc[0-9]+]])
// CHECK: #[[FIELD_LOC]] = loc(fused<{{.*}}ast_declaring_record_usr = "c:@S@ZeroSizeOwner"{{.*}}ast_member_decl_usr = "c:@S@ZeroSizeOwner@FI@empty"{{.*}})
