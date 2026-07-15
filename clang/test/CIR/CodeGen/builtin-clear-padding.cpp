// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR

struct FullBytePadding {
  unsigned char value;
  unsigned int payload;
};

// CIR-LABEL: @{{.*}}clear_full_byte_padding
// CIR: %[[FULL_BASE:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!rec_FullBytePadding> -> !cir.ptr<!u8i>
// CIR-NEXT: %[[FULL_ZERO:.*]] = cir.const #cir.int<0> : !u8i
// CIR-NEXT: %[[FULL_OFFSET_1:.*]] = cir.const #cir.int<1> : !u64i
// CIR-NEXT: %[[FULL_BYTE_1:.*]] = cir.ptr_stride %[[FULL_BASE]], %[[FULL_OFFSET_1]] : (!cir.ptr<!u8i>, !u64i) -> !cir.ptr<!u8i>
// CIR-NEXT: cir.store align(1) %[[FULL_ZERO]], %[[FULL_BYTE_1]] : !u8i, !cir.ptr<!u8i>
// CIR-NEXT: %[[FULL_OFFSET_2:.*]] = cir.const #cir.int<2> : !u64i
// CIR-NEXT: %[[FULL_BYTE_2:.*]] = cir.ptr_stride %[[FULL_BASE]], %[[FULL_OFFSET_2]] : (!cir.ptr<!u8i>, !u64i) -> !cir.ptr<!u8i>
// CIR-NEXT: cir.store align(2) %[[FULL_ZERO]], %[[FULL_BYTE_2]] : !u8i, !cir.ptr<!u8i>
// CIR-NEXT: %[[FULL_OFFSET_3:.*]] = cir.const #cir.int<3> : !u64i
// CIR-NEXT: %[[FULL_BYTE_3:.*]] = cir.ptr_stride %[[FULL_BASE]], %[[FULL_OFFSET_3]] : (!cir.ptr<!u8i>, !u64i) -> !cir.ptr<!u8i>
// CIR-NEXT: cir.store align(1) %[[FULL_ZERO]], %[[FULL_BYTE_3]] : !u8i, !cir.ptr<!u8i>
void clear_full_byte_padding(FullBytePadding *object) {
  __builtin_clear_padding(object);
}

struct PartialBytePadding {
  unsigned char value : 3;
};

// CIR-LABEL: @{{.*}}clear_partial_byte_padding
// CIR: %[[PARTIAL_BASE:.*]] = cir.cast bitcast %{{.*}} : !cir.ptr<!rec_PartialBytePadding> -> !cir.ptr<!u8i>
// CIR: %[[PARTIAL_LOAD:.*]] = cir.load align(1) %{{.*}} : !cir.ptr<!u8i>, !u8i
// CIR-NEXT: %[[PARTIAL_MASK:.*]] = cir.const #cir.int<7> : !u8i
// CIR-NEXT: %[[PARTIAL_VALUE:.*]] = cir.and %[[PARTIAL_LOAD]], %[[PARTIAL_MASK]] : !u8i
// CIR-NEXT: cir.store align(1) %[[PARTIAL_VALUE]], %{{.*}} : !u8i, !cir.ptr<!u8i>
void clear_partial_byte_padding(PartialBytePadding *object) {
  __builtin_clear_padding(object);
}
