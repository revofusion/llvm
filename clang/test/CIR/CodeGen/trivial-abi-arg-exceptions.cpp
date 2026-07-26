// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -fcxx-exceptions -fexceptions %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm -fcxx-exceptions -fexceptions %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=LLVM

struct [[clang::trivial_abi]] CalleeDestroyed {
  int value;
  CalleeDestroyed(const CalleeDestroyed &);
  ~CalleeDestroyed();
};

int may_throw();
void consume(CalleeDestroyed, int) noexcept;

void destroy_unpassed_arg(CalleeDestroyed &value) {
  consume(value, may_throw());
}

// The caller owns the first argument after its copy construction and until the
// call begins. A later throwing argument therefore unwinds through a guarded
// destructor cleanup. The false store immediately before consume transfers
// destruction to the callee on both the normal and exceptional call paths.
// CIR-LABEL: cir.func{{.*}} @_Z20destroy_unpassed_argR15CalleeDestroyed
// CIR: %[[ARG_TMP:.*]] = cir.alloca "agg.tmp{{[0-9]+}}" {{.*}} : !cir.ptr<!rec_CalleeDestroyed>
// CIR: %[[ACTIVE:.*]] = cir.alloca "cleanup.isactive" {{.*}} : !cir.ptr<!cir.bool>
// CIR: cir.call @_ZN15CalleeDestroyedC1ERKS_(%[[ARG_TMP]],
// CIR: %[[TRUE:.*]] = cir.const #true
// CIR: cir.store %[[TRUE]], %[[ACTIVE]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR: cir.cleanup.scope {
// CIR:   %[[LATER:.*]] = cir.call @_Z9may_throwv()
// CIR:   %[[FALSE:.*]] = cir.const #false
// CIR:   cir.store %[[FALSE]], %[[ACTIVE]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR:   cir.call @_Z7consume15CalleeDestroyedi({{.*}}%[[LATER]])
// CIR: } cleanup all {
// CIR:   %[[IS_ACTIVE:.*]] = cir.load{{.*}} %[[ACTIVE]] : !cir.ptr<!cir.bool>, !cir.bool
// CIR:   cir.if %[[IS_ACTIVE]] {
// CIR:     cir.call @_ZN15CalleeDestroyedD1Ev(%[[ARG_TMP]])
// CIR:   }
// CIR: }

// LLVM-LABEL: define dso_local void @_Z20destroy_unpassed_argR15CalleeDestroyed
// LLVM: %[[ARG_TMP:.*]] = alloca %struct.CalleeDestroyed
// LLVM: %[[ACTIVE:.*]] = alloca i8
// LLVM: call void @_ZN15CalleeDestroyedC1ERKS_(ptr {{.*}} %[[ARG_TMP]],
// LLVM: store i8 1, ptr %[[ACTIVE]]
// LLVM: %[[LATER:.*]] = invoke {{.*}}i32 @_Z9may_throwv()
// LLVM: to label %[[LATER_CONT:.*]] unwind label %[[UNWIND:.*]]
// LLVM: [[LATER_CONT]]:
// LLVM: store i8 0, ptr %[[ACTIVE]]
// LLVM: call void @_Z7consume15CalleeDestroyedi(
// LLVM: [[UNWIND]]:
// LLVM: landingpad { ptr, i32 }
// LLVM: load i8, ptr %[[ACTIVE]]
// LLVM: call void @_ZN15CalleeDestroyedD1Ev(ptr {{.*}} %[[ARG_TMP]])
