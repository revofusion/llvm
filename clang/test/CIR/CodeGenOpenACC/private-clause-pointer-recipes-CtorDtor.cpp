// RUN: %clang_cc1 -fopenacc -triple x86_64-linux-gnu -Wno-openacc-self-if-potential-conflict -emit-cir -fclangir -triple x86_64-linux-pc %s -o - | sed -E "s/ loc\([^)]*\)//g; s/ ,/,/g; s/ \)/)/g" | FileCheck %s

struct CtorDtor {
  int i;
  CtorDtor();
  CtorDtor(const CtorDtor&);
  ~CtorDtor();
};

template<typename T>
void do_things(unsigned A, unsigned B) {

  T ***ThreePtr;
#pragma acc parallel private(ThreePtr)
// CHECK: acc.private.recipe @privatization__ZTSPPP8CtorDtor : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, ["openacc.private.init"] {alignment = 8 : i64}
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  ;
#pragma acc parallel private(ThreePtr[A])
// CHECK: acc.private.recipe @privatization__Bcnt1__ZTSPPP8CtorDtor : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, ["openacc.private.init"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<8> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  ;
#pragma acc parallel private(ThreePtr[B][B])
// CHECK: acc.private.recipe @privatization__Bcnt2__ZTSPPP8CtorDtor : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, ["openacc.private.init"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<8> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<8> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_CtorDtor>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  ;
#pragma acc parallel private(ThreePtr[B][A:B])
  ;
#pragma acc parallel private(ThreePtr[A:B][A:B])
  ;
#pragma acc parallel private(ThreePtr[B][B][B])
  ;
#pragma acc parallel private(ThreePtr[B][B][A:B])
// CHECK: acc.private.recipe @privatization__Bcnt3__ZTSPPP8CtorDtor : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, ["openacc.private.init"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<8> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<8> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_CtorDtor>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_CtorDtor, !cir.ptr<!rec_CtorDtor>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 4 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_CtorDtor>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.scope {
// CHECK-NEXT:           %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:           %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:           %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:           %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:           %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:           cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:           cir.for : cond {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:             cir.condition(%{{.*}})
// CHECK-NEXT:           } body {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:             cir.scope {
// CHECK-NEXT:               %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:               %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:               %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:               %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:               %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:               cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:               cir.for : cond {
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:                 %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:                 cir.condition(%{{.*}})
// CHECK-NEXT:               } body {
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:                 %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:                 cir.call @_ZN8CtorDtorC1Ev(%{{.*}}) : (!cir.ptr<!rec_CtorDtor>) -> ()
// CHECK-NEXT:                 cir.yield
// CHECK-NEXT:               } step {
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:                 %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:                 cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:                 cir.yield
// CHECK-NEXT:               }
// CHECK-NEXT:             }
// CHECK-NEXT:             cir.yield
// CHECK-NEXT:           } step {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:             cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:             cir.yield
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, %{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.scope {
// CHECK-NEXT:           %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:           %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:           %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:           %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:           %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:           %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:           %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:           cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:           cir.for : cond {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:             cir.condition(%{{.*}})
// CHECK-NEXT:           } body {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:             cir.scope {
// CHECK-NEXT:               %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:               %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:               %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:               %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:               %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:               %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:               %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:               cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:               cir.for : cond {
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:                 %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:                 cir.condition(%{{.*}})
// CHECK-NEXT:               } body {
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:                 %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:                 cir.call @_ZN8CtorDtorD1Ev(%{{.*}}) : (!cir.ptr<!rec_CtorDtor>) -> ()
// CHECK-NEXT:                 cir.yield
// CHECK-NEXT:               } step {
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:                 %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:                 cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:                 cir.yield
// CHECK-NEXT:               }
// CHECK-NEXT:             }
// CHECK-NEXT:             cir.yield
// CHECK-NEXT:           } step {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:             cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:             cir.yield
// CHECK-NEXT:           }
// CHECK-NEXT:         }
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
  ;
#pragma acc parallel private(ThreePtr[B][A:B][A:B])
  ;
#pragma acc parallel private(ThreePtr[A:B][A:B][A:B])
  ;

  T **TwoPtr;
#pragma acc parallel private(TwoPtr)
// CHECK: acc.private.recipe @privatization__ZTSPP8CtorDtor : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, ["openacc.private.init"] {alignment = 8 : i64}
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  ;
#pragma acc parallel private(TwoPtr[A])
// CHECK: acc.private.recipe @privatization__Bcnt1__ZTSPP8CtorDtor : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, ["openacc.private.init"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<8> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_CtorDtor>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  ;
#pragma acc parallel private(TwoPtr[B][B])
// CHECK: acc.private.recipe @privatization__Bcnt2__ZTSPP8CtorDtor : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, ["openacc.private.init"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<8> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_CtorDtor>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_CtorDtor, !cir.ptr<!rec_CtorDtor>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 4 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_CtorDtor>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         cir.scope {
// CHECK-NEXT:           %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:           %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:           %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:           %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:           %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:           cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:           cir.for : cond {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:             cir.condition(%{{.*}})
// CHECK-NEXT:           } body {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:             cir.call @_ZN8CtorDtorC1Ev(%{{.*}}) : (!cir.ptr<!rec_CtorDtor>) -> ()
// CHECK-NEXT:             cir.yield
// CHECK-NEXT:           } step {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:             cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:             cir.yield
// CHECK-NEXT:           }
// CHECK-NEXT:         }
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, %{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_CtorDtor>>>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         cir.scope {
// CHECK-NEXT:           %{{.*}} = acc.get_lowerbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:           %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:           %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:           %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:           %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["iter"] {alignment = 8 : i64}
// CHECK-NEXT:           %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:           %{{.*}} = cir.binop(sub, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:           cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:           cir.for : cond {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.cmp(ge, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:             cir.condition(%{{.*}})
// CHECK-NEXT:           } body {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:             cir.call @_ZN8CtorDtorD1Ev(%{{.*}}) : (!cir.ptr<!rec_CtorDtor>) -> ()
// CHECK-NEXT:             cir.yield
// CHECK-NEXT:           } step {
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:             %{{.*}} = cir.unary(dec, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:             cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:             cir.yield
// CHECK-NEXT:           }
// CHECK-NEXT:         }
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
  ;
#pragma acc parallel private(TwoPtr[B][A:B])
  ;
#pragma acc parallel private(TwoPtr[A:B][A:B])
  ;

  T *OnePtr;
#pragma acc parallel private(OnePtr)
// CHECK: acc.private.recipe @privatization__ZTSP8CtorDtor : !cir.ptr<!cir.ptr<!rec_CtorDtor>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!rec_CtorDtor>>):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_CtorDtor>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>, ["openacc.private.init"] {alignment = 8 : i64}
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   }
  ;
#pragma acc parallel private(OnePtr[B])
// CHECK: acc.private.recipe @privatization__Bcnt1__ZTSP8CtorDtor : !cir.ptr<!cir.ptr<!rec_CtorDtor>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!rec_CtorDtor>>, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_CtorDtor>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>, ["openacc.private.init"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<4> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_CtorDtor, !cir.ptr<!rec_CtorDtor>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 4 : i64}
// CHECK-NEXT:     cir.scope {
// CHECK-NEXT:       %{{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["itr"] {alignment = 8 : i64}
// CHECK-NEXT:       %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:       cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:       cir.for : cond {
// CHECK-NEXT:         %{{.*}} = cir.const #cir.int<1> : !u64i
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.cmp(lt, %{{.*}}, %{{.*}}) : !u64i, !cir.bool
// CHECK-NEXT:         cir.condition(%{{.*}})
// CHECK-NEXT:       } body {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_CtorDtor>, !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:         cir.call @_ZN8CtorDtorC1Ev(%{{.*}}) : (!cir.ptr<!rec_CtorDtor>) -> ()
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } destroy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!rec_CtorDtor>>, %{{.*}}: !cir.ptr<!cir.ptr<!rec_CtorDtor>>, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:         cir.call @_ZN8CtorDtorD1Ev(%{{.*}}) : (!cir.ptr<!rec_CtorDtor>) -> ()
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
  ;
#pragma acc parallel private(OnePtr[A:B])
  ;
}

void use(unsigned A, unsigned B) {
  do_things<CtorDtor>(A, B);
}

