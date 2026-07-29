// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck %s --check-prefix=CIR --input-file=%t.cir
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck %s --check-prefix=LLVM --input-file=%t-cir.ll
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o %t.ll
// RUN: FileCheck %s --check-prefix=LLVM --input-file=%t.ll

unsigned char addcb(unsigned char x, unsigned char y, unsigned char carry_in,
                    unsigned char *carry_out) {
  return __builtin_addcb(x, y, carry_in, carry_out);
}

// CIR-LABEL: cir.func {{.*}} @addcb(
// CIR: %[[ADDCB1:.*]], %[[ADD_CB1:.*]] = cir.add.overflow %{{.*}}, %{{.*}} : !u8i -> !u8i
// CIR: %[[ADD_CB_RESULT:.*]], %[[ADD_CB2:.*]] = cir.add.overflow %[[ADDCB1]], %{{.*}} : !u8i -> !u8i
// CIR: %[[ADD_CB:.*]] = cir.or %[[ADD_CB1]], %[[ADD_CB2]] : !cir.bool
// CIR: %[[ADD_CB_EXT:.*]] = cir.cast bool_to_int %[[ADD_CB]] : !cir.bool -> !u8i
// CIR: cir.store {{.*}}%[[ADD_CB_EXT]], %{{.*}} : !u8i, !cir.ptr<!u8i>
// CIR: cir.store %[[ADD_CB_RESULT]], %{{.*}} : !u8i, !cir.ptr<!u8i>
// CIR: %[[ADD_CB_RET:.*]] = cir.load %{{.*}} : !cir.ptr<!u8i>, !u8i
// CIR: cir.return %[[ADD_CB_RET]] : !u8i

// LLVM-LABEL: define{{.*}} i8 @addcb(
// LLVM: %[[ADD_CB_PAIR1:.*]] = call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 %{{.*}}, i8 %{{.*}})
// LLVM: %[[ADD_CB_SUM1:.*]] = extractvalue { i8, i1 } %[[ADD_CB_PAIR1]], 0
// LLVM: %[[ADD_CB_PAIR2:.*]] = call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 %[[ADD_CB_SUM1]], i8 %{{.*}})
// LLVM: or i1 %{{.*}}, %{{.*}}
// LLVM: %[[ADD_CB_EXT:.*]] = zext i1 %{{.*}} to i8
// LLVM: store i8 %[[ADD_CB_EXT]], ptr %{{.*}}

unsigned short addcs(unsigned short x, unsigned short y,
                     unsigned short carry_in, unsigned short *carry_out) {
  return __builtin_addcs(x, y, carry_in, carry_out);
}

// CIR-LABEL: cir.func {{.*}} @addcs(
// CIR: %[[ADDCS1:.*]], %{{.*}} = cir.add.overflow %{{.*}}, %{{.*}} : !u16i -> !u16i
// CIR: %{{.*}}, %{{.*}} = cir.add.overflow %[[ADDCS1]], %{{.*}} : !u16i -> !u16i
// LLVM-LABEL: define{{.*}} i16 @addcs(
// LLVM: call { i16, i1 } @llvm.uadd.with.overflow.i16
// LLVM: call { i16, i1 } @llvm.uadd.with.overflow.i16

unsigned addc(unsigned x, unsigned y, unsigned carry_in,
              unsigned *carry_out) {
  return __builtin_addc(x, y, carry_in, carry_out);
}

// CIR-LABEL: cir.func {{.*}} @addc(
// CIR: %[[ADDC1:.*]], %{{.*}} = cir.add.overflow %{{.*}}, %{{.*}} : !u32i -> !u32i
// CIR: %{{.*}}, %{{.*}} = cir.add.overflow %[[ADDC1]], %{{.*}} : !u32i -> !u32i
// LLVM-LABEL: define{{.*}} i32 @addc(
// LLVM: call { i32, i1 } @llvm.uadd.with.overflow.i32
// LLVM: call { i32, i1 } @llvm.uadd.with.overflow.i32

unsigned long addcl(unsigned long x, unsigned long y, unsigned long carry_in,
                    unsigned long *carry_out) {
  return __builtin_addcl(x, y, carry_in, carry_out);
}

// CIR-LABEL: cir.func {{.*}} @addcl(
// CIR: %[[ADDCL1:.*]], %{{.*}} = cir.add.overflow %{{.*}}, %{{.*}} : !u64i -> !u64i
// CIR: %{{.*}}, %{{.*}} = cir.add.overflow %[[ADDCL1]], %{{.*}} : !u64i -> !u64i
// LLVM-LABEL: define{{.*}} i64 @addcl(
// LLVM: call { i64, i1 } @llvm.uadd.with.overflow.i64
// LLVM: call { i64, i1 } @llvm.uadd.with.overflow.i64

unsigned long long addcll(unsigned long long x, unsigned long long y,
                          unsigned long long carry_in,
                          unsigned long long *carry_out) {
  return __builtin_addcll(x, y, carry_in, carry_out);
}

