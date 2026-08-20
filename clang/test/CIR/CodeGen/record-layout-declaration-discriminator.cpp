// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir %s -o - | FileCheck %s

namespace {
struct HiddenA {};
struct HiddenB {};
}

template <class T>
struct Holder {
  int value;
};

extern "C" int use_hidden_a() {
  Holder<HiddenA> value{};
  return value.value;
}
extern "C" int use_hidden_b() {
  Holder<HiddenB> value{};
  return value.value;
}

// A specialization whose anonymous argument removes external visibility owns
// the exact source declaration discriminator used by importer layout identity.
// Both specializations name the same template declaration and therefore share
// that discriminator, while their exact argument-owner identities stay apart.
// CHECK: cir.record_decl_identities = {
// CHECK-SAME: "Holder<{{.*}}HiddenA>" = "[[FIRST:c:[^"]+@S@Holder>[^"]+#argowners:[^"]+#decl.]][[DECL:[0-9a-f]+]]"
// CHECK-NOT: "Holder<{{.*}}HiddenB>" = "[[FIRST]][[DECL]]"
// CHECK-SAME: "Holder<{{.*}}HiddenB>" = "[[SECOND:c:[^"]+@S@Holder>[^"]+#argowners:[^"]+#decl.]][[DECL]]"
