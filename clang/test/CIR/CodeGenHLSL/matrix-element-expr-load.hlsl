// RUN: not %clang_cc1 -x hlsl -finclude-default-header -triple spirv-unknown-vulkan-compute %s \
// RUN:   -fclangir -emit-cir -disable-llvm-passes 2>&1 | FileCheck %s

// CHECK: error: ClangIR code gen Not Yet Implemented: Matrix type conversion
float test_zero_indexed(float2x2 M) {
  return M._m00;
}