// CIR-LABEL: cir.func {{.*}} @addcll(
// CIR: %[[ADDCLL1:.*]], %{{.*}} = cir.add.overflow %{{.*}}, %{{.*}} : !u64i -> !u64i
// CIR: %{{.*}}, %{{.*}} = cir.add.overflow %[[ADDCLL1]], %{{.*}} : !u64i -> !u64i
// LLVM-LABEL: define{{.*}} i64 @addcll(
// LLVM: call { i64, i1 } @llvm.uadd.with.overflow.i64
// LLVM: call { i64, i1 } @llvm.uadd.with.overflow.i64

unsigned char subcb(unsigned char x, unsigned char y, unsigned char borrow_in,
                    unsigned char *borrow_out) {
  return __builtin_subcb(x, y, borrow_in, borrow_out);
}

// CIR-LABEL: cir.func {{.*}} @subcb(
// CIR: %[[SUBCB1:.*]], %[[SUB_CB1:.*]] = cir.sub.overflow %{{.*}}, %{{.*}} : !u8i -> !u8i
// CIR: %[[SUB_CB_RESULT:.*]], %[[SUB_CB2:.*]] = cir.sub.overflow %[[SUBCB1]], %{{.*}} : !u8i -> !u8i
// CIR: %[[SUB_CB:.*]] = cir.or %[[SUB_CB1]], %[[SUB_CB2]] : !cir.bool
// CIR: %[[SUB_CB_EXT:.*]] = cir.cast bool_to_int %[[SUB_CB]] : !cir.bool -> !u8i
// CIR: cir.store {{.*}}%[[SUB_CB_EXT]], %{{.*}} : !u8i, !cir.ptr<!u8i>
// CIR: cir.store %[[SUB_CB_RESULT]], %{{.*}} : !u8i, !cir.ptr<!u8i>
// CIR: %[[SUB_CB_RET:.*]] = cir.load %{{.*}} : !cir.ptr<!u8i>, !u8i
// CIR: cir.return %[[SUB_CB_RET]] : !u8i

// LLVM-LABEL: define{{.*}} i8 @subcb(
// LLVM: %[[SUB_CB_PAIR1:.*]] = call { i8, i1 } @llvm.usub.with.overflow.i8(i8 %{{.*}}, i8 %{{.*}})
// LLVM: %[[SUB_CB_SUM1:.*]] = extractvalue { i8, i1 } %[[SUB_CB_PAIR1]], 0
// LLVM: %[[SUB_CB_PAIR2:.*]] = call { i8, i1 } @llvm.usub.with.overflow.i8(i8 %[[SUB_CB_SUM1]], i8 %{{.*}})
// LLVM: or i1 %{{.*}}, %{{.*}}
// LLVM: %[[SUB_CB_EXT:.*]] = zext i1 %{{.*}} to i8
// LLVM: store i8 %[[SUB_CB_EXT]], ptr %{{.*}}

unsigned short subcs(unsigned short x, unsigned short y,
                     unsigned short borrow_in, unsigned short *borrow_out) {
  return __builtin_subcs(x, y, borrow_in, borrow_out);
}

// CIR-LABEL: cir.func {{.*}} @subcs(
// CIR: %[[SUBCS1:.*]], %{{.*}} = cir.sub.overflow %{{.*}}, %{{.*}} : !u16i -> !u16i
// CIR: %{{.*}}, %{{.*}} = cir.sub.overflow %[[SUBCS1]], %{{.*}} : !u16i -> !u16i
// LLVM-LABEL: define{{.*}} i16 @subcs(
// LLVM: call { i16, i1 } @llvm.usub.with.overflow.i16
// LLVM: call { i16, i1 } @llvm.usub.with.overflow.i16

unsigned subc(unsigned x, unsigned y, unsigned borrow_in,
              unsigned *borrow_out) {
  return __builtin_subc(x, y, borrow_in, borrow_out);
}

// CIR-LABEL: cir.func {{.*}} @subc(
// CIR: %[[SUBC1:.*]], %{{.*}} = cir.sub.overflow %{{.*}}, %{{.*}} : !u32i -> !u32i
// CIR: %{{.*}}, %{{.*}} = cir.sub.overflow %[[SUBC1]], %{{.*}} : !u32i -> !u32i
// LLVM-LABEL: define{{.*}} i32 @subc(
// LLVM: call { i32, i1 } @llvm.usub.with.overflow.i32
// LLVM: call { i32, i1 } @llvm.usub.with.overflow.i32

unsigned long subcl(unsigned long x, unsigned long y, unsigned long borrow_in,
                    unsigned long *borrow_out) {
  return __builtin_subcl(x, y, borrow_in, borrow_out);
}

