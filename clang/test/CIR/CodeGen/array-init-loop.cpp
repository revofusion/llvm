// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s -check-prefix=LLVM

// An ArrayInitLoopExpr is produced when an array is copied element-wise, e.g.
// when a lambda captures an array by value: the array member of the closure is
// initialized from the captured array via a per-element copy loop. The common
// subexpression is indexed by an ArrayInitIndexExpr bound to the loop index.

auto capture_array() {
  int data[3] = {1, 2, 3};
  return [data] { return data[0]; };
}

// CIR-LABEL: cir.func {{.*}}@_Z13capture_arrayv()
// The loop keeps a running element pointer in an "arrayinit.temp" slot; the
// index for the ArrayInitIndexExpr is recovered from it via cir.ptr_diff.
// CIR: %[[ITR:.*]] = cir.alloca "arrayinit.temp" {{.*}} : !cir.ptr<!cir.ptr<!s32i>>
// The destination (closure array member) decays to an element pointer.
// CIR: %[[BEGIN:.*]] = cir.cast array_to_ptrdecay {{.*}} -> !cir.ptr<!s32i>
// CIR: cir.store {{.*}}%[[BEGIN]], %[[ITR]]
// CIR: %[[N:.*]] = cir.const #cir.int<3>
// CIR: %[[END:.*]] = cir.ptr_stride %[[BEGIN]], %[[N]]
// CIR: cir.do {
// CIR:   %[[CUR:.*]] = cir.load {{.*}}%[[ITR]]
// The current index is the distance from the array beginning.
// CIR:   cir.ptr_diff %[[CUR]], %[[BEGIN]]
// The ArrayInitIndexExpr reads the same index to subscript the source array.
// CIR:   cir.get_element
// Advance the running pointer and store it back.
// CIR:   %[[NEXT:.*]] = cir.ptr_stride %[[CUR]], {{.*}}
// CIR:   cir.store {{.*}}%[[NEXT]], %[[ITR]]
// CIR:   cir.yield
// CIR: } while {
// CIR:   cir.cmp ne {{.*}}, %[[END]]
// CIR:   cir.condition
// CIR: }

// The CIR loop lowers to an element-wise copy loop in LLVM IR.
// LLVM-LABEL: define {{.*}} @_Z13capture_arrayv()
// LLVM: %[[ITR:.*]] = alloca ptr
// LLVM: store ptr {{.*}}, ptr %[[ITR]]
// LLVM: getelementptr {{.*}}, i64 3
// The condition block compares the running pointer against the array end.
// LLVM: icmp ne ptr
// The ptr_diff-derived index feeds the source-array subscript.
// LLVM: sdiv exact i64
// LLVM: load i32
// LLVM: store i32
// LLVM: ret
