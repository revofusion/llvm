// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-aeneas-metadata %s -o - | FileCheck %s

enum Mode : unsigned { First = 0, Second = 1 };

struct Options {
  Mode mode;
  unsigned prefix;
  unsigned enabled : 1;
};

template <class C, class M>
__attribute__((noinline)) int readMember(M C::*member, const C &object) {
  return static_cast<int>(object.*member);
}

extern "C" int scalarizedMember(const Options *options) {
  return readMember(&Options::mode, *options);
}

extern "C" void bitfieldConversion(Options *options, Mode mode) {
  options->enabled = mode;
}

// CHECK-LABEL: cir.func{{.*}} @scalarizedMember
// CHECK: cir.const{{.*}}ast_data_member_pointer_constant = {
// CHECK-SAME: field_decl_id = "{{[^"]+}}"
// CHECK-SAME: field_declared_type = [
// CHECK-SAME: field_declared_type_spelling = "Mode"
// CHECK-SAME: field_declaring_record_usr = "c:@S@Options"
// CHECK-SAME: field_offset_bits = 0 : i64
// CHECK-SAME: provenance_kind = "field_decl"
// CHECK-SAME: target_record_usr = "c:@S@Options"

// CHECK-LABEL: cir.func{{.*}} @bitfieldConversion
// CHECK: cir.set_bitfield{{.*}}ast_bitfield_assignment = {
// CHECK-SAME: field_declared_type = [
// CHECK-SAME: field_declared_type_spelling = "unsigned int"
// CHECK-SAME: source_declared_type = [
// CHECK-SAME: source_declared_type_spelling = "Mode"
// CHECK-SAME: storage_type = !u8i
