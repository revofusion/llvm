// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir --check-prefix=CIR
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck %s --input-file=%t.ll --check-prefix=LLVM

namespace std {
template <class T> class initializer_list {
  const T *begin_;
  __SIZE_TYPE__ size_;
};
} // namespace std

struct Metadata {
  enum class Platforms {
    Windows,
    Mac,
    Linux,
    ChromeOS,
  };

  static constexpr std::initializer_list<Platforms> kAllDesktopPlatforms{
      Platforms::Windows, Platforms::Mac, Platforms::Linux,
      Platforms::ChromeOS};
};

std::initializer_list<Metadata::Platforms> getAllDesktopPlatforms() {
  return Metadata::kAllDesktopPlatforms;
}

// The backing array is a lifetime-extended temporary with the weak linkage of
// the inline static data member. It must be emitted once in the same COMDAT.
// CIR: cir.global constant linkonce_odr comdat @[[TEMP:_ZGRN8Metadata20kAllDesktopPlatformsE_]] = #cir.const_array<[#cir.int<0>, #cir.int<1>, #cir.int<2>, #cir.int<3>]> : !cir.array<!s32i x 4> {alignment = 4 : i64}
// CIR-NOT: cir.global {{.*}} @[[TEMP]]

// LLVM: @[[TEMP:_ZGRN8Metadata20kAllDesktopPlatformsE_]] = linkonce_odr constant [4 x i32] [i32 0, i32 1, i32 2, i32 3], comdat, align 4
// LLVM-NOT: @[[TEMP]] =
