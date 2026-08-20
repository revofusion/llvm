// A member-pointer constant that Clang materializes as a private `__const`
// global carries no method to name when the value is null. Its pointee kind
// and owning record are still fully determined by the declared type, and a
// consumer reading the global has no other channel to recover them: the CIR
// record is an anonymous {pointer, offset} pair shared by every member pointer
// in the module. Withholding the target identity because the value happens to
// be null would erase the class the storage belongs to.
//
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir \
// RUN:   -fclangir-aeneas-metadata -emit-cir %s -o - | FileCheck %s

struct Settings {
  bool disable_local_echo() const;
  bool render_to_associated_sink() const;
};

using BoolMember = bool (Settings::*)() const;

void use_member(BoolMember member);

// A loop-local null member pointer: Clang emits `__const.f.member` and copies
// from it on every iteration.
void loop_local_null(int count) {
  for (int index = 0; index < count; ++index) {
    BoolMember member = nullptr;
    use_member(member);
  }
}

// CHECK: cir.global {{.*}}@__const.{{[A-Za-z0-9_.]*}}member = #cir.const_record<{#cir.int<0>, #cir.int<0>}>
// CHECK-SAME: ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Settings"}

// Value-initialization reaches the same storage without a null-to-member
// conversion, so nothing rides along to carry the target identity. The global
// still describes a member pointer into `Settings` and must say so.
void loop_local_value_init(int count) {
  for (int index = 0; index < count; ++index) {
    BoolMember member{};
    use_member(member);
  }
}

// CHECK: cir.global {{.*}}@__const.{{[A-Za-z0-9_.]*}}member = #cir.const_record<{#cir.int<0>, #cir.int<0>}>
// CHECK-SAME: ast_member_function_pointer_constant = {kind = "null", null_adjustment_value = "0", null_function_value = "0"}
// CHECK-SAME: ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Settings"}

// A non-null constant global keeps naming its exact method alongside the
// target record, so the added fact never displaces the existing one.
static const BoolMember kMember = &Settings::disable_local_echo;
BoolMember named_member() { return kMember; }

// CHECK: cir.global {{.*}}@_ZL7kMember = #cir.const_record<{#cir.global_view<@_ZNK8Settings18disable_local_echoEv>, #cir.int<0>}>
// CHECK-SAME: ast_member_function_pointer_constant =
// CHECK-SAME: ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Settings"}
