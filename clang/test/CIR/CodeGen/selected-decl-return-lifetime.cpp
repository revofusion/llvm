// RUN: printf '_Z16direct_temporaryv\n_Z16braced_aggregatev\n_Z16branch_temporaryb\n_Z18template_temporaryIiE7Payloadv\n_Z11nrvo_resultv\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --implicit-check-not=@_Z9unrelatedv --input-file=%t.cir

struct Payload {
  Payload();
  Payload(const Payload &);
  ~Payload();
};

struct Envelope {
  Payload payload;
};

void inspect(const Payload &);

Payload direct_temporary() {
  return Payload{};
}

Envelope braced_aggregate() {
  return {Payload{}};
}

Payload branch_temporary(bool branch) {
  if (branch)
    return Payload{};
  return Payload{};
}

template <class T>
Payload template_temporary() {
  return Payload{};
}

template Payload template_temporary<int>();

Payload nrvo_result() {
  Payload result;
  inspect(result);
  return result;
}

Payload unrelated() {
  return Payload{};
}

// A selected definition returning a direct prvalue constructs into caller-owned
// return storage. Its alloca must never acquire local temporary identity.
// CHECK-LABEL: cir.func{{.*}} @_Z16direct_temporaryv(){{.*}} {
// CHECK-NOT: ast_temporary_object_identities
// CHECK: %[[DIRECT_RET:.*]] = cir.alloca "__retval"
// CHECK-NOT: ast_temporary_object_identities
// CHECK: cir.return

// The same ownership rule applies when aggregate initialization is braced.
// CHECK-LABEL: cir.func{{.*}} @_Z16braced_aggregatev(){{.*}} {
// CHECK-NOT: ast_temporary_object_identities
// CHECK: %[[BRACED_RET:.*]] = cir.alloca "__retval"
// CHECK-NOT: ast_temporary_object_identities
// CHECK: cir.return

// Both branch-local CXXBindTemporaryExpr nodes target the one caller-owned
// return allocation; neither may turn it into a function-local cleanup owner.
// CHECK-LABEL: cir.func{{.*}} @_Z16branch_temporaryb({{.*}}){{.*}} {
// CHECK-NOT: ast_temporary_object_identities
// CHECK: %[[BRANCH_RET:.*]] = cir.alloca "__retval"
// CHECK-NOT: ast_temporary_object_identities
// CHECK: cir.return

// Selected template specialization bodies obey the same exact allocation
// ownership rule even though their definition originates in a template.
// CHECK-LABEL: cir.func{{.*}} @_Z18template_temporaryIiE7Payloadv(){{.*}} {
// CHECK-NOT: ast_temporary_object_identities
// CHECK: %[[TEMPLATE_RET:.*]] = cir.alloca "__retval"
// CHECK-NOT: ast_temporary_object_identities
// CHECK: cir.return

// NRVO is distinct: the exact automatic VarDecl owns cleanup identity even
// though its storage is the return allocation.
// CHECK-LABEL: cir.func{{.*}} @_Z11nrvo_resultv(){{.*}} {
// CHECK-NOT: ast_temporary_object_identities
// CHECK: %[[NRVO_RET:.*]] = cir.alloca "__retval"
// CHECK-SAME: ast_automatic_object_identity = {{.*}}declaration_usr = "{{[^"]+}}"{{.*}}function = @_Z11nrvo_resultv
// CHECK-NOT: ast_temporary_object_identities
// CHECK: cir.return
