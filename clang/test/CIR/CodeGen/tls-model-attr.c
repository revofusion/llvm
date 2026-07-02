// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s

// An explicit tls_model attribute overrides the target's default TLS model.

__attribute__((tls_model("local-exec"))) extern __thread int model_le;
int use_le(void) { return model_le; }
// CHECK-DAG: cir.global "private" external tls_local_exec @model_le : !s32i

__attribute__((tls_model("initial-exec"))) extern __thread int model_ie;
int use_ie(void) { return model_ie; }
// CHECK-DAG: cir.global "private" external tls_init_exec @model_ie : !s32i

__attribute__((tls_model("local-dynamic"))) extern __thread int model_ld;
int use_ld(void) { return model_ld; }
// CHECK-DAG: cir.global "private" external tls_local_dyn @model_ld : !s32i

__attribute__((tls_model("global-dynamic"))) extern __thread int model_gd;
int use_gd(void) { return model_gd; }
// CHECK-DAG: cir.global "private" external tls_dyn @model_gd : !s32i
