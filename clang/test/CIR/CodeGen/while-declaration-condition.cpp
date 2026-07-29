// RUN: %clang_cc1 -no-enable-noundef-analysis %s -triple=x86_64-linux-gnu -fclangir -emit-cir -std=c++17 -fcxx-exceptions -fexceptions -o %t.cir
// RUN: FileCheck -check-prefix=CIR --input-file=%t.cir %s

int next_int();
void observe_int(int);

void scalar_decl_condition() {
  while (int value = next_int())
    observe_int(value);
}

// A scalar condition declaration must leave cir.condition directly in the
// condition region; nesting the terminator in a declaration scope makes the
// cir.while malformed.
// CIR-LABEL: cir.func{{.*}} @_Z21scalar_decl_conditionv()
// CIR:       %[[VALUE:.*]] = cir.alloca "value"{{.*}} : !cir.ptr<!s32i>
// CIR:       cir.while {
// CIR-NOT:     cir.scope
// CIR:         %[[NEXT:.*]] = cir.call @_Z8next_intv() : () -> !s32i
// CIR:         cir.store{{.*}} %[[NEXT]], %[[VALUE]] : !s32i, !cir.ptr<!s32i>
// CIR:         %[[LOADED:.*]] = cir.load{{.*}} %[[VALUE]] : !cir.ptr<!s32i>, !s32i
// CIR:         %[[COND:.*]] = cir.cast int_to_bool %[[LOADED]] : !s32i -> !cir.bool
// CIR-NEXT:    cir.condition(%[[COND]])
// CIR-NEXT:  } do {
// CIR:         %[[BODY_VALUE:.*]] = cir.load{{.*}} %[[VALUE]] : !cir.ptr<!s32i>, !s32i
// CIR:         cir.call @_Z11observe_inti(%[[BODY_VALUE]])

struct Condition {
  ~Condition();
  explicit operator bool() const;
};

Condition make_condition();
void observe(const Condition &);

void record_decl_condition(bool do_continue, bool do_break) {
  while (Condition condition = make_condition()) {
    observe(condition);
    if (do_continue)
      continue;
    if (do_break)
      break;
    observe(condition);
  }
}

// The cleanup is a region of the cir.while itself, so it runs once on every
// per-evaluation exit: a false condition, body fallthrough, continue, or break.
// With exceptions enabled, cleanup all also covers unwinding from the
// conversion and body calls. The active flag prevents destruction if the
// condition initializer throws before construction completes.
// CIR-LABEL: cir.func{{.*}} @_Z21record_decl_conditionbb(
// CIR-DAG:   %[[RECORD:.*]] = cir.alloca "condition"{{.*}} : !cir.ptr<!rec_Condition>
// CIR-DAG:   %[[ACTIVE:.*]] = cir.alloca "cond.cleanup.isactive"{{.*}} : !cir.ptr<!cir.bool>
// CIR:       cir.while {
// CIR-NOT:     cir.cleanup.scope
// CIR:         %[[INACTIVE:.*]] = cir.const #false
// CIR-NEXT:    cir.store %[[INACTIVE]], %[[ACTIVE]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR:         %[[MADE:.*]] = cir.call @_Z14make_conditionv() : () -> !rec_Condition
// CIR-NEXT:    cir.store{{.*}} %[[MADE]], %[[RECORD]] : !rec_Condition, !cir.ptr<!rec_Condition>
// CIR:         %[[NOW_ACTIVE:.*]] = cir.const #true
// CIR-NEXT:    cir.store %[[NOW_ACTIVE]], %[[ACTIVE]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR:         %[[RECORD_COND:.*]] = cir.call @_ZNK9ConditioncvbEv(%[[RECORD]])
// CIR-NEXT:    cir.condition(%[[RECORD_COND]])
// CIR-NEXT:  } do {
// CIR:         cir.call @_Z7observeRK9Condition(%[[RECORD]])
// CIR:         cir.continue
// CIR:         cir.break
// CIR:         cir.call @_Z7observeRK9Condition(%[[RECORD]])
// CIR:         cir.yield
// CIR-NEXT:  } cleanup all {
// CIR:         %[[IS_ACTIVE:.*]] = cir.load{{.*}} %[[ACTIVE]] : !cir.ptr<!cir.bool>, !cir.bool
// CIR-NEXT:    cir.if %[[IS_ACTIVE]] {
// CIR-NEXT:      cir.call @_ZN9ConditionD1Ev(%[[RECORD]]) nothrow
// CIR-NEXT:    } loc(
// CIR-NEXT:    cir.yield
// CIR-NEXT:    } loc(
// CIR-NEXT:  } loc(
