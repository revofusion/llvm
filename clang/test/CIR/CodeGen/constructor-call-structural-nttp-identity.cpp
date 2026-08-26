// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir %s -o - | FileCheck %s --implicit-check-not='constructor_usr = ""'

// A class-template constructor whose specialization is owned by a structural
// non-type template argument must retain one canonical declaration identity.
// The runtime call and the emitted base-variant definition must agree through
// producer-owned USR and ABI facts; qualified-name or type-shape recovery is
// deliberately insufficient because every specialization has the same spelling.
template <unsigned N>
struct StringLiteral {
  char value[N];

  constexpr StringLiteral(const char (&text)[N]) : value{} {
    for (unsigned i = 0; i != N; ++i)
      value[i] = text[i];
  }
};

template <StringLiteral Name>
struct ZoneWithName {
  int *zone;

  explicit ZoneWithName(int *value) : zone(value) {
    *zone += Name.value[0];
  }
};

int constructZoneWithName(int *zone) {
  ZoneWithName<"zone"> named(zone);
  return *named.zone;
}

// The call uses the complete-object entry point, preserves it as the canonical
// symbol, and publishes a nonempty exact canonical declaration USR.
// CHECK-LABEL: cir.func{{.*}} @_Z21constructZoneWithNamePi(
// CHECK: cir.call @[[CTOR_COMPLETE:_ZN[^ (]+C1EPi]](
// CHECK-SAME: ast_constructor_call = {callee_symbol = "[[CTOR_COMPLETE]]", canonical_symbol = "[[CTOR_COMPLETE]]", constructor_usr = "[[CTOR_USR:c:[^"]+]]", variant = "complete"}

// The emitted base-variant definition is the declaration identity authority.
// Its producer USR must agree exactly with the call identity instead of forcing
// a same-spelling constructor lookup.
// CHECK-LABEL: cir.func no_inline comdat linkonce_odr private @
// CHECK-SAME: [[CTOR_BASE:_ZN[^ (]+C2EPi]](
// CHECK-SAME: ast_decl_linkage_name = "[[CTOR_BASE]]"
// CHECK-SAME: ast_decl_usr = "[[CTOR_USR]]"
