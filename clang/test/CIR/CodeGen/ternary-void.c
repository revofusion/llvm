// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s

void sink_true(int *);
void sink_false(int *);

void test_void_ternary(_Bool cond, int *ptr) {
  cond ? sink_true(ptr) : sink_false(ptr);
}

// CIR-LABEL: cir.func{{.*}} @test_void_ternary(
// CIR: cir.ternary(%{{.*}}, true {
// CIR:   cir.call @sink_true(%{{.*}}) : (!cir.ptr<!s32i>) -> ()
// CIR:   cir.yield
// CIR: }, false {
// CIR:   cir.call @sink_false(%{{.*}}) : (!cir.ptr<!s32i>) -> ()
// CIR:   cir.yield
// CIR: })
