// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -fcxx-exceptions -fexceptions %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR

struct [[clang::trivial_abi]] CalleeDestroyed {
  int value;
  CalleeDestroyed(const CalleeDestroyed &);
  ~CalleeDestroyed();
};

int may_throw();
void consume(CalleeDestroyed, int) noexcept;

void conditional_callee_destroyed_arg(bool condition,
                                      CalleeDestroyed &value) {
  condition ? consume(value, may_throw()) : void();
}

// A callee-destroyed argument remains caller-owned while later arguments are
// evaluated. In a conditional arm, the active flag is false before the arm,
// becomes true immediately after construction, and is cleared only when
// ownership transfers to the callee.
// CIR-LABEL: cir.func{{.*}} @_Z32conditional_callee_destroyed_argbR15CalleeDestroyed
// CIR: %[[ACTIVE:.*]] = cir.alloca "cleanup.cond" {{.*}} : !cir.ptr<!cir.bool>
// CIR: %[[INIT_FALSE:.*]] = cir.const #false
// CIR-NEXT: cir.store %[[INIT_FALSE]], %[[ACTIVE]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR: cir.ternary({{.*}}, true {
// CIR:   cir.call @_ZN15CalleeDestroyedC1ERKS_
// CIR:   %[[TRUE:.*]] = cir.const #true
// CIR-NEXT:   cir.store %[[TRUE]], %[[ACTIVE]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR:   %[[LATER:.*]] = cir.call @_Z9may_throwv()
// CIR:   %[[FALSE:.*]] = cir.const #false
// CIR-NEXT:   cir.store %[[FALSE]], %[[ACTIVE]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR:   cir.call @_Z7consume15CalleeDestroyedi
