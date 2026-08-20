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

struct EndpointLeaf {
  int value;
};

struct EndpointMember {
  EndpointLeaf leaf;
};

struct EndpointOwner {
  EndpointMember member;
};

EndpointMember return_member_endpoint(const EndpointOwner &owner) {
  return owner.member;
}

int nested_member_endpoint(const EndpointOwner &owner) {
  return owner.member.leaf.value;
}

struct AnonymousEndpointOwner {
  struct {
    int value;
  } endpoint;
};

int anonymous_member_endpoint(const AnonymousEndpointOwner &owner) {
  return owner.endpoint.value;
}

EndpointMember *lambda_capture_endpoint(EndpointMember &member) {
  auto captured = [&member] { return &member; };
  return captured();
}

// CHECK-LABEL: cir.func {{.*}}AnnotatedFieldC2
// CHECK: cir.get_member {{.*}}{ast_declaring_record_usr = "c:@S@AnnotatedField", ast_member_decl_usr = "clang-field:19:c:@S@AnnotatedField:c:@S@AnnotatedField@FI@value", ast_member_offset_bits = 0 : i64, name = "value"}
// CHECK: cir.store

// CHECK-LABEL: cir.func {{.*}}zero_size_field_address
// CHECK: cir.cast bitcast {{.*}} loc(#[[FIELD_LOC:loc[0-9]+]])

// CHECK-LABEL: cir.func {{.*}} @_Z22return_member_endpointRK13EndpointOwner
// CHECK: %[[RETVAL:.*]] = cir.alloca "__retval" {{.*}} : !cir.ptr<!rec_EndpointMember>
// CHECK: %[[RETURN_MEMBER:.*]] = cir.get_member {{.*}}[0] {ast_declaring_record_usr = "c:@S@EndpointOwner", ast_member_decl_usr = "clang-field:18:c:@S@EndpointOwner:c:@S@EndpointOwner@FI@member", ast_member_offset_bits = 0 : i64, ast_member_record_endpoint = {declaring_record_usr = "c:@S@EndpointOwner", field_decl_usr = "clang-field:18:c:@S@EndpointOwner:c:@S@EndpointOwner@FI@member", field_offset_bits = 0 : i64, field_storage_type = !rec_EndpointMember, record_schema = !rec_EndpointMember, record_usr = "c:@S@EndpointMember", source_type = [{{.*}}]}, name = "member"} : !cir.ptr<!rec_EndpointOwner> -> !cir.ptr<!rec_EndpointMember>

// CHECK-LABEL: cir.func {{.*}} @_Z22nested_member_endpointRK13EndpointOwner
// CHECK: %[[OUTER_MEMBER:.*]] = cir.get_member {{.*}}[0] {ast_declaring_record_usr = "c:@S@EndpointOwner", ast_member_decl_usr = "clang-field:18:c:@S@EndpointOwner:c:@S@EndpointOwner@FI@member", ast_member_offset_bits = 0 : i64, ast_member_record_endpoint = {declaring_record_usr = "c:@S@EndpointOwner", field_decl_usr = "clang-field:18:c:@S@EndpointOwner:c:@S@EndpointOwner@FI@member", field_offset_bits = 0 : i64, field_storage_type = !rec_EndpointMember, record_schema = !rec_EndpointMember, record_usr = "c:@S@EndpointMember", source_type = [{{.*}}]}, name = "member"} : !cir.ptr<!rec_EndpointOwner> -> !cir.ptr<!rec_EndpointMember>
// CHECK: %[[INNER_MEMBER:.*]] = cir.get_member %[[OUTER_MEMBER]][0] {ast_declaring_record_usr = "c:@S@EndpointMember", ast_member_decl_usr = "clang-field:19:c:@S@EndpointMember:c:@S@EndpointMember@FI@leaf", ast_member_offset_bits = 0 : i64, ast_member_record_endpoint = {declaring_record_usr = "c:@S@EndpointMember", field_decl_usr = "clang-field:19:c:@S@EndpointMember:c:@S@EndpointMember@FI@leaf", field_offset_bits = 0 : i64, field_storage_type = !rec_EndpointLeaf, record_schema = !rec_EndpointLeaf, record_usr = "c:@S@EndpointLeaf", source_type = [{{.*}}]}, name = "leaf"} : !cir.ptr<!rec_EndpointMember> -> !cir.ptr<!rec_EndpointLeaf>

// A field-owned anonymous RecordDecl is authenticated by both its exact
// FieldDecl and its producer schema identity; an importer never has to infer
// either endpoint from the result pointer type.
// CHECK-LABEL: cir.func {{.*}} @_Z25anonymous_member_endpointRK22AnonymousEndpointOwner
// CHECK: cir.get_member {{.*}} {ast_declaring_record_usr = "c:@S@AnonymousEndpointOwner", ast_member_decl_usr = "[[ANON_FIELD:[^"]+]]", ast_member_offset_bits = 0 : i64, ast_member_record_endpoint = {declaring_record_usr = "c:@S@AnonymousEndpointOwner", field_decl_usr = "[[ANON_FIELD]]", field_offset_bits = 0 : i64, field_storage_type = [[ANON_SCHEMA:!rec_[^, }]+]], record_schema = [[ANON_SCHEMA]], record_usr = "[[ANON_RECORD:[^"]+]]"

// Lambda captures are implicit FieldDecls with no Clang USR. Their enclosing
// lambda identity and exact field ordinal preserve the endpoint structurally.
// CHECK-LABEL: cir.func {{.*}} @_ZZ23lambda_capture_endpointR14EndpointMemberENK3$_0clEv
// CHECK: cir.get_member {{.*}} {ast_declaring_record_usr = "[[LAMBDA_OWNER:[^"]+]]", ast_member_decl_usr = "[[LAMBDA_FIELD:clang-field-ordinal:[^"]+:0]]", ast_member_offset_bits = 0 : i64, ast_member_record_endpoint = {declaring_record_usr = "[[LAMBDA_OWNER]]", field_decl_usr = "[[LAMBDA_FIELD]]"
// CHECK: #[[FIELD_LOC]] = loc(fused<{{.*}}ast_declaring_record_usr = "c:@S@ZeroSizeOwner"{{.*}}ast_member_decl_usr = "clang-field:18:c:@S@ZeroSizeOwner:c:@S@ZeroSizeOwner@FI@empty"{{.*}})
