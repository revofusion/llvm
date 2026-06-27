// RUN: %clang_cc1 -fopenacc -triple x86_64-linux-gnu -Wno-openacc-self-if-potential-conflict -emit-cir -fclangir -triple x86_64-linux-pc %s -o - | sed -E "s/ loc\([^)]*\)//g; s/ ,/,/g; s/ \)/)/g" > %t.cir
// RUN: FileCheck --input-file=%t.cir %s

struct DefaultOperators {
  int i;
  unsigned u;
  float f;
  double d;
  bool b;
};

struct DefaultOperatorsNoFloats {
  int i;
  unsigned int u;
  bool b;
};

template<typename T>
void acc_combined() {
  T someVar;
  T someVarArr[5];
  struct DefaultOperatorsNoFloats someVarNoFloats;
  struct DefaultOperatorsNoFloats someVarArrNoFloats[5];
#pragma acc parallel loop reduction(+:someVar)
// CHECK: acc.reduction.recipe @reduction_add__ZTS16DefaultOperators : !cir.ptr<!rec_DefaultOperators> reduction_operator <add> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_DefaultOperators, !cir.ptr<!rec_DefaultOperators>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !rec_DefaultOperators
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !rec_DefaultOperators, !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>, %{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !cir.float
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:     %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:     cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(*:someVar)
// CHECK: acc.reduction.recipe @reduction_mul__ZTS16DefaultOperators : !cir.ptr<!rec_DefaultOperators> reduction_operator <mul> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_DefaultOperators, !cir.ptr<!rec_DefaultOperators>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>, %{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !cir.float
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:     %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:     cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(max:someVar)
// CHECK: acc.reduction.recipe @reduction_max__ZTS16DefaultOperators : !cir.ptr<!rec_DefaultOperators> reduction_operator <max> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_DefaultOperators, !cir.ptr<!rec_DefaultOperators>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>, %{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       cir.yield %{{.*}} : !s32i
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       cir.yield %{{.*}} : !s32i
// CHECK-NEXT:     }) : (!cir.bool) -> !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u32i, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       cir.yield %{{.*}} : !u32i
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       cir.yield %{{.*}} : !u32i
// CHECK-NEXT:     }) : (!cir.bool) -> !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.float, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.float
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.double, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(min:someVar)
// CHECK: acc.reduction.recipe @reduction_min__ZTS16DefaultOperators : !cir.ptr<!rec_DefaultOperators> reduction_operator <min> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_DefaultOperators, !cir.ptr<!rec_DefaultOperators>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>, %{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       cir.yield %{{.*}} : !s32i
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       cir.yield %{{.*}} : !s32i
// CHECK-NEXT:     }) : (!cir.bool) -> !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u32i, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       cir.yield %{{.*}} : !u32i
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       cir.yield %{{.*}} : !u32i
// CHECK-NEXT:     }) : (!cir.bool) -> !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.float, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.float
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.double, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(&:someVarNoFloats)
// CHECK: acc.reduction.recipe @reduction_iand__ZTS24DefaultOperatorsNoFloats : !cir.ptr<!rec_DefaultOperatorsNoFloats> reduction_operator <iand> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperatorsNoFloats>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_DefaultOperatorsNoFloats, !cir.ptr<!rec_DefaultOperatorsNoFloats>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperatorsNoFloats>, %{{.*}}: !cir.ptr<!rec_DefaultOperatorsNoFloats>):
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:     %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:     cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(|:someVarNoFloats)
// CHECK: acc.reduction.recipe @reduction_ior__ZTS24DefaultOperatorsNoFloats : !cir.ptr<!rec_DefaultOperatorsNoFloats> reduction_operator <ior> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperatorsNoFloats>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_DefaultOperatorsNoFloats, !cir.ptr<!rec_DefaultOperatorsNoFloats>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !rec_DefaultOperatorsNoFloats
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !rec_DefaultOperatorsNoFloats, !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperatorsNoFloats>, %{{.*}}: !cir.ptr<!rec_DefaultOperatorsNoFloats>):
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:     %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:     cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(^:someVarNoFloats)
// CHECK: acc.reduction.recipe @reduction_xor__ZTS24DefaultOperatorsNoFloats : !cir.ptr<!rec_DefaultOperatorsNoFloats> reduction_operator <xor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperatorsNoFloats>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_DefaultOperatorsNoFloats, !cir.ptr<!rec_DefaultOperatorsNoFloats>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !rec_DefaultOperatorsNoFloats
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !rec_DefaultOperatorsNoFloats, !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperatorsNoFloats>, %{{.*}}: !cir.ptr<!rec_DefaultOperatorsNoFloats>):
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:     %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:     cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(&&:someVar)
// CHECK: acc.reduction.recipe @reduction_land__ZTS16DefaultOperators : !cir.ptr<!rec_DefaultOperators> reduction_operator <land> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_DefaultOperators, !cir.ptr<!rec_DefaultOperators>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>, %{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.const #false
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.const #false
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.const #false
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.float
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.const #false
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.const #false
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(||:someVar)
// CHECK: acc.reduction.recipe @reduction_lor__ZTS16DefaultOperators : !cir.ptr<!rec_DefaultOperators> reduction_operator <lor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_DefaultOperators, !cir.ptr<!rec_DefaultOperators>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !rec_DefaultOperators
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !rec_DefaultOperators, !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_DefaultOperators>, %{{.*}}: !cir.ptr<!rec_DefaultOperators>):
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:     %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.const #true
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.const #true
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:     %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.const #true
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.float
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:     %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.const #true
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       %{{.*}} = cir.const #true
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:     cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);

