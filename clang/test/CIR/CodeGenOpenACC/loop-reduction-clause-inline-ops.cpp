// RUN: %clang_cc1 -fopenacc -triple x86_64-linux-gnu -Wno-openacc-self-if-potential-conflict -emit-cir -fclangir -triple x86_64-linux-pc %s -o - | sed -E "s/ loc\([^)]*\)//g; s/ ,/,/g; s/ \)/)/g" | FileCheck %s

struct HasOperatorsInline {
  int i;
  unsigned u;
  float f;
  double d;
  bool b;

  ~HasOperatorsInline();

  HasOperatorsInline &operator+=(HasOperatorsInline& other);
  HasOperatorsInline &operator*=(HasOperatorsInline& other);
  HasOperatorsInline &operator&=(HasOperatorsInline& other);
  HasOperatorsInline &operator|=(HasOperatorsInline& other);
  HasOperatorsInline &operator^=(HasOperatorsInline& other);
  HasOperatorsInline &operator&&(HasOperatorsInline& other);
  HasOperatorsInline &operator||(HasOperatorsInline& other);
  // For min/max
  bool operator<(HasOperatorsInline& other);
  HasOperatorsInline &operator=(HasOperatorsInline& other);
};

template<typename T>
void acc_loop() {
  T someVar;
  T someVarArr[5];
#pragma acc loop reduction(+:someVar)
// CHECK: acc.reduction.recipe @reduction_add__ZTS18HasOperatorsInline : !cir.ptr<!rec_HasOperatorsInline> reduction_operator <add> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_HasOperatorsInline, !cir.ptr<!rec_HasOperatorsInline>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlinepLERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(*:someVar)
// CHECK: acc.reduction.recipe @reduction_mul__ZTS18HasOperatorsInline : !cir.ptr<!rec_HasOperatorsInline> reduction_operator <mul> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_HasOperatorsInline, !cir.ptr<!rec_HasOperatorsInline>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlinemLERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(max:someVar)
// CHECK: acc.reduction.recipe @reduction_max__ZTS18HasOperatorsInline : !cir.ptr<!rec_HasOperatorsInline> reduction_operator <max> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_HasOperatorsInline, !cir.ptr<!rec_HasOperatorsInline>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineltERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(min:someVar)
// CHECK: acc.reduction.recipe @reduction_min__ZTS18HasOperatorsInline : !cir.ptr<!rec_HasOperatorsInline> reduction_operator <min> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_HasOperatorsInline, !cir.ptr<!rec_HasOperatorsInline>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineltERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.bool
// CHECK-NEXT:     %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     }, false {
// CHECK-NEXT:       cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     }) : (!cir.bool) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(&:someVar)
// CHECK: acc.reduction.recipe @reduction_iand__ZTS18HasOperatorsInline : !cir.ptr<!rec_HasOperatorsInline> reduction_operator <iand> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_HasOperatorsInline, !cir.ptr<!rec_HasOperatorsInline>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFF> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFFFFFFFFFF> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineaNERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(|:someVar)
// CHECK: acc.reduction.recipe @reduction_ior__ZTS18HasOperatorsInline : !cir.ptr<!rec_HasOperatorsInline> reduction_operator <ior> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_HasOperatorsInline, !cir.ptr<!rec_HasOperatorsInline>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineoRERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(^:someVar)
// CHECK: acc.reduction.recipe @reduction_xor__ZTS18HasOperatorsInline : !cir.ptr<!rec_HasOperatorsInline> reduction_operator <xor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_HasOperatorsInline, !cir.ptr<!rec_HasOperatorsInline>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineeOERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(&&:someVar)
// CHECK: acc.reduction.recipe @reduction_land__ZTS18HasOperatorsInline : !cir.ptr<!rec_HasOperatorsInline> reduction_operator <land> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_HasOperatorsInline, !cir.ptr<!rec_HasOperatorsInline>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineaaERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(||:someVar)
// CHECK: acc.reduction.recipe @reduction_lor__ZTS18HasOperatorsInline : !cir.ptr<!rec_HasOperatorsInline> reduction_operator <lor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_HasOperatorsInline, !cir.ptr<!rec_HasOperatorsInline>, ["openacc.reduction.init", init] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineooERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!rec_HasOperatorsInline>, %{{.*}}: !cir.ptr<!rec_HasOperatorsInline>):
// CHECK-NEXT:     cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);

#pragma acc loop reduction(+:someVarArr)
// CHECK: acc.reduction.recipe @reduction_add__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <add> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["arrayinit.temp"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:       cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:       cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.const #false
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlinepLERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["__array_idx"] {alignment = 1 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<-1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(*:someVarArr)
// CHECK: acc.reduction.recipe @reduction_mul__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <mul> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlinemLERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["__array_idx"] {alignment = 1 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<-1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(max:someVarArr)
// CHECK: acc.reduction.recipe @reduction_max__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <max> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #false
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineltERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["__array_idx"] {alignment = 1 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<-1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(min:someVarArr)
// CHECK: acc.reduction.recipe @reduction_min__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <min> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineltERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.bool
// CHECK-NEXT:       %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       }, false {
// CHECK-NEXT:         cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       }) : (!cir.bool) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["__array_idx"] {alignment = 1 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<-1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(&:someVarArr)
// CHECK: acc.reduction.recipe @reduction_iand__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <iand> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFF> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFFFFFFFFFF> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFF> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFFFFFFFFFF> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFF> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFFFFFFFFFF> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFF> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFFFFFFFFFF> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFF> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<0xFFFFFFFFFFFFFFFF> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineaNERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["__array_idx"] {alignment = 1 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<-1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(|:someVarArr)
// CHECK: acc.reduction.recipe @reduction_ior__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <ior> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["arrayinit.temp"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:       cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:       cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.const #false
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineoRERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["__array_idx"] {alignment = 1 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<-1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(^:someVarArr)
// CHECK: acc.reduction.recipe @reduction_xor__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <xor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["arrayinit.temp"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:       cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:       cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.const #false
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineeOERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["__array_idx"] {alignment = 1 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<-1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }

  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(&&:someVarArr)
// CHECK: acc.reduction.recipe @reduction_land__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <land> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<2> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<3> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:     cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:     cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:     %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:     %{{.*}} = cir.const #true
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineaaERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["__array_idx"] {alignment = 1 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<-1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(||:someVarArr)
// CHECK: acc.reduction.recipe @reduction_lor__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <lor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init", init] {alignment = 16 : i64}
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["arrayinit.temp"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     cir.store align(8) %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<5> : !s64i
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:       cir.store align(16) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:       cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:       cir.store align(16) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:       %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.const #false
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store align(8) %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load align(8) %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } combiner {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
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
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineooERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } step {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK-NEXT:       %{{.*}} = cir.unary(inc, %{{.*}}) : !s64i, !s64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>):
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, ["__array_idx"] {alignment = 1 : i64}
// CHECK-NEXT:     cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:     cir.do {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<-1> : !s64i
// CHECK-NEXT:       %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !s64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>
// CHECK-NEXT:       cir.yield
// CHECK-NEXT:     } while {
// CHECK-NEXT:       %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_HasOperatorsInline>>, !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:       %{{.*}} = cir.cmp(ne, %{{.*}}, %{{.*}}) : !cir.ptr<!rec_HasOperatorsInline>, !cir.bool
// CHECK-NEXT:       cir.condition(%{{.*}})
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);

#pragma acc loop reduction(+:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_add__Bcnt1__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <add> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlinepLERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:       %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(*:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_mul__Bcnt1__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <mul> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlinemLERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:       %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(max:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_max__Bcnt1__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <max> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<-2147483648> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<-3.40282347E+38> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<-1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineltERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:       %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(min:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_min__Bcnt1__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <min> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<2147483647> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<3.40282347E+38> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.7976931348623157E+308> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineltERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.bool
// CHECK-NEXT:         %{{.*}} = cir.ternary(%{{.*}}, true {
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         }, false {
// CHECK-NEXT:           cir.yield %{{.*}} : !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         }) : (!cir.bool) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:       %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(&:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_iand__Bcnt1__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <iand> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<-1> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<4294967295> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0xFFFFFFFF> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0xFFFFFFFFFFFFFFFF> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineaNERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:       %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(|:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_ior__Bcnt1__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <ior> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineoRERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:       %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(^:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_xor__Bcnt1__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <xor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineeOERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:       %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(&&:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_land__Bcnt1__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <land> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<1.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineaaERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:       %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(||:someVarArr[2])
// CHECK: acc.reduction.recipe @reduction_lor__Bcnt1__ZTSA5_18HasOperatorsInline : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> reduction_operator <lor> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!rec_HasOperatorsInline x 5>, !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, ["openacc.reduction.init"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[0] {name = "i"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !s32i
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[1] {name = "u"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<0> : !u32i
// CHECK-NEXT:         cir.store align(4) %{{.*}}, %{{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[2] {name = "f"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.float
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[4] {name = "d"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.const #cir.fp<0.000000e+00> : !cir.double
// CHECK-NEXT:         cir.store align(8) %{{.*}}, %{{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK-NEXT:         %{{.*}} = cir.get_member %{{.*}}[5] {name = "b"} : !cir.ptr<!rec_HasOperatorsInline> -> !cir.ptr<!cir.bool>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineooERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.call @_ZN18HasOperatorsInlineaSERS_(%{{.*}}, %{{.*}}) : (!cir.ptr<!rec_HasOperatorsInline>, !cir.ptr<!rec_HasOperatorsInline>) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:       %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:       %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_HasOperatorsInline x 5>> -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_HasOperatorsInline>, !u64i) -> !cir.ptr<!rec_HasOperatorsInline>
// CHECK-NEXT:         cir.call @_ZN18HasOperatorsInlineD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_HasOperatorsInline>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  for(int i=0;i < 5; ++i);

#pragma acc loop reduction(+:someVarArr[1:1])
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(*:someVarArr[1:1])
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(max:someVarArr[1:1])
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(min:someVarArr[1:1])
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(&:someVarArr[1:1])
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(|:someVarArr[1:1])
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(^:someVarArr[1:1])
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(&&:someVarArr[1:1])
  for(int i=0;i < 5; ++i);
#pragma acc loop reduction(||:someVarArr[1:1])
  for(int i=0;i < 5; ++i);
  // CHECK-NEXT: cir.func {{.*}}@_Z8acc_loop
}

void uses() {
  acc_loop<HasOperatorsInline>();
}
