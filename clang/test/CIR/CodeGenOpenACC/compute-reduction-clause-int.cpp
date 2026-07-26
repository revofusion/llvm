// RUN: %clang_cc1 -fopenacc -triple x86_64-linux-gnu -Wno-openacc-self-if-potential-conflict -emit-cir -fclangir -triple x86_64-linux-pc %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

template<typename T>
void acc_compute() {
  T someVar;
  T someVarArr[5];
#pragma acc parallel reduction(+:someVar)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!s32i> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<0>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i> {{.*}}, {{.*}}: !cir.ptr<!s32i> {{.*}})
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.add nsw {{.*}}, {{.*}}
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield {{.*}} : !cir.ptr<!s32i>
// CHECK: }
  ;
#pragma acc parallel reduction(*:someVar)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!s32i> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<1>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i> {{.*}}, {{.*}}: !cir.ptr<!s32i> {{.*}})
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.mul nsw {{.*}}, {{.*}}
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield {{.*}} : !cir.ptr<!s32i>
// CHECK: }
  ;
#pragma acc parallel reduction(max:someVar)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!s32i> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<-2147483648>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i> {{.*}}, {{.*}}: !cir.ptr<!s32i> {{.*}})
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: cir.{{.*}}
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }, false {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }) : (!cir.bool) -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: acc.yield {{.*}} : !cir.ptr<!s32i>
// CHECK: }
  ;
#pragma acc parallel reduction(min:someVar)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!s32i> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<2147483647>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i> {{.*}}, {{.*}}: !cir.ptr<!s32i> {{.*}})
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: cir.{{.*}}
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }, false {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }) : (!cir.bool) -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: acc.yield {{.*}} : !cir.ptr<!s32i>
// CHECK: }
  ;
#pragma acc parallel reduction(&:someVar)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!s32i> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<-1>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i> {{.*}}, {{.*}}: !cir.ptr<!s32i> {{.*}})
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.and {{.*}}, {{.*}}
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield {{.*}} : !cir.ptr<!s32i>
// CHECK: }
  ;
  ;
#pragma acc parallel reduction(^:someVar)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!s32i> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<0>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i> {{.*}}, {{.*}}: !cir.ptr<!s32i> {{.*}})
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.xor {{.*}}, {{.*}}
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield {{.*}} : !cir.ptr<!s32i>
// CHECK: }

  ;
#pragma acc parallel reduction(&&:someVar)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!s32i> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<1>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i> {{.*}}, {{.*}}: !cir.ptr<!s32i> {{.*}})
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }, false {
// CHECK: {{.*}} = cir.const #false
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }) : (!cir.bool) -> !cir.bool
// CHECK: {{.*}} = cir.cast bool_to_int {{.*}} : !cir.bool -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}} {{.*}}
// CHECK: acc.yield {{.*}} : !cir.ptr<!s32i>
// CHECK: }
  ;
