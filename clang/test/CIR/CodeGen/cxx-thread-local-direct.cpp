// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: not %clang_cc1 -triple arm64-apple-macosx13.0.0 -std=c++20 -fclangir -emit-cir -DUNSUPPORTED_TLS %s -o /dev/null 2>&1 | FileCheck --check-prefix=NYI %s

#ifdef UNSUPPORTED_TLS
extern thread_local int dyn_tls;
int use_dynamic_tls() { return dyn_tls; }
// NYI: Not Yet Implemented: emitGlobalVarDeclLValue: dynamic thread_local wrapper
#else
thread_local int *tls_ptr = nullptr;

bool has_tls_ptr() { return tls_ptr != nullptr; }
// CIR: cir.global{{.*}} tls_dyn @tls_ptr
// CIR-LABEL: cir.func{{.*}} @_Z11has_tls_ptrv
// CIR: cir.get_global thread_local @tls_ptr
#endif
