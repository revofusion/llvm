// Simple functions
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o -  | FileCheck %s

void empty() { }
// CHECK: cir.func{{.*}} @_Z5emptyv()
// CHECK:   cir.return
// CHECK: }

void voidret() { return; }
// CHECK: cir.func{{.*}} @_Z7voidretv()
// CHECK:   cir.return
// CHECK: }

int intfunc() { return 42; }
// CHECK: cir.func{{.*}} @_Z7intfuncv() -> !s32i
// CHECK:   {{.*}} = cir.alloca !s32i, !cir.ptr<!s32i>, ["__retval"] {alignment = 4 : i64}
// CHECK:   {{.*}} = cir.const #cir.int<42> : !s32i
// CHECK:   cir.store {{.*}}, {{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK:   {{.*}} = cir.load {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK:   cir.return {{.*}} : !s32i
// CHECK: }

int scopes() {
  {
    {
      return 99;
    }
  }
}
// CHECK: cir.func{{.*}} @_Z6scopesv() -> !s32i
// CHECK:   {{.*}} = cir.alloca !s32i, !cir.ptr<!s32i>, ["__retval"] {alignment = 4 : i64}
// CHECK:   cir.scope {
// CHECK:     cir.scope {
// CHECK:       {{.*}} = cir.const #cir.int<99> : !s32i
// CHECK:       cir.store {{.*}}, {{.*}} : !s32i, !cir.ptr<!s32i>
// CHECK:       {{.*}} = cir.load {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK:       cir.return {{.*}} : !s32i
// CHECK:     }
// CHECK:   }
// CHECK:   cir.trap
// CHECK: }

long longfunc() { return 42l; }
// CHECK: cir.func{{.*}} @_Z8longfuncv() -> !s64i
// CHECK:   {{.*}} = cir.alloca !s64i, !cir.ptr<!s64i>, ["__retval"] {alignment = 8 : i64}
// CHECK:   {{.*}} = cir.const #cir.int<42> : !s64i
// CHECK:   cir.store {{.*}}, {{.*}} : !s64i, !cir.ptr<!s64i>
// CHECK:   {{.*}} = cir.load {{.*}} : !cir.ptr<!s64i>, !s64i
// CHECK:   cir.return {{.*}} : !s64i
// CHECK: }

unsigned unsignedfunc() { return 42u; }
// CHECK: cir.func{{.*}} @_Z12unsignedfuncv() -> !u32i
// CHECK:   {{.*}} = cir.alloca !u32i, !cir.ptr<!u32i>, ["__retval"] {alignment = 4 : i64}
// CHECK:   {{.*}} = cir.const #cir.int<42> : !u32i
// CHECK:   cir.store {{.*}}, {{.*}} : !u32i, !cir.ptr<!u32i>
// CHECK:   {{.*}} = cir.load {{.*}} : !cir.ptr<!u32i>, !u32i
// CHECK:   cir.return {{.*}} : !u32i
// CHECK: }

unsigned long long ullfunc() { return 42ull; }
// CHECK: cir.func{{.*}} @_Z7ullfuncv() -> !u64i
// CHECK:   {{.*}} = cir.alloca !u64i, !cir.ptr<!u64i>, ["__retval"] {alignment = 8 : i64}
// CHECK:   {{.*}} = cir.const #cir.int<42> : !u64i
// CHECK:   cir.store {{.*}}, {{.*}} : !u64i, !cir.ptr<!u64i>
// CHECK:   {{.*}} = cir.load {{.*}} : !cir.ptr<!u64i>, !u64i
// CHECK:   cir.return {{.*}} : !u64i
// CHECK: }

bool boolfunc() { return true; }
// CHECK: cir.func{{.*}} @_Z8boolfuncv() -> !cir.bool
// CHECK:   {{.*}} = cir.alloca !cir.bool, !cir.ptr<!cir.bool>, ["__retval"] {alignment = 1 : i64}
// CHECK:   {{.*}} = cir.const #true
// CHECK:   cir.store {{.*}}, {{.*}} : !cir.bool, !cir.ptr<!cir.bool>
// CHECK:   {{.*}} = cir.load {{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CHECK:   cir.return {{.*}} : !cir.bool
// CHECK: }

float floatfunc() { return 42.42f; }
// CHECK: cir.func{{.*}} @_Z9floatfuncv() -> !cir.float
// CHECK:   {{.*}} = cir.alloca !cir.float, !cir.ptr<!cir.float>, ["__retval"] {alignment = 4 : i64}
// CHECK:   {{.*}} = cir.const #cir.fp<4.242
// CHECK:   cir.store {{.*}}, {{.*}} : !cir.float, !cir.ptr<!cir.float>
// CHECK:   {{.*}} = cir.load {{.*}} : !cir.ptr<!cir.float>, !cir.float
// CHECK:   cir.return {{.*}} : !cir.float
// CHECK: }

double doublefunc() { return 42.42; }
// CHECK: cir.func{{.*}} @_Z10doublefuncv() -> !cir.double
// CHECK:   {{.*}} = cir.alloca !cir.double, !cir.ptr<!cir.double>, ["__retval"] {alignment = 8 : i64}
// CHECK:   {{.*}} = cir.const #cir.fp<4.242
// CHECK:   cir.store {{.*}}, {{.*}} : !cir.double, !cir.ptr<!cir.double>
// CHECK:   {{.*}} = cir.load {{.*}} : !cir.ptr<!cir.double>, !cir.double
// CHECK:   cir.return {{.*}} : !cir.double
// CHECK: }
