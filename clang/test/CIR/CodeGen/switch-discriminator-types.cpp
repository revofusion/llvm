// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

extern "C" void switch_bool(bool value) {
  switch (value) {
  case false:
    break;
  case true:
    break;
  }
}

// CHECK-LABEL: cir.func{{.*}} @switch_bool
// CHECK: cir.switch(%{{.*}} : !s32i) {
// CHECK: cir.case(equal, [#cir.int<0> : !s32i]) {
// CHECK: cir.case(equal, [#cir.int<1> : !s32i]) {

enum class BoolEnum : bool { False = false, True = true };

extern "C" void switch_bool_enum(BoolEnum value) {
  switch (value) {
  case BoolEnum::False:
    break;
  case BoolEnum::True:
    break;
  }
}

// A scoped enum is not subject to integral promotion.  Its bool CIR value must
// still be converted to an integer before constructing cir.switch and its case
// attributes must use that same integer type.
// CHECK-LABEL: cir.func{{.*}} @switch_bool_enum
// CHECK: %[[BOOL_VALUE:.*]] = cir.load{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK: %[[BOOL_DISCRIMINATOR:.*]] = cir.cast bool_to_int %[[BOOL_VALUE]] : !cir.bool -> !s32i
// CHECK: cir.switch(%[[BOOL_DISCRIMINATOR]] : !s32i) all_enum_cases_covered {
// CHECK: cir.case(equal, [#cir.int<0> : !s32i]) {
// CHECK: cir.case(equal, [#cir.int<1> : !s32i]) {

enum Ordinary : unsigned short { OrdinaryZero = 0, OrdinaryMax = 65535 };

extern "C" void switch_ordinary_enum(Ordinary value) {
  switch (value) {
  case OrdinaryZero:
    break;
  case static_cast<Ordinary>(65535):
    break;
  }
}

// The unscoped enum promotes from unsigned short to int.  In particular, the
// second case starts as an unsigned-short enum constant but must be extended
// and retagged to match the promoted switch discriminator.
// CHECK-LABEL: cir.func{{.*}} @switch_ordinary_enum
// CHECK: cir.switch(%{{.*}} : !s32i) all_enum_cases_covered {
// CHECK: cir.case(equal, [#cir.int<0> : !s32i]) {
// CHECK: cir.case(equal, [#cir.int<65535> : !s32i]) {

enum class ByteEnum : unsigned char { Zero = 0, High = 255 };

extern "C" void switch_scoped_byte_enum(ByteEnum value) {
  switch (value) {
  case ByteEnum::Zero:
    break;
  case ByteEnum::High:
    break;
  }
}

// Non-promoted scoped enums retain their integer underlying type, and every
// case attribute must be normalized to that discriminator width and sign.
// CHECK-LABEL: cir.func{{.*}} @switch_scoped_byte_enum
// CHECK: cir.switch(%{{.*}} : !u8i) all_enum_cases_covered {
// CHECK: cir.case(equal, [#cir.int<0> : !u8i]) {
// CHECK: cir.case(equal, [#cir.int<255> : !u8i]) {