#pragma acc parallel reduction(||:someVar)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!s32i> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<0>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!s32i> {{.*}}, {{.*}}: !cir.ptr<!s32i> {{.*}})
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} = cir.const #true
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }, false {
// CHECK: {{.*}} : !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }) : (!cir.bool) -> !cir.bool
// CHECK: {{.*}} = cir.cast bool_to_int {{.*}} : !cir.bool -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: acc.yield {{.*}} : !cir.ptr<!s32i>
// CHECK: }
  ;

#pragma acc parallel reduction(+:someVarArr)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: {{.*}} = cir.const #cir.zero : !cir.array<!s32i x 5>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}} : !cir.array<!s32i x 5>, !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}})
// CHECK: {{.*}} = cir.const #cir.int<0> : !s64i
// CHECK: {{.*}} = cir.alloca "itr" align(8) : !cir.ptr<!s64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.const #cir.int<5> : !s64i
// CHECK: cir.{{.*}}
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.add nsw {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.inc {{.*}} : !s64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(*:someVarArr)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: {{.*}} = cir.const #cir.const_array<[#cir.int<1>, #cir.int<1>, #cir.int<1>, #cir.int<1>, #cir.int<1>]> : !cir.array<!s32i x 5>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}} : !cir.array<!s32i x 5>, !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}})
// CHECK: {{.*}} = cir.const #cir.int<0> : !s64i
// CHECK: {{.*}} = cir.alloca "itr" align(8) : !cir.ptr<!s64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.const #cir.int<5> : !s64i
// CHECK: cir.{{.*}}
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.mul nsw {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.inc {{.*}} : !s64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(max:someVarArr)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: {{.*}} = cir.const #cir.const_array<[#cir.int<-2147483648>, #cir.int<-2147483648>, #cir.int<-2147483648>, #cir.int<-2147483648>, #cir.int<-2147483648>]> : !cir.array<!s32i x 5>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}} : !cir.array<!s32i x 5>, !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}})
// CHECK: {{.*}} = cir.const #cir.int<0> : !s64i
// CHECK: {{.*}} = cir.alloca "itr" {{.*}} : !cir.ptr<!s64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.const #cir.int<5> : !s64i
// CHECK: {{.*}} : !s64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}}
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }, false {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }) : (!cir.bool) -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.inc {{.*}} : !s64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(min:someVarArr)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: {{.*}} = cir.const #cir.const_array<[#cir.int<2147483647>, #cir.int<2147483647>, #cir.int<2147483647>, #cir.int<2147483647>, #cir.int<2147483647>]> : !cir.array<!s32i x 5>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}} : !cir.array<!s32i x 5>, !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}})
// CHECK: {{.*}} = cir.const #cir.int<0> : !s64i
// CHECK: {{.*}} = cir.alloca "itr" {{.*}} : !cir.ptr<!s64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.const #cir.int<5> : !s64i
// CHECK: {{.*}} : !s64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}}
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }, false {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }) : (!cir.bool) -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.inc {{.*}} : !s64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(&:someVarArr)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: {{.*}} = cir.const #cir.const_array<[#cir.int<-1>, #cir.int<-1>, #cir.int<-1>, #cir.int<-1>, #cir.int<-1>]> : !cir.array<!s32i x 5>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}} : !cir.array<!s32i x 5>, !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}})
// CHECK: {{.*}} = cir.const #cir.int<0> : !s64i
// CHECK: {{.*}} = cir.alloca "itr" align(8) : !cir.ptr<!s64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.const #cir.int<5> : !s64i
// CHECK: cir.{{.*}}
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.and {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.inc {{.*}} : !s64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(|:someVarArr)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: {{.*}} = cir.const #cir.zero : !cir.array<!s32i x 5>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}} : !cir.array<!s32i x 5>, !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}})
// CHECK: {{.*}} = cir.const #cir.int<0> : !s64i
// CHECK: {{.*}} = cir.alloca "itr" align(8) : !cir.ptr<!s64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.const #cir.int<5> : !s64i
// CHECK: cir.{{.*}}
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.or {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.inc {{.*}} : !s64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(^:someVarArr)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: {{.*}} = cir.const #cir.zero : !cir.array<!s32i x 5>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}} : !cir.array<!s32i x 5>, !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}})
// CHECK: {{.*}} = cir.const #cir.int<0> : !s64i
// CHECK: {{.*}} = cir.alloca "itr" align(8) : !cir.ptr<!s64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.const #cir.int<5> : !s64i
// CHECK: cir.{{.*}}
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.xor {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.inc {{.*}} : !s64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(&&:someVarArr)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: {{.*}} = cir.const #cir.const_array<[#cir.int<1>, #cir.int<1>, #cir.int<1>, #cir.int<1>, #cir.int<1>]> : !cir.array<!s32i x 5>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}} : !cir.array<!s32i x 5>, !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}})
// CHECK: {{.*}} = cir.const #cir.int<0> : !s64i
// CHECK: {{.*}} = cir.alloca "itr" align(8) : !cir.ptr<!s64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.const #cir.int<5> : !s64i
// CHECK: cir.{{.*}}
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
//
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }, false {
// CHECK: {{.*}} = cir.const #false
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }) : (!cir.bool) -> !cir.bool
// CHECK: {{.*}} = cir.cast bool_to_int {{.*}} : !cir.bool -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
//
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.inc {{.*}} : !s64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(||:someVarArr)
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}})
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} init : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: {{.*}} = cir.const #cir.zero : !cir.array<!s32i x 5>
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}} : !cir.array<!s32i x 5>, !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: acc.yield
//
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}})
// CHECK: {{.*}} = cir.const #cir.int<0> : !s64i
// CHECK: {{.*}} = cir.alloca "itr" align(8) : !cir.ptr<!s64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.const #cir.int<5> : !s64i
// CHECK: cir.{{.*}}
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !s64i) -> !cir.ptr<!s32i>
//
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} = cir.const #true
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }, false {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }) : (!cir.bool) -> !cir.bool
// CHECK: {{.*}} = cir.cast bool_to_int {{.*}} : !cir.bool -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
//
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK: {{.*}} = cir.inc {{.*}} : !s64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;

