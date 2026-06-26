// RUN: %clang_cc1 -triple arm64-apple-macosx15.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

int signbit_double(double x) { return __builtin_signbit(x); }
int signbit_float(float x) { return __builtin_signbitf(x); }
int signbit_long_double(long double x) { return __builtin_signbitl(x); }

// CHECK-LABEL: @_Z14signbit_doubled
// CHECK: cir.cast bitcast %{{.*}} : !cir.double -> !s64i
// CHECK: cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK: cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i

// CHECK-LABEL: @_Z13signbit_floatf
// CHECK: cir.cast bitcast %{{.*}} : !cir.float -> !s32i
// CHECK: cir.cmp(lt, %{{.*}}, %{{.*}}) : !s32i, !cir.bool
// CHECK: cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i

// CHECK-LABEL: @_Z19signbit_long_doublee
// CHECK: cir.cast bitcast %{{.*}} : !cir.long_double<!cir.double> -> !s64i
// CHECK: cir.cmp(lt, %{{.*}}, %{{.*}}) : !s64i, !cir.bool
// CHECK: cir.cast bool_to_int %{{.*}} : !cir.bool -> !s32i
