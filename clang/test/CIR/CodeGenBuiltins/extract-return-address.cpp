// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -disable-llvm-passes %s -o %t-x86.cir
// RUN: FileCheck %s --check-prefix=CIR --input-file=%t-x86.cir
// RUN: %clang_cc1 -triple aarch64-apple-darwin-macho -fclangir -emit-cir -disable-llvm-passes %s -o %t-arm64.cir
// RUN: FileCheck %s --check-prefix=CIR --input-file=%t-arm64.cir
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm -disable-llvm-passes %s -o %t-x86-cir.ll
// RUN: FileCheck %s --check-prefix=LLVM --input-file=%t-x86-cir.ll
// RUN: %clang_cc1 -triple aarch64-apple-darwin-macho -fclangir -emit-llvm -disable-llvm-passes %s -o %t-arm64-cir.ll
// RUN: FileCheck %s --check-prefix=LLVM --input-file=%t-arm64-cir.ll
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -disable-llvm-passes %s -o %t-x86-og.ll
// RUN: FileCheck %s --check-prefix=LLVM --input-file=%t-x86-og.ll
// RUN: %clang_cc1 -triple aarch64-apple-darwin-macho -emit-llvm -disable-llvm-passes %s -o %t-arm64-og.ll
// RUN: FileCheck %s --check-prefix=LLVM --input-file=%t-arm64-og.ll

extern "C" void consume(void *);

extern "C" void extract_return_address(void *address) {
  consume(__builtin_extract_return_addr(address));
}

// x86_64 and AArch64 decode stored return addresses as the identity.
// CIR-LABEL: cir.func{{.*}} @extract_return_address(
// CIR: %[[ADDRESS:.*]] = cir.load{{.*}} : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// CIR-NEXT: cir.call @consume(%[[ADDRESS]])

// LLVM-LABEL: define{{.*}} void @extract_return_address(
// LLVM: %[[ADDRESS:.*]] = load ptr, ptr %{{.*}}
// LLVM-NEXT: call void @consume(ptr{{.*}} %[[ADDRESS]])
