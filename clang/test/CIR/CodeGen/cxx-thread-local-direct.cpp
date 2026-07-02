// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -std=c++20 -fclangir -emit-cir -DDYNAMIC_TLS %s -o - | FileCheck --check-prefix=DYNAMIC %s

#ifdef DYNAMIC_TLS
// A TU that only sees an `extern` declaration (no visible initializer) can't
// prove the variable has constant initialization, so this must be accessed
// through the Itanium ABI's per-TU thread-local wrapper function rather than
// directly.
extern thread_local int dyn_tls;
int use_dynamic_tls() { return dyn_tls; }
// DYNAMIC: cir.func private dso_local @_ZTW7dyn_tls() -> !cir.ptr<!s32i>
// DYNAMIC-LABEL: cir.func{{.*}} @_Z15use_dynamic_tlsv
// DYNAMIC: cir.call @_ZTW7dyn_tls() : () -> !cir.ptr<!s32i>
#else
thread_local int *tls_ptr = nullptr;

bool has_tls_ptr() { return tls_ptr != nullptr; }
// CIR: cir.global{{.*}} tls_dyn @tls_ptr
// CIR-LABEL: cir.func{{.*}} @_Z11has_tls_ptrv
// CIR: cir.get_global thread_local @tls_ptr
#endif
