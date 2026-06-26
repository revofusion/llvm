// RUN: %clang_cc1 -triple arm64-apple-macosx15.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

struct Empty {};

template <typename T>
struct OptionalStorageBase {
  union {
    signed char dummy;
    T value;
  };
  bool engaged;

  constexpr OptionalStorageBase(T v) : value(v), engaged(true) {}
};

template <typename T>
struct Optional : private Empty, private OptionalStorageBase<T> {
  constexpr Optional(T v) : OptionalStorageBase<T>(v) {}
};

enum E : unsigned short { A = 7 };

Optional<E> opt = Optional<E>(A);

struct Base {
  int x;
  char c;
  constexpr Base(int x, char c) : x(x), c(c) {}
};

struct Derived : Base {
  char d;
  constexpr Derived() : Base(1, 2), d(3) {}
};

Derived derived = Derived();

// CHECK: cir.global external @opt = #cir.const_record
// CHECK-SAME: #cir.int<7> : !u16i
// CHECK-SAME: #true
// CHECK: cir.global external @derived = #cir.const_record
// CHECK-SAME: #cir.int<1> : !s32i
// CHECK-SAME: #cir.int<2> : !s8i
// CHECK-SAME: #cir.int<3> : !s8i
