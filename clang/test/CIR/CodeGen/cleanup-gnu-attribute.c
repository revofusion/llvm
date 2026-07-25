// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=LLVM

void cleanup(unsigned long **);

unsigned long *cleanup_pointer_return(void) {
  unsigned long *v __attribute__((cleanup(cleanup))) = 0;
  return v;
}

// CIR-LABEL: cir.func{{.*}} @cleanup_pointer_return()
// CIR:         %[[V:.*]] = cir.alloca "v" {{.*}} : !cir.ptr<!cir.ptr<!u64i>>
// CIR:         cir.cleanup.scope {
// CIR:           cir.return
// CIR:         } cleanup normal {
// CIR:           cir.call @cleanup(%[[V]]) : (!cir.ptr<!cir.ptr<!u64i>> {{.*}}) -> ()
// CIR-NEXT:      cir.yield
// CIR:         }

// LLVM-LABEL: define{{.*}} ptr @cleanup_pointer_return()
// LLVM:         call void @cleanup(ptr {{.*}})
// LLVM:         ret ptr
