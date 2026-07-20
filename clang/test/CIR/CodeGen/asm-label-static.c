// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -disable-llvm-passes %s -o %t-x86.cir
// RUN: FileCheck %s --check-prefix=CIR-X86 --input-file=%t-x86.cir
// RUN: %clang_cc1 -triple aarch64-apple-darwin-macho -fclangir -emit-cir -disable-llvm-passes %s -o %t-arm64.cir
// RUN: FileCheck %s --check-prefix=CIR-DARWIN --input-file=%t-arm64.cir
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm -disable-llvm-passes %s -o %t-x86-cir.ll
// RUN: FileCheck %s --check-prefix=LLVM-X86 --input-file=%t-x86-cir.ll
// RUN: %clang_cc1 -triple aarch64-apple-darwin-macho -fclangir -emit-llvm -disable-llvm-passes %s -o %t-arm64-cir.ll
// RUN: FileCheck %s --check-prefix=LLVM-DARWIN --input-file=%t-arm64-cir.ll
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -disable-llvm-passes %s -o %t-x86-og.ll
// RUN: FileCheck %s --check-prefix=LLVM-X86 --input-file=%t-x86-og.ll
// RUN: %clang_cc1 -triple aarch64-apple-darwin-macho -emit-llvm -disable-llvm-passes %s -o %t-arm64-og.ll
// RUN: FileCheck %s --check-prefix=LLVM-DARWIN --input-file=%t-arm64-og.ll

const char *static_asm_label(void) {
  static const char text[] __asm("LOS_LOG_0") = "x";
  return text;
}

// CIR-X86-DAG: cir.global "private"{{.*}} internal{{.*}} @LOS_LOG_0 = #cir.const_array
// CIR-X86-LABEL: cir.func{{.*}} @static_asm_label(
// CIR-X86: cir.get_global @LOS_LOG_0
// CIR-DARWIN-DAG: cir.global "private"{{.*}} internal{{.*}} @"\01LOS_LOG_0" = #cir.const_array
// CIR-DARWIN-LABEL: cir.func{{.*}} @static_asm_label(
// CIR-DARWIN: cir.get_global @"\01LOS_LOG_0"

// LLVM-X86-DAG: @LOS_LOG_0 = internal constant [2 x i8] c"x\00"
// LLVM-X86-LABEL: define{{.*}} ptr @{{_?}}static_asm_label(
// LLVM-DARWIN-DAG: @"\01LOS_LOG_0" = internal constant [2 x i8] c"x\00"
// LLVM-DARWIN-LABEL: define{{.*}} ptr @{{_?}}static_asm_label(
