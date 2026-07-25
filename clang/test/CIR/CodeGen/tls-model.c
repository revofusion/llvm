// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o - | FileCheck %s --check-prefix=OGCG

__thread int tls_global_dynamic
    __attribute__((tls_model("global-dynamic")));
__thread int tls_local_dynamic
    __attribute__((tls_model("local-dynamic")));
__thread int tls_initial_exec
    __attribute__((tls_model("initial-exec")));
__thread int tls_local_exec
    __attribute__((tls_model("local-exec")));

// CIR-DAG: cir.global external tls_dyn @tls_global_dynamic = #cir.int<0> : !s32i
// CIR-DAG: cir.global external tls_local_dyn @tls_local_dynamic = #cir.int<0> : !s32i
// CIR-DAG: cir.global external tls_init_exec @tls_initial_exec = #cir.int<0> : !s32i
// CIR-DAG: cir.global external tls_local_exec @tls_local_exec = #cir.int<0> : !s32i

// OGCG-DAG: @tls_global_dynamic = thread_local global i32 0
// OGCG-DAG: @tls_local_dynamic = thread_local(localdynamic) global i32 0
// OGCG-DAG: @tls_initial_exec = thread_local(initialexec) global i32 0
// OGCG-DAG: @tls_local_exec = thread_local(localexec) global i32 0
