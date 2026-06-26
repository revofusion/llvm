// RUN: %clang_cc1 -triple arm64-apple-macosx15.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

enum class RootIndex : unsigned short { A = 7 };

struct OptionalDestructBase {
  union Storage {
    signed char null_state;
    RootIndex value;
    constexpr Storage(RootIndex value) : value(value) {}
  } storage;
  bool engaged;
  constexpr OptionalDestructBase(RootIndex value)
      : storage(value), engaged(true) {}
};

struct OptionalStorageBase : OptionalDestructBase {
  constexpr OptionalStorageBase(RootIndex value) : OptionalDestructBase(value) {}
};

struct OptionalCopyBase : OptionalStorageBase {
  constexpr OptionalCopyBase(RootIndex value) : OptionalStorageBase(value) {}
};

struct Optional : OptionalCopyBase {
  constexpr Optional(RootIndex value) : OptionalCopyBase(value) {}
};

Optional global = Optional(RootIndex::A);

// CHECK: !rec_OptionalDestructBase2Ebase = !cir.record<struct "OptionalDestructBase.base" packed
// CHECK: !rec_Optional = !cir.record<struct "Optional" padded
// CHECK: cir.global external @global = #cir.const_record<
// CHECK-SAME: #cir.int<7> : !u16i
// CHECK-SAME: #true
// CHECK-SAME: #cir.int<0> : !u8i}> : !rec_Optional
