// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s

using size_t = __SIZE_TYPE__;
void *operator new(size_t, void *) noexcept;

struct Marker {};
template <class Tag> struct Payload {
  long payload;
};
using Replacement = Payload<Marker>;

template <class T> union OptionalStorage {
  char empty;
  T value;

  OptionalStorage() : empty() {}
  ~OptionalStorage() {}

  void assign(T &&source) {
    ::new ((void *)&value) T(static_cast<T &&>(source));
  }
};

void instantiate_optional_copy(OptionalStorage<Replacement> &storage,
                               Replacement &&source) {
  storage.assign(static_cast<Replacement &&>(source));
}

// The trivial placement constructor is ABI-lowered to cir.copy. Its pointer
// type alone cannot distinguish the replacement object from the enclosing
// Optional storage, so the copy owns all three exact RecordDecl endpoints.
// The module map must bind that same late-completed template specialization;
// the exporter resolves the TypeAttr through this exact producer identity.
// CHECK: [[SCHEMA:!rec_Payload3CMarker3E]] = !cir.
// CHECK: cir.record_decl_identities = {{.*}}"Payload<Marker>" = "[[USR:[^"]+]]"
// CHECK-LABEL: cir.func{{.*}} @{{.*}}assign{{.*}}(
// CHECK: cir.copy
// CHECK-SAME: ast_copy_schema = {
// CHECK-SAME: destination_record_schema = [[SCHEMA]]
// CHECK-SAME: destination_record_usr = "[[USR]]"
// CHECK-SAME: replacement_record_schema = [[SCHEMA]]
// CHECK-SAME: replacement_record_usr = "[[USR]]"
// CHECK-SAME: source_record_schema = [[SCHEMA]]
// CHECK-SAME: source_record_usr = "[[USR]]"
