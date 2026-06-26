// RUN: %clang_cc1 -triple arm64-apple-macosx15.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

namespace std {
namespace __Cr {
template <typename T, typename U>
struct pair {
  T first;
  U second;
  constexpr pair(T first, U second) : first(first), second(second) {}
};
}
}

union U {
  signed char dummy;
  std::__Cr::pair<unsigned, unsigned> payload;
  constexpr U() : payload(1, 2) {}
};

U u = U();

// CHECK: !rec_U = !cir.record<union "U"
// CHECK: cir.global external @u = #cir.const_record
// CHECK-SAME: #cir.int<1> : !u32i
// CHECK-SAME: #cir.int<2> : !u32i
