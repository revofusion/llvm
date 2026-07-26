// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t.ll %s

// Chromium's CSS property table is declared as an array of an aligned,
// incomplete union so that an inline accessor can do byte-address arithmetic
// without including every property definition. Emitting that accessor must not
// ask the union for definition-only properties.
union alignas(16) Incomplete;
extern const Incomplete incomplete_objects[];

const char *get_incomplete_object(unsigned index) {
  return reinterpret_cast<const char *>(incomplete_objects) + 16 * index;
}

// CIR: cir.global {{.*}} @incomplete_objects : !cir.array<!s8i x 0>
// CIR-LABEL: cir.func{{.*}} @_Z21get_incomplete_objectj(
// CIR: %[[OBJECTS:.*]] = cir.get_global @incomplete_objects : !cir.ptr<!cir.array<!s8i x 0>>

// LLVM: @incomplete_objects = external global [0 x i8]
