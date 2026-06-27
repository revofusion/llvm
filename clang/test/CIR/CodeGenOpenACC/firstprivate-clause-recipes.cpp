// RUN: %clang_cc1 -fopenacc -triple x86_64-linux-gnu -Wno-openacc-self-if-potential-conflict -emit-cir -fclangir -triple x86_64-linux-pc %s -o - | sed -E "s/ loc\([^)]*\)//g; s/ ,/,/g; s/ \)/)/g" | FileCheck %s

// Note: unlike the 'private' recipe checks, this is just for spot-checking,
// so this test isn't as comprehensive.  The same code paths are used for
// 'private', so we just count on those to catch the errors.
struct NoOps {
  int i;
  ~NoOps();
};

struct CtorDtor {
  int i;
  CtorDtor();
  ~CtorDtor();
};

void do_things(unsigned A, unsigned B) {
  NoOps ThreeArr[5][5][5];

#pragma acc parallel firstprivate(ThreeArr[B][B][B])
// CHECK: acc.firstprivate.recipe @firstprivatization__Bcnt3__ZTSA5_A5_A5_5NoOps : !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>, !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>>, ["openacc.firstprivate.init"] {alignment = 4 : i64}
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } copy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>>, %{{.*}}: !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>> -> !cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>>, !u64i) -> !cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>>
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>> -> !cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>>, !u64i) -> !cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>>
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
// CHECK-NEXT:             %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>> -> !cir.ptr<!cir.array<!rec_NoOps x 5>>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!rec_NoOps x 5>>, !u64i) -> !cir.ptr<!cir.array<!rec_NoOps x 5>>
// CHECK-NEXT:             %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>> -> !cir.ptr<!cir.array<!rec_NoOps x 5>>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!rec_NoOps x 5>>, !u64i) -> !cir.ptr<!cir.array<!rec_NoOps x 5>>
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
// CHECK-NEXT:                 %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_NoOps x 5>> -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_NoOps>, !u64i) -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_NoOps x 5>> -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_NoOps>, !u64i) -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 cir.copy %{{.*}} to %{{.*}} : !cir.ptr<!rec_NoOps>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>>, %{{.*}}: !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.array<!cir.array<!rec_NoOps x 5> x 5> x 5>> -> !cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>>, !u64i) -> !cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>>
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
// CHECK-NEXT:             %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.array<!rec_NoOps x 5> x 5>> -> !cir.ptr<!cir.array<!rec_NoOps x 5>>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!rec_NoOps x 5>>, !u64i) -> !cir.ptr<!cir.array<!rec_NoOps x 5>>
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
// CHECK-NEXT:                 %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!rec_NoOps x 5>> -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_NoOps>, !u64i) -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 cir.call @_ZN5NoOpsD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_NoOps>) -> ()
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

  NoOps ***ThreePtr;
#pragma acc parallel firstprivate(ThreePtr[B][B][A:B])
// CHECK: acc.firstprivate.recipe @firstprivatization__Bcnt3__ZTSPPP5NoOps : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, ["openacc.firstprivate.init"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<8> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.ptr<!rec_NoOps>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>
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
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!rec_NoOps>, !cir.ptr<!cir.ptr<!rec_NoOps>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_NoOps>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_NoOps>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.ptr<!rec_NoOps>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>
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
// CHECK-NEXT:     %{{.*}} = cir.alloca !rec_NoOps, !cir.ptr<!rec_NoOps>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 4 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_NoOps>, !u64i) -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_NoOps>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_NoOps>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!rec_NoOps>, !cir.ptr<!cir.ptr<!rec_NoOps>>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       } step {
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK-NEXT:         %{{.*}} = cir.unary(inc, %{{.*}}) : !u64i, !u64i
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK-NEXT:         cir.yield
// CHECK-NEXT:       }
// CHECK-NEXT:     }
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } copy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, %{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>
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
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !cir.ptr<!cir.ptr<!rec_NoOps>>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_NoOps>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_NoOps>>
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !cir.ptr<!cir.ptr<!rec_NoOps>>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_NoOps>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_NoOps>>
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
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_NoOps>>, !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_NoOps>, !u64i) -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_NoOps>>, !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_NoOps>, !u64i) -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 cir.copy %{{.*}} to %{{.*}} : !cir.ptr<!rec_NoOps>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, %{{.*}}: !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>>, !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>
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
// CHECK-NEXT:             %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.ptr<!rec_NoOps>>>, !cir.ptr<!cir.ptr<!rec_NoOps>>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_NoOps>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_NoOps>>
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
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_NoOps>>, !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_NoOps>, !u64i) -> !cir.ptr<!rec_NoOps>
// CHECK-NEXT:                 cir.call @_ZN5NoOpsD1Ev(%{{.*}}) nothrow : (!cir.ptr<!rec_NoOps>) -> ()
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
  using PtrTArrayTy = CtorDtor*[5];
  PtrTArrayTy *PtrArrayPtr;

#pragma acc parallel firstprivate(PtrArrayPtr[B][B][B])
// CHECK: acc.firstprivate.recipe @firstprivatization__Bcnt3__ZTSPA5_P8CtorDtor : !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>> init {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>, !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, ["openacc.firstprivate.init"] {alignment = 8 : i64}
// CHECK-NEXT:     %{{.*}} = acc.get_upperbound %{{.*}} : (!acc.data_bounds_ty) -> index
// CHECK-NEXT:     %{{.*}} = builtin.unrealized_conversion_cast %{{.*}} : index to !u64i
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<40> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.binop(mul, %{{.*}}, %{{.*}}) : !u64i
// CHECK-NEXT:     %{{.*}} = cir.alloca !cir.array<!cir.ptr<!rec_CtorDtor> x 5>, !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>, %{{.*}} : !u64i, ["openacc.init.bounds"] {alignment = 8 : i64}
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
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>, !u64i) -> !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, !u64i) -> !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>
// CHECK-NEXT:         cir.store %{{.*}}, %{{.*}} : !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>, !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>
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
// CHECK-NEXT:     %{{.*}} = cir.const #cir.int<0> : !u64i
// CHECK-NEXT:     %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>> -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:     %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
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
// CHECK-NEXT:     acc.yield
// CHECK-NEXT:   } copy {
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, %{{.*}}: !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>, !u64i) -> !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>, !u64i) -> !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>
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
// CHECK-NEXT:             %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>> -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:             %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.ptr<!rec_CtorDtor>>, !u64i) -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
// CHECK-NEXT:             %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>> -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
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
// CHECK-NEXT:                 %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!rec_CtorDtor>>, !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:                 %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!rec_CtorDtor>, !u64i) -> !cir.ptr<!rec_CtorDtor>
// CHECK-NEXT:                 cir.copy %{{.*}} to %{{.*}} : !cir.ptr<!rec_CtorDtor>
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
// CHECK-NEXT:   ^bb0(%{{.*}}: !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, %{{.*}}: !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty, %{{.*}}: !acc.data_bounds_ty):
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
// CHECK-NEXT:         %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>>, !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>
// CHECK-NEXT:         %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>, !u64i) -> !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>>
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
// CHECK-NEXT:             %{{.*}} = cir.cast array_to_ptrdecay %{{.*}} : !cir.ptr<!cir.array<!cir.ptr<!rec_CtorDtor> x 5>> -> !cir.ptr<!cir.ptr<!rec_CtorDtor>>
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
}
