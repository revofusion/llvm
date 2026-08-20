// Selected-decl emission interleaves Sema instantiation with CIRGen: a later
// selected body can complete a specialization that an earlier function's
// object-storage identity already hashed while it was incomplete. Module
// release must reconcile every op-level record identity with the final
// canonical identity, exactly as cir.record_decl_identities already does.
// The roots are plain emission roots on purpose: a parse-symbol root would
// parse late()'s body eagerly and complete Meta<int> before early() emits.
// RUN: printf '_Z5earlyv\n_Z4latev\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir

template <typename T> struct Meta {
  T value;
  int pad;
};

// Box<Meta<int>> completes without completing Meta<int>: the argument record
// is only named through a pointer member.
template <typename A> struct Box {
  A *link;
  int tag;
};

using BoxT = Box<Meta<int>>;

void sink(const BoxT &box);

// The materialized temporary mints BoxT's object-storage identity while
// Meta<int> has no definition, so the #argowners leaf is the undecorated USR.
void early() { sink(BoxT{nullptr, 1}); }

// Parsing late's body instantiates Meta<int>; BoxT's exact identity gains the
// decorated argument-owner leaf and the module map is updated. The alloca in
// early() must carry that same final identity.
int late() {
  Meta<int> meta{7, 0};
  sink(BoxT{&meta, 2});
  return meta.value;
}

// The module identity map binds the final identity for BoxT.
// CHECK: cir.record_decl_identities
// CHECK-SAME: "Box<Meta<int>>" = "[[FINAL:[^"]+]]"

// CHECK: cir.func {{.*}}@_Z5earlyv
// CHECK: cir.alloca {{.*}} !cir.ptr<!rec_Box3CMeta3Cint3E3E>
// CHECK-SAME: record_usr = "[[FINAL]]"

// CHECK: cir.func {{.*}}@_Z4latev
// CHECK: cir.alloca {{.*}} !cir.ptr<!rec_Box3CMeta3Cint3E3E>
// CHECK-SAME: record_usr = "[[FINAL]]"
