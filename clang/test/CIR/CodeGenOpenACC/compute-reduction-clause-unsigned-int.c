// RUN: %clang_cc1 -fopenacc -triple x86_64-linux-gnu -Wno-openacc-self-if-potential-conflict -emit-cir -fclangir -triple x86_64-linux-pc %s -o - | sed -E "s/ loc\([^)]*\)//g; s/ ,/,/g; s/ \)/)/g" > %t.cir
// RUN: FileCheck --input-file=%t.cir %s

void acc_compute() {
  unsigned int someVar;
  unsigned int someVarArr[5];
#pragma acc parallel reduction(+:someVar)
// CHECK: acc.reduction.recipe @reduction_add__ZTSj : !cir.ptr<!u32i> reduction_operator <add> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>, %{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!u32i>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(*:someVar)
// CHECK: acc.reduction.recipe @reduction_mul__ZTSj : !cir.ptr<!u32i> reduction_operator <mul> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>, %{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!u32i>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(max:someVar)
// CHECK: acc.reduction.recipe @reduction_max__ZTSj : !cir.ptr<!u32i> reduction_operator <max> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>, %{{.*}}: !cir.ptr<!u32i>):
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
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!u32i>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(min:someVar)
// CHECK: acc.reduction.recipe @reduction_min__ZTSj : !cir.ptr<!u32i> reduction_operator <min> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>, %{{.*}}: !cir.ptr<!u32i>):
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
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!u32i>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(&:someVar)
// CHECK: acc.reduction.recipe @reduction_iand__ZTSj : !cir.ptr<!u32i> reduction_operator <iand> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>, %{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!u32i>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(|:someVar)
// CHECK: acc.reduction.recipe @reduction_ior__ZTSj : !cir.ptr<!u32i> reduction_operator <ior> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>, %{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!u32i>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(^:someVar)
// CHECK: acc.reduction.recipe @reduction_xor__ZTSj : !cir.ptr<!u32i> reduction_operator <xor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>, %{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:     %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!u32i>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(&&:someVar)
// CHECK: acc.reduction.recipe @reduction_land__ZTSj : !cir.ptr<!u32i> reduction_operator <land> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>, %{{.*}}: !cir.ptr<!u32i>):
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
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.cast integral %{{.*}} : !s32i -> !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!u32i>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(||:someVar)
// CHECK: acc.reduction.recipe @reduction_lor__ZTSj : !cir.ptr<!u32i> reduction_operator <lor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["openacc.reduction.init", init] {alignment = 4 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!u32i>, %{{.*}}: !cir.ptr<!u32i>):
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
// CHECK-NEXT:     %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:     %{{.*}} = cir.cast integral %{{.*}} : !s32i -> !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!u32i>
// CHECK-NEXT:   }
  ;

#pragma acc parallel reduction(+:someVarArr)
// CHECK: acc.reduction.recipe @reduction_add__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <add> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !cir.array<!u32i x 5>
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(*:someVarArr)
// CHECK: acc.reduction.recipe @reduction_mul__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <mul> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(max:someVarArr)
// CHECK: acc.reduction.recipe @reduction_max__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <max> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !cir.array<!u32i x 5>
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
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
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(min:someVarArr)
// CHECK: acc.reduction.recipe @reduction_min__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <min> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
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
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(&:someVarArr)
// CHECK: acc.reduction.recipe @reduction_iand__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <iand> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(|:someVarArr)
// CHECK: acc.reduction.recipe @reduction_ior__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <ior> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !cir.array<!u32i x 5>
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(^:someVarArr)
// CHECK: acc.reduction.recipe @reduction_xor__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <xor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !cir.array<!u32i x 5>
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:       %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(&&:someVarArr)
// CHECK: acc.reduction.recipe @reduction_land__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <land> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
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
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast integral %{{.*}} : !s32i -> !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(||:someVarArr)
// CHECK: acc.reduction.recipe @reduction_lor__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <lor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.const #cir.zero : !cir.array<!u32i x 5>
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !s64i) -> !cir.ptr<!u32i>
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
// CHECK-NEXT:       %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:       %{{.*}} = cir.cast integral %{{.*}} : !s32i -> !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;

#pragma acc parallel reduction(+:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_add__Bcnt1__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <add> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(add, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(*:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_mul__Bcnt1__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <mul> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(max:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_max__Bcnt1__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <max> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
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
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(min:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_min__Bcnt1__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <min> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
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
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(&:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_iand__Bcnt1__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <iand> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(and, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(|:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_ior__Bcnt1__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <ior> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(or, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(^:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_xor__Bcnt1__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <xor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.load align(4) %{{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK-NEXT:         %{{.*}} = cir.binop(xor, %{{.*}}, %{{.*}}) : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(&&:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_land__Bcnt1__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <land> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
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
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast integral %{{.*}} : !s32i -> !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;
#pragma acc parallel reduction(||:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_lor__Bcnt1__ZTSA5_j : !cir.ptr<!cir.array<!u32i x 5>> reduction_operator <lor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!u32i x 5>, !cir.ptr<!cir.array<!u32i x 5>>, ["openacc.reduction.init"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !cir.ptr<!cir.array<!u32i x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!u32i>, !u64i) -> !cir.ptr<!u32i>
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
// CHECK-NEXT:         %{{.*}} = cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
// CHECK-NEXT:         %{{.*}} = cir.cast integral %{{.*}} : !s32i -> !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!u32i x 5>>
// CHECK-NEXT:   }
  ;

#pragma acc parallel reduction(+:someVarArr[1:1])
  ;
#pragma acc parallel reduction(*:someVarArr[1:1])
  ;
#pragma acc parallel reduction(max:someVarArr[1:1])
  ;
#pragma acc parallel reduction(min:someVarArr[1:1])
  ;
#pragma acc parallel reduction(&:someVarArr[1:1])
  ;
#pragma acc parallel reduction(|:someVarArr[1:1])
  ;
#pragma acc parallel reduction(^:someVarArr[1:1])
  ;
#pragma acc parallel reduction(&&:someVarArr[1:1])
  ;
#pragma acc parallel reduction(||:someVarArr[1:1])
  ;
  // CHECK-NEXT: cir.func {{.*}}@acc_compute
}
