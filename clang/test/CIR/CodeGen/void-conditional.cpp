// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t.ll %s

extern "C" bool get_condition();
extern "C" void true_arm();
extern "C" void false_arm();

// This used to ask the CIR builder for a null !cir.void value when both cheap
// arms were evaluated unconditionally.
extern "C" void cheap_void_conditional() {
  get_condition() ? static_cast<void>(0) : static_cast<void>(1);
}

// CIR-LABEL: cir.func{{.*}} @cheap_void_conditional()
// CIR-NOT: cir.select
// CIR-NOT: cir.ternary
// CIR: %[[COND:.*]] = cir.call @get_condition()
// CIR-NOT: cir.select
// CIR-NOT: cir.ternary
// CIR: cir.return
// LLVM-LABEL: define{{.*}} void @cheap_void_conditional()
// LLVM-NOT: select
// LLVM-NOT: br i1
// LLVM: call{{.*}} i1 @get_condition()
// LLVM-NOT: select
// LLVM-NOT: br i1
// LLVM: ret void

extern "C" void constant_true_void_conditional() {
  true ? true_arm() : false_arm();
}

// CIR-LABEL: cir.func{{.*}} @constant_true_void_conditional()
// CIR-NOT: cir.ternary
// CIR-NOT: cir.select
// CIR-NOT: cir.call @false_arm()
// CIR: cir.call @true_arm()
// CIR-NOT: cir.call @false_arm()
// CIR: cir.return
// LLVM-LABEL: define{{.*}} void @constant_true_void_conditional()
// LLVM-NOT: br i1
// LLVM-NOT: call void @false_arm()
// LLVM: call void @true_arm()
// LLVM-NOT: call void @false_arm()
// LLVM-NEXT: ret void

extern "C" void constant_false_void_conditional() {
  false ? true_arm() : false_arm();
}

// CIR-LABEL: cir.func{{.*}} @constant_false_void_conditional()
// CIR-NOT: cir.ternary
// CIR-NOT: cir.select
// CIR-NOT: cir.call @true_arm()
// CIR: cir.call @false_arm()
// CIR-NOT: cir.call @true_arm()
// CIR: cir.return
// LLVM-LABEL: define{{.*}} void @constant_false_void_conditional()
// LLVM-NOT: br i1
// LLVM-NOT: call void @true_arm()
// LLVM: call void @false_arm()
// LLVM-NOT: call void @true_arm()
// LLVM-NEXT: ret void

extern "C" void dynamic_void_conditional(bool condition) {
  condition ? true_arm() : false_arm();
}

// CIR-LABEL: cir.func{{.*}} @dynamic_void_conditional(
// CIR-NOT: cir.select
// CIR: %[[DYNAMIC_COND:.*]] = cir.load{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CIR: cir.ternary(%[[DYNAMIC_COND]], true {
// CIR-NEXT: cir.call @true_arm()
// CIR-NEXT: cir.yield
// CIR-NEXT: }, false {
// CIR-NEXT: cir.call @false_arm()
// CIR-NEXT: cir.yield
// CIR-NEXT: }) : (!cir.bool) -> ()
// CIR-NEXT: cir.return
// LLVM-LABEL: define{{.*}} void @dynamic_void_conditional(
// LLVM-NOT: select
// LLVM: br i1 %{{.*}}, label %[[TRUE:.*]], label %[[FALSE:.*]]
// LLVM: [[TRUE]]:
// LLVM-NEXT: call void @true_arm()
// LLVM-NEXT: br label %[[CONT:.*]]
// LLVM: [[FALSE]]:
// LLVM-NEXT: call void @false_arm()
// LLVM-NEXT: br label %[[CONT]]
// LLVM: [[CONT]]:
// LLVM: ret void