#pragma acc parallel loop reduction(+:someVarArr)
// CHECK: acc.reduction.recipe @reduction_add__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <add> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !cir.array<!rec_DefaultOperators x 5>
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:     cir.for : cond {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     } body {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !cir.float
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !cir.double
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:       cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(*:someVarArr)
// CHECK: acc.reduction.recipe @reduction_mul__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <mul> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:     cir.for : cond {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     } body {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !cir.float
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !cir.double
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:       cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(max:someVarArr)

// CHECK: acc.reduction.recipe @reduction_max__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <max> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:     cir.for : cond {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     } body {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         cir.yield %{{.*}} : !s32i
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         cir.yield %{{.*}} : !s32i
// CHECK-NEXT:       }) : (!cir.bool) -> !s32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u32i, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         cir.yield %{{.*}} : !u32i
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         cir.yield %{{.*}} : !u32i
// CHECK-NEXT:       }) : (!cir.bool) -> !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.float, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.float
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.double, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.double
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(min:someVarArr)
// CHECK: acc.reduction.recipe @reduction_min__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <min> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:     cir.for : cond {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     } body {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         cir.yield %{{.*}} : !s32i
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         cir.yield %{{.*}} : !s32i
// CHECK-NEXT:       }) : (!cir.bool) -> !s32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u32i, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         cir.yield %{{.*}} : !u32i
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         cir.yield %{{.*}} : !u32i
// CHECK-NEXT:       }) : (!cir.bool) -> !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.float, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.float
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.double, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.double
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(&:someVarArrNoFloats)
// CHECK: acc.reduction.recipe @reduction_iand__ZTSA5_24DefaultOperatorsNoFloats : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> reduction_operator <iand> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperatorsNoFloats x 5>, !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:     cir.for : cond {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     } body {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:       cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(|:someVarArrNoFloats)
// CHECK: acc.reduction.recipe @reduction_ior__ZTSA5_24DefaultOperatorsNoFloats : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> reduction_operator <ior> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperatorsNoFloats x 5>, !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !cir.array<!rec_DefaultOperatorsNoFloats x 5>
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.array<!rec_DefaultOperatorsNoFloats x 5>, !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:     cir.for : cond {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     } body {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:       cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(^:someVarArrNoFloats)
// CHECK: acc.reduction.recipe @reduction_xor__ZTSA5_24DefaultOperatorsNoFloats : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> reduction_operator <xor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperatorsNoFloats x 5>, !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !cir.array<!rec_DefaultOperatorsNoFloats x 5>
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.array<!rec_DefaultOperatorsNoFloats x 5>, !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:     cir.for : cond {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     } body {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !s64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:       cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(&&:someVarArr)
// CHECK: acc.reduction.recipe @reduction_land__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <land> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:     cir.for : cond {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     } body {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.float
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.double
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(||:someVarArr)
// CHECK: acc.reduction.recipe @reduction_lor__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <lor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !cir.array<!rec_DefaultOperators x 5>
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:     cir.for : cond {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     } body {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !s64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.const #true
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.const #true
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:       %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.const #true
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.float
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:       %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.const #true
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.double
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         %{{.*}} = cir.const #true
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:       cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);

#pragma acc parallel loop reduction(+:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_add__Bcnt1__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <add> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !cir.float
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:         cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(*:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_mul__Bcnt1__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <mul> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.const #true
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !cir.float
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) nsw : !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:         cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(max:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_max__Bcnt1__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <max> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:           cir.yield %{{.*}} : !s32i
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:           cir.yield %{{.*}} : !s32i
// CHECK-NEXT:         }) : (!cir.bool) -> !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u32i, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:           cir.yield %{{.*}} : !u32i
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:           cir.yield %{{.*}} : !u32i
// CHECK-NEXT:         }) : (!cir.bool) -> !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.float, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.float
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.double, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(min:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_min__Bcnt1__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <min> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.const #true
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:           cir.yield %{{.*}} : !s32i
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:           cir.yield %{{.*}} : !s32i
// CHECK-NEXT:         }) : (!cir.bool) -> !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u32i, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:           cir.yield %{{.*}} : !u32i
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:           cir.yield %{{.*}} : !u32i
// CHECK-NEXT:         }) : (!cir.bool) -> !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.float, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.float
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.float
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !cir.double, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.double
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(&:someVarArrNoFloats[2])
// CHECK: acc.reduction.recipe @reduction_iand__Bcnt1__ZTSA5_24DefaultOperatorsNoFloats : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> reduction_operator <iand> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperatorsNoFloats x 5>, !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !u64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.const #true
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !u64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !u64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:         cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(|:someVarArrNoFloats[2])
// CHECK: acc.reduction.recipe @reduction_ior__Bcnt1__ZTSA5_24DefaultOperatorsNoFloats : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> reduction_operator <ior> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperatorsNoFloats x 5>, !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !u64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !u64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !u64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:         cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(^:someVarArrNoFloats[2])
// CHECK: acc.reduction.recipe @reduction_xor__Bcnt1__ZTSA5_24DefaultOperatorsNoFloats : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> reduction_operator <xor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperatorsNoFloats x 5>, !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !u64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !u64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>> -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperatorsNoFloats>, !u64i) -> !cir.ptr<!rec_DefaultOperatorsNoFloats>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "b"} : !cir.ptr<!rec_DefaultOperatorsNoFloats> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:         cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperatorsNoFloats x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(&&:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_land__Bcnt1__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <land> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.const #true
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:           %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.const #false
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:           %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.const #false
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:           %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.const #false
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.float
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:           %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.const #false
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.const #false
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(||:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_lor__Bcnt1__ZTSA5_16DefaultOperators : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> reduction_operator <lor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_DefaultOperators x 5>, !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.const #false
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>> -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_DefaultOperators>, !u64i) -> !cir.ptr<!rec_DefaultOperators>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.const #true
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK-NEXT:           %{{.*}} = cir.cast int_to_bool %{{.*}} : !s32i -> !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.const #true
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:           %{{.*}} = cir.cast int_to_bool %{{.*}} : !u32i -> !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:         %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.const #true
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK-NEXT:           %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.float -> !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.float
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[3] {name = "d"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:         %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.const #true
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK-NEXT:           %{{.*}} = cir.cast float_to_bool %{{.*}} : !cir.double -> !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_float %{{.*}} : !cir.bool -> !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "b"} : !cir.ptr<!rec_DefaultOperators> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:         %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           %{{.*}} = cir.const #true
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           %{{.*}} = cir.load align(1) %{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.bool
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.bool
// CHECK-NEXT:         cir.store align(1) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_DefaultOperators x 5>>
// CHECK-NEXT:   }
  for(int i = 0; i < 5; ++i);

#pragma acc parallel loop reduction(+:someVarArr[1:1])
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(*:someVarArr[1:1])
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(max:someVarArr[1:1])
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(min:someVarArr[1:1])
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(&:someVarArrNoFloats[1:1])
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(|:someVarArrNoFloats[1:1])
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(^:someVarArrNoFloats[1:1])
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(&&:someVarArr[1:1])
  for(int i = 0; i < 5; ++i);
#pragma acc parallel loop reduction(||:someVarArr[1:1])
  for(int i = 0; i < 5; ++i);
  // CHECK-NEXT: cir.func {{.*}}@_Z12acc_combined
}

void uses() {
  acc_combined<DefaultOperators>();
}
