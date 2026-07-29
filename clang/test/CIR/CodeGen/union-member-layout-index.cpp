// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR

struct Owner {
  int member;
};

struct Payload {
  int Owner::*member;
};

union CustomStorage {
  unsigned : 0;
  Payload value;

  CustomStorage() {}
  ~CustomStorage() {}
};

Payload &custom_value(CustomStorage &storage) { return storage.value; }

// CIR: !rec_CustomStorage = !cir.union<"CustomStorage" {!rec_Payload}>
// CIR-LABEL: cir.func {{.*}} @{{.*}}custom_value
// CIR: cir.get_member %{{.*}}[0] {{.*}}name = "value"{{.*}} : !cir.ptr<!rec_CustomStorage> -> !cir.ptr<!rec_Payload>

struct AnonymousStorage {
  union {
    unsigned : 0;
    Payload __value_;
  };
};

Payload &anonymous_value(AnonymousStorage &storage) {
  return storage.__value_;
}

// CIR-LABEL: cir.func {{.*}} @{{.*}}anonymous_value
// CIR: cir.get_member %{{.*}}[0] {{.*}}name = "__value_"{{.*}} -> !cir.ptr<!rec_Payload>
