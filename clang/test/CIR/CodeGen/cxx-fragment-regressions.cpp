// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

struct Incomplete;
using IncompleteCallback = void (*)(Incomplete);
IncompleteCallback incomplete_callback;

struct Destructed {
  Destructed();
  Destructed(const Destructed &);
  ~Destructed();
};

void lambda_with_destructed_capture(Destructed value) {
  auto closure = [value] {};
}

int consume_int(int);

template <class... Args> int pack_count(Args... args) {
  return consume_int(static_cast<int>(sizeof...(args)));
}

int use_pack_count() {
  return pack_count(1, 2, 3);
}

int switch_on_bool(bool flag) {
  switch (flag) {
  case true:
    return 1;
  case false:
    return 0;
  }
}

struct MemberPointerOwner {
  int field;
};

union NonZeroInitUnion {
  int MemberPointerOwner::*memberPointer;
};

int MemberPointerOwner::*& access_nonzero_init_union(NonZeroInitUnion *u) {
  return u->memberPointer;
}

// CHECK: !rec_NonZeroInitUnion = !cir.record<union "NonZeroInitUnion" {!cir.data_member<!s32i in !rec_MemberPointerOwner>}>
// CHECK-LABEL: @_Z25access_nonzero_init_unionP16NonZeroInitUnion
// CHECK: cir.get_member {{.*}}[0] {name = "memberPointer"} : !cir.ptr<!rec_NonZeroInitUnion> -> !cir.ptr<!cir.data_member<!s32i in !rec_MemberPointerOwner>>
