// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck --check-prefix=CIR %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o - | FileCheck --check-prefix=LLVM %s

typedef int v4si __attribute__((vector_size(16)));

int test_builtin_reduce_or(v4si x) {
  return __builtin_reduce_or(x);
}

// CIR-LABEL: @_Z22test_builtin_reduce_orDv4_i
// CIR:         %[[VALUE:.*]] = cir.load {{.*}} : !cir.ptr<!cir.vector<4 x !s32i>>, !cir.vector<4 x !s32i>
// CIR:         %[[RESULT:.*]] = cir.call_llvm_intrinsic "vector.reduce.or" %[[VALUE]] : (!cir.vector<4 x !s32i>) -> !s32i
// CIR:         cir.store %[[RESULT]], %{{.*}} : !s32i, !cir.ptr<!s32i>
// CIR:         %[[RET:.*]] = cir.load %{{.*}} : !cir.ptr<!s32i>, !s32i
// CIR:         cir.return %[[RET]] : !s32i

// LLVM-LABEL: @_Z22test_builtin_reduce_orDv4_i
// LLVM:         %[[RESULT:.*]] = call i32 @llvm.vector.reduce.or.v4i32(<4 x i32> %{{.*}})
// LLVM:         store i32 %[[RESULT]], ptr %[[RET_PTR:.*]], align 4
// LLVM:         %[[RET:.*]] = load i32, ptr %[[RET_PTR]], align 4
// LLVM:         ret i32 %[[RET]]
