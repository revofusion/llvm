// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s

template <class T>
struct EndpointTraits {
  int primary;
};

struct EndpointValue {};

void *erase_endpoint_traits(EndpointTraits<EndpointValue> *traits) {
  return static_cast<void *>(traits);
}

template <>
struct EndpointTraits<EndpointValue> {
  long specialized;
};

long read_specialized(EndpointTraits<EndpointValue> *traits) {
  return traits->specialized;
}

// Completing the cast endpoint must wait until Sema has selected the explicit
// specialization. The primary definition must never be instantiated by CIRGen.
// CHECK-COUNT-1: !rec_EndpointTraits3CEndpointValue3E = !cir.struct<"EndpointTraits<EndpointValue>" {!s64i}>
// CHECK: cir.record_decl_identities = {{.*}}"EndpointTraits<EndpointValue>" = "[[ENDPOINT_TRAITS_USR:[^"]+]]"
// CHECK-NOT: !cir.struct<"EndpointTraits<EndpointValue>" {!s32i}>
// CHECK-LABEL: cir.func{{.*}} @_Z21erase_endpoint_traitsP14EndpointTraitsI13EndpointValueE
// CHECK: cir.cast bitcast
// CHECK-SAME: source_record_schema = !rec_EndpointTraits3CEndpointValue3E
// CHECK-SAME: source_record_usr = "[[ENDPOINT_TRAITS_USR]]"
// CHECK-LABEL: cir.func{{.*}} @_Z16read_specializedP14EndpointTraitsI13EndpointValueE
// CHECK: cir.get_member {{.*}}[0]
// CHECK-SAME: name = "specialized"
