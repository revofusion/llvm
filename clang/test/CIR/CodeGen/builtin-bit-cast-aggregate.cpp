// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR

struct From {
  unsigned long long bits;
};

struct To {
  unsigned long long bits;
};

To bitcast_aggregate(const From &from) {
  return __builtin_bit_cast(To, from);
}

// CIR: cir.func{{.*}} @_Z17bitcast_aggregateRK4From(
// CIR:   %[[FROM_PTR:.*]] = cir.load{{.*}} : !cir.ptr<!cir.ptr<!rec_From>>, !cir.ptr<!rec_From>
// CIR:   %[[TO_PTR:.*]] = cir.cast bitcast %[[FROM_PTR]] : !cir.ptr<!rec_From> -> !cir.ptr<!rec_To>
// CIR:   cir.copy %[[TO_PTR]] to %{{.*}} : !cir.ptr<!rec_To>
