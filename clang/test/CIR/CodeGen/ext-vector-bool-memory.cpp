// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=LLVM

using short16 = short __attribute__((ext_vector_type(16)));
using int4 = int __attribute__((ext_vector_type(4)));
using bool16 = bool __attribute__((ext_vector_type(16)));
using bool4 = bool __attribute__((ext_vector_type(4)));
// CIR-DAG:  !rec_Bool16Holder = !cir.struct<"Bool16Holder" {!u16i}>
// LLVM-DAG: %struct.Bool16Holder = type { i16 }


// This is the shape used by libc++'s SIMD __any_of implementation reached by
// Chromium: convert an integer vector to a boolean vector, spill it, reload it,
// and reduce the reloaded value.
extern "C" bool chromium_any_of(short16 input) {
  bool16 mask = __builtin_convertvector(input, bool16);
  return __builtin_reduce_or(mask);
}

// CIR-LABEL: cir.func{{.*}} @chromium_any_of
// CIR:         %[[MASK:.*]] = cir.alloca "mask" {{.*}} : !cir.ptr<!u16i>
// CIR:         %[[BOOLS:.*]] = cir.cast int_to_bool %{{.*}} : !cir.vector<16 x !s16i> -> !cir.vector<16 x !cir.bool>
// CIR:         %[[PACKED:.*]] = cir.cast bitcast %[[BOOLS]] : !cir.vector<16 x !cir.bool> -> !u16i
// CIR:         cir.store align(2) %[[PACKED]], %[[MASK]] : !u16i, !cir.ptr<!u16i>
// CIR:         %[[RAW:.*]] = cir.load align(2) %[[MASK]] : !cir.ptr<!u16i>, !u16i
// CIR:         %[[UNPACKED:.*]] = cir.cast bitcast %[[RAW]] : !u16i -> !cir.vector<16 x !cir.bool>
// CIR:         %[[REDUCED:.*]] = cir.call_llvm_intrinsic "vector.reduce.or" %[[UNPACKED]] : (!cir.vector<16 x !cir.bool>) -> !cir.bool
// CIR:         cir.store %[[REDUCED]], %[[RET_PTR:.*]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR:         %[[RET:.*]] = cir.load %[[RET_PTR]] : !cir.ptr<!cir.bool>, !cir.bool
// CIR:         cir.return %[[RET]] : !cir.bool

// LLVM-LABEL: define{{.*}} i1 @chromium_any_of
// LLVM:         %[[MASK:.*]] = alloca i16, i64 1, align 2
// LLVM:         %[[BOOLS:.*]] = icmp ne <16 x i16> %{{.*}}, zeroinitializer
// LLVM:         %[[PACKED:.*]] = bitcast <16 x i1> %[[BOOLS]] to i16
// LLVM:         store i16 %[[PACKED]], ptr %[[MASK]], align 2
// LLVM:         %[[RAW:.*]] = load i16, ptr %[[MASK]], align 2
// LLVM:         %[[UNPACKED:.*]] = bitcast i16 %[[RAW]] to <16 x i1>
// LLVM:         %[[REDUCED:.*]] = call i1 @llvm.vector.reduce.or.v16i1(<16 x i1> %[[UNPACKED]])
// LLVM:         %[[REDUCED_BYTE:.*]] = zext i1 %[[REDUCED]] to i8
// LLVM:         store i8 %[[REDUCED_BYTE]], ptr %[[RET_PTR:.*]], align 1
// LLVM:         %[[RET_BYTE:.*]] = load i8, ptr %[[RET_PTR]], align 1
// LLVM:         %[[RET:.*]] = trunc i8 %[[RET_BYTE]] to i1
// LLVM:         ret i1 %[[RET]]

// Vectors narrower than a byte are byte-padded in memory. The explicit zero
// operand makes the padding bits defined before the vector is packed.
extern "C" bool padded_any_of(int4 input) {
  bool4 mask = __builtin_convertvector(input, bool4);
  return __builtin_reduce_or(mask);
}

