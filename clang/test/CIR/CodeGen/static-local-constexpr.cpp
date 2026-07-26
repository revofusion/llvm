// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s

using size_t = __SIZE_TYPE__;

struct Options {
  enum Unit : int { Bytes, Lines, Objects };
  struct Amount {
    const size_t num = 1;
    const Unit unit = Lines;

    template <typename T>
    constexpr Amount toLines() const {
      return unit == Objects ? Amount{num * sizeof(T), Lines} : *this;
    }
  };

  const Amount num = {1, Lines};
  const Amount from = {0, Bytes};
  const int value = 0;
};

template <const Options &opts, typename T>
int read_static() {
  static constexpr Options local = {
      opts.num.template toLines<T>(), opts.from, opts.value};
  return local.num.num + local.from.num;
}

inline constexpr Options object = {
    {1, Options::Objects}, {2, Options::Objects}, 3};
template int read_static<object, int>();

// CHECK: cir.global constant linkonce_odr comdat @[[LOCAL:_ZZ.*local]] = {{.*}} : !rec_anon_struct{{[0-9]*}}
// CHECK-LABEL: cir.func {{.*}} @_Z11read_static
// CHECK: %[[RAW:.*]] = cir.get_global @[[LOCAL]] : !cir.ptr<!rec_anon_struct{{[0-9]*}}>
// CHECK-NEXT: %[[TYPED:.*]] = cir.cast bitcast %[[RAW]] : !cir.ptr<!rec_anon_struct{{[0-9]*}}> -> !cir.ptr<!rec_Options>
// CHECK: cir.get_member %[[TYPED]][0] {{.*}} : !cir.ptr<!rec_Options> -> !cir.ptr<!rec_Options3A3AAmount>