// CIR-LABEL: cir.func {{.*}} @subcl(
// CIR: %[[SUBCL1:.*]], %{{.*}} = cir.sub.overflow %{{.*}}, %{{.*}} : !u64i -> !u64i
// CIR: %{{.*}}, %{{.*}} = cir.sub.overflow %[[SUBCL1]], %{{.*}} : !u64i -> !u64i
// LLVM-LABEL: define{{.*}} i64 @subcl(
// LLVM: call { i64, i1 } @llvm.usub.with.overflow.i64
// LLVM: call { i64, i1 } @llvm.usub.with.overflow.i64

unsigned long long subcll(unsigned long long x, unsigned long long y,
                          unsigned long long borrow_in,
                          unsigned long long *borrow_out) {
  return __builtin_subcll(x, y, borrow_in, borrow_out);
}

// CIR-LABEL: cir.func {{.*}} @subcll(
// CIR: %[[SUBCLL1:.*]], %{{.*}} = cir.sub.overflow %{{.*}}, %{{.*}} : !u64i -> !u64i
// CIR: %{{.*}}, %{{.*}} = cir.sub.overflow %[[SUBCLL1]], %{{.*}} : !u64i -> !u64i
// LLVM-LABEL: define{{.*}} i64 @subcll(
// LLVM: call { i64, i1 } @llvm.usub.with.overflow.i64
// LLVM: call { i64, i1 } @llvm.usub.with.overflow.i64

unsigned next_value(void);
unsigned *next_output(void);

unsigned addc_once(void) {
  return __builtin_addc(next_value(), next_value(), next_value(),
                        next_output());
}

// CIR-LABEL: cir.func {{.*}} @addc_once(
// CIR-COUNT-3: cir.call @next_value
// CIR-COUNT-1: cir.call @next_output
// LLVM-LABEL: define{{.*}} i32 @addc_once(
// LLVM-COUNT-3: call i32 @next_value()
// LLVM-COUNT-1: call ptr @next_output()

unsigned char addcb_boundary(unsigned char *carry_out) {
  return __builtin_addcb(255, 0, 1, carry_out);
}

// CIR-LABEL: cir.func {{.*}} @addcb_boundary(
// CIR-DAG: %[[MAX:.*]] = cir.const #cir.int<255> : !u8i
// CIR-DAG: %[[ZERO:.*]] = cir.const #cir.int<0> : !u8i
// CIR-DAG: %[[ONE:.*]] = cir.const #cir.int<1> : !u8i
// CIR: %[[BOUND_ADD1:.*]], %[[BOUND_CARRY1:.*]] = cir.add.overflow %[[MAX]], %[[ZERO]] : !u8i -> !u8i
// CIR: %[[BOUND_RESULT:.*]], %[[BOUND_CARRY2:.*]] = cir.add.overflow %[[BOUND_ADD1]], %[[ONE]] : !u8i -> !u8i
// CIR: %[[BOUND_CARRY:.*]] = cir.or %[[BOUND_CARRY1]], %[[BOUND_CARRY2]] : !cir.bool
// CIR: cir.cast bool_to_int %[[BOUND_CARRY]] : !cir.bool -> !u8i
// CIR: cir.store %[[BOUND_RESULT]], %{{.*}} : !u8i, !cir.ptr<!u8i>
// LLVM-LABEL: define{{.*}} i8 @addcb_boundary(
// LLVM: call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 -1, i8 0)
// LLVM: call { i8, i1 } @llvm.uadd.with.overflow.i8(i8 %{{.*}}, i8 1)

unsigned long long subcll_boundary(unsigned long long *borrow_out) {
  return __builtin_subcll(0, 0, 1, borrow_out);
}

// CIR-LABEL: cir.func {{.*}} @subcll_boundary(
// CIR-DAG: %[[ZERO64:.*]] = cir.const #cir.int<0> : !u64i
// CIR-DAG: %[[ONE64:.*]] = cir.const #cir.int<1> : !u64i
// CIR: %[[BOUND_SUB1:.*]], %[[BOUND_BORROW1:.*]] = cir.sub.overflow %[[ZERO64]], %{{.*}} : !u64i -> !u64i
// CIR: %[[BOUND_SUB_RESULT:.*]], %[[BOUND_BORROW2:.*]] = cir.sub.overflow %[[BOUND_SUB1]], %[[ONE64]] : !u64i -> !u64i
// CIR: %[[BOUND_BORROW:.*]] = cir.or %[[BOUND_BORROW1]], %[[BOUND_BORROW2]] : !cir.bool
// CIR: cir.cast bool_to_int %[[BOUND_BORROW]] : !cir.bool -> !u64i
// CIR: cir.store %[[BOUND_SUB_RESULT]], %{{.*}} : !u64i, !cir.ptr<!u64i>
// LLVM-LABEL: define{{.*}} i64 @subcll_boundary(
// LLVM: call { i64, i1 } @llvm.usub.with.overflow.i64(i64 0, i64 0)
// LLVM: call { i64, i1 } @llvm.usub.with.overflow.i64(i64 %{{.*}}, i64 1)