// CIR-LABEL: cir.func{{.*}} @padded_any_of
// CIR:         %[[MASK4:.*]] = cir.alloca "mask" {{.*}} : !cir.ptr<!u8i>
// CIR:         %[[BOOLS4:.*]] = cir.cast int_to_bool %{{.*}} : !cir.vector<4 x !s32i> -> !cir.vector<4 x !cir.bool>
// CIR:         %[[ZERO4:.*]] = cir.const #cir.zero : !cir.vector<4 x !cir.bool>
// CIR:         %[[PADDED:.*]] = cir.vec.shuffle(%[[BOOLS4]], %[[ZERO4]] : !cir.vector<4 x !cir.bool>) [#cir.int<0> : !s32i, #cir.int<1> : !s32i, #cir.int<2> : !s32i, #cir.int<3> : !s32i, #cir.int<4> : !s32i, #cir.int<4> : !s32i, #cir.int<4> : !s32i, #cir.int<4> : !s32i] : !cir.vector<8 x !cir.bool>
// CIR:         %[[PACKED4:.*]] = cir.cast bitcast %[[PADDED]] : !cir.vector<8 x !cir.bool> -> !u8i
// CIR:         cir.store align(1) %[[PACKED4]], %[[MASK4]] : !u8i, !cir.ptr<!u8i>
// CIR:         %[[RAW4:.*]] = cir.load align(1) %[[MASK4]] : !cir.ptr<!u8i>, !u8i
// CIR:         %[[WIDE4:.*]] = cir.cast bitcast %[[RAW4]] : !u8i -> !cir.vector<8 x !cir.bool>
// CIR:         %[[UNPACKED4:.*]] = cir.vec.shuffle(%[[WIDE4]], %[[WIDE4]] : !cir.vector<8 x !cir.bool>) [#cir.int<0> : !s32i, #cir.int<1> : !s32i, #cir.int<2> : !s32i, #cir.int<3> : !s32i] : !cir.vector<4 x !cir.bool>
// CIR:         %[[REDUCED4:.*]] = cir.call_llvm_intrinsic "vector.reduce.or" %[[UNPACKED4]] : (!cir.vector<4 x !cir.bool>) -> !cir.bool
// CIR:         cir.store %[[REDUCED4]], %[[RET_PTR4:.*]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR:         %[[RET4:.*]] = cir.load %[[RET_PTR4]] : !cir.ptr<!cir.bool>, !cir.bool
// CIR:         cir.return %[[RET4]] : !cir.bool

// LLVM-LABEL: define{{.*}} i1 @padded_any_of
// LLVM:         %[[BOOLS4:.*]] = icmp ne <4 x i32> %{{.*}}, zeroinitializer
// LLVM:         %[[PADDED4:.*]] = shufflevector <4 x i1> %[[BOOLS4]], <4 x i1> zeroinitializer, <8 x i32> <i32 0, i32 1, i32 2, i32 3, i32 4, i32 4, i32 4, i32 4>
// LLVM:         %[[PACKED4:.*]] = bitcast <8 x i1> %[[PADDED4]] to i8
// LLVM:         store i8 %[[PACKED4]], ptr %[[MASK4:.*]], align 1
// LLVM:         %[[RAW4:.*]] = load i8, ptr %[[MASK4]], align 1
// LLVM:         %[[WIDE4:.*]] = bitcast i8 %[[RAW4]] to <8 x i1>
// LLVM:         %[[UNPACKED4:.*]] = shufflevector <8 x i1> %[[WIDE4]], <8 x i1> %[[WIDE4]], <4 x i32> <i32 0, i32 1, i32 2, i32 3>
// LLVM:         %[[REDUCED4:.*]] = call i1 @llvm.vector.reduce.or.v4i1(<4 x i1> %[[UNPACKED4]])
// LLVM:         %[[REDUCED_BYTE4:.*]] = zext i1 %[[REDUCED4]] to i8
// LLVM:         store i8 %[[REDUCED_BYTE4]], ptr %[[RET_PTR4:.*]], align 1
// LLVM:         %[[RET_BYTE4:.*]] = load i8, ptr %[[RET_PTR4]], align 1
// LLVM:         %[[RET4:.*]] = trunc i8 %[[RET_BYTE4]] to i1
// LLVM:         ret i1 %[[RET4]]

