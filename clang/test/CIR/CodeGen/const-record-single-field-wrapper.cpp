// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

struct Inner {
  unsigned long long words[2];
};

struct Wrapper {
  Inner inner;
};

struct Storage {
  unsigned long long words[2];
};

constexpr Storage make() {
  return {{1, 2}};
}

Wrapper w = __builtin_bit_cast(Wrapper, make());

// CHECK: !rec_Inner = !cir.record<struct "Inner" {!cir.array<!u64i x 2>}>
// CHECK: !rec_Wrapper = !cir.record<struct "Wrapper" {!rec_Inner}>
// CHECK: cir.global external @w = #cir.const_record<{#cir.const_record<{#cir.const_array<[#cir.int<1> : !u64i, #cir.int<2> : !u64i]> : !cir.array<!u64i x 2>}> : !rec_Inner}> : !rec_Wrapper