#pragma acc parallel reduction(+:someVarArr[2])
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<0>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.add nsw {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(*:someVarArr[2])
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<1>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.mul nsw {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(max:someVarArr[2])
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<-2147483648>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" {{.*}} : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: cir.{{.*}}
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}}
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }, false {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }) : (!cir.bool) -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(min:someVarArr[2])
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<2147483647>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" {{.*}} : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: cir.{{.*}}
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}}
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }, false {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.{{.*}} {{.*}}
// CHECK: }) : (!cir.bool) -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(&:someVarArr[2])
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<-1>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.and {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(|:someVarArr[2])
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<0>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.or {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(^:someVarArr[2])
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<0>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.xor {{.*}}, {{.*}}
// CHECK: cir.{{.*}} {{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(&&:someVarArr[2])
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<1>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
//
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }, false {
// CHECK: {{.*}} = cir.const #false
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }) : (!cir.bool) -> !cir.bool
// CHECK: {{.*}} = cir.cast bool_to_int {{.*}} : !cir.bool -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
//
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
  ;
#pragma acc parallel reduction(||:someVarArr[2])
// CHECK: acc.reduction.recipe {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> reduction_operator {{.*}} init {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>>{{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: {{.*}} = cir.alloca "openacc.reduction.init" {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.const #cir.int<0>
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield
// CHECK: } combiner {
// CHECK: ^bb0({{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !cir.ptr<!cir.array<!s32i x 5>> {{.*}}, {{.*}}: !acc.data_bounds_ty{{.*}}))
// CHECK: cir.scope {
// CHECK: {{.*}} = acc.get_lowerbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = acc.get_upperbound {{.*}} : (!acc.data_bounds_ty) -> index
// CHECK: {{.*}} = builtin.unrealized_conversion_cast {{.*}} : index to !u64i
// CHECK: {{.*}} = cir.alloca "iter" align(8) : !cir.ptr<!u64i>
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.for : cond {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cmp lt {{.*}}, {{.*}} : !u64i
// CHECK: cir.{{.*}}({{.*}})
// CHECK: } body {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.cast array_to_ptrdecay {{.*}} : !cir.ptr<!cir.array<!s32i x 5>> -> !cir.ptr<!s32i>
// CHECK: {{.*}} = cir.ptr_stride {{.*}}, {{.*}} : (!cir.ptr<!s32i>, !u64i) -> !cir.ptr<!s32i>
//
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: {{.*}} = cir.ternary({{.*}}, true {
// CHECK: {{.*}} = cir.const #true
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }, false {
// CHECK: {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: {{.*}} = cir.cast int_to_bool {{.*}}  -> !cir.bool
// CHECK: cir.{{.*}} {{.*}} : !cir.bool
// CHECK: }) : (!cir.bool) -> !cir.bool
// CHECK: {{.*}} = cir.cast bool_to_int {{.*}} : !cir.bool -> !s32i
// CHECK: cir.{{.*}}{{.*}} {{.*}}, {{.*}}
//
// CHECK: cir.{{.*}}
// CHECK: } step {
// CHECK: {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK: {{.*}} = cir.inc {{.*}} : !u64i
// CHECK: cir.{{.*}} {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK: cir.{{.*}}
// CHECK: }
// CHECK: }
// CHECK: acc.yield {{.*}} : !cir.ptr<!cir.array<!s32i x 5>>
// CHECK: }
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
  // CHECK: cir.func {{.*}}@_Z11acc_compute
}

void uses() {
  acc_compute<int>();
}