struct Bool16Holder {
  bool16 value;
};

extern "C" bool field_roundtrip(Bool16Holder *holder, short16 input) {
  holder->value = __builtin_convertvector(input, bool16);
  return __builtin_reduce_or(holder->value);
}

// CIR-LABEL: cir.func{{.*}} @field_roundtrip
// CIR:         %[[FIELD:.*]] = cir.get_member %{{.*}}[0] {{.*}}name = "value"{{.*}} : !cir.ptr<!rec_Bool16Holder> -> !cir.ptr<!u16i>
// CIR:         %[[FIELD_PACKED:.*]] = cir.cast bitcast %{{.*}} : !cir.vector<16 x !cir.bool> -> !u16i
// CIR:         cir.store align(2) %[[FIELD_PACKED]], %[[FIELD]] : !u16i, !cir.ptr<!u16i>
// CIR:         %[[FIELD_LOAD_ADDR:.*]] = cir.get_member %{{.*}}[0] {{.*}}name = "value"{{.*}} : !cir.ptr<!rec_Bool16Holder> -> !cir.ptr<!u16i>
// CIR:         %[[FIELD_RAW:.*]] = cir.load align(2) %[[FIELD_LOAD_ADDR]] : !cir.ptr<!u16i>, !u16i
// CIR:         %[[FIELD_VALUE:.*]] = cir.cast bitcast %[[FIELD_RAW]] : !u16i -> !cir.vector<16 x !cir.bool>
// CIR:         %[[FIELD_REDUCED:.*]] = cir.call_llvm_intrinsic "vector.reduce.or" %[[FIELD_VALUE]] : (!cir.vector<16 x !cir.bool>) -> !cir.bool
// CIR:         cir.store %[[FIELD_REDUCED]], %[[FIELD_RET_PTR:.*]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR:         %[[FIELD_RET:.*]] = cir.load %[[FIELD_RET_PTR]] : !cir.ptr<!cir.bool>, !cir.bool
// CIR:         cir.return %[[FIELD_RET]] : !cir.bool

// LLVM-LABEL: define{{.*}} i1 @field_roundtrip
// LLVM:         %[[FIELD_ADDR:.*]] = getelementptr inbounds nuw %struct.Bool16Holder, ptr %{{.*}}, i32 0, i32 0
// LLVM:         store i16 %{{.*}}, ptr %[[FIELD_ADDR]], align 2
// LLVM:         %[[FIELD_ADDR_LOAD:.*]] = getelementptr inbounds nuw %struct.Bool16Holder, ptr %{{.*}}, i32 0, i32 0
// LLVM:         %[[FIELD_RAW:.*]] = load i16, ptr %[[FIELD_ADDR_LOAD]], align 2
// LLVM:         %[[FIELD_VALUE:.*]] = bitcast i16 %[[FIELD_RAW]] to <16 x i1>
// LLVM:         %[[FIELD_REDUCED:.*]] = call i1 @llvm.vector.reduce.or.v16i1(<16 x i1> %[[FIELD_VALUE]])
// LLVM:         %[[FIELD_REDUCED_BYTE:.*]] = zext i1 %[[FIELD_REDUCED]] to i8
// LLVM:         store i8 %[[FIELD_REDUCED_BYTE]], ptr %[[FIELD_RET_PTR:.*]], align 1
// LLVM:         %[[FIELD_RET_BYTE:.*]] = load i8, ptr %[[FIELD_RET_PTR]], align 1
// LLVM:         %[[FIELD_RET:.*]] = trunc i8 %[[FIELD_RET_BYTE]] to i1
// LLVM:         ret i1 %[[FIELD_RET]]
