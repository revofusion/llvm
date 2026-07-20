// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s --check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=LLVM
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o - | FileCheck %s --check-prefix=LLVM
// RUN: %clang_cc1 -triple arm64-apple-darwin -fclangir -emit-cir %s -o - | FileCheck %s --check-prefix=DARWIN-CIR
// RUN: %clang_cc1 -triple arm64-apple-darwin -fclangir -emit-llvm %s -o - | FileCheck %s --check-prefix=DARWIN-LLVM
// RUN: %clang_cc1 -triple arm64-apple-darwin -emit-llvm %s -o - | FileCheck %s --check-prefix=DARWIN-OGCG
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -DCIR_PRAGMA_BSS_NEGATIVE -fclangir -emit-cir %s -o - 2>&1 | FileCheck %s --check-prefix=BSS-ERROR
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -DCIR_PRAGMA_DATA_NEGATIVE -fclangir -emit-cir %s -o - 2>&1 | FileCheck %s --check-prefix=DATA-ERROR
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -DCIR_PRAGMA_RODATA_NEGATIVE -fclangir -emit-cir %s -o - 2>&1 | FileCheck %s --check-prefix=RODATA-ERROR
// RUN: not %clang_cc1 -triple x86_64-unknown-linux-gnu -DCIR_PRAGMA_RELRO_NEGATIVE -fclangir -emit-cir %s -o - 2>&1 | FileCheck %s --check-prefix=RELRO-ERROR
// CIR-DAG: cir.global "private" internal {{.*}}@get_static_section.value = #cir.int<7> : !s32i {{{.*}}section = ".custom_static"}
// LLVM-DAG: @get_static_section.value = internal global i32 7, section ".custom_static"

#if !defined(__APPLE__)
extern int __attribute__((section(".shared"))) ext;
int getExt(void) {
  return ext;
}
// CIR-DAG: cir.global "private" external @ext : !s32i {{{.*}}section = ".shared"}
// LLVM-DAG: @ext = external global i32, section ".shared"

int __attribute__((section(".shared"))) glob = 42;
// CIR-DAG: cir.global external @glob = #cir.int<42> : !s32i {{{.*}}section = ".shared"}
// LLVM-DAG: @glob = global i32 42, section ".shared"

__attribute__((section(".custom_fn"))) void func_in_section(void) {}
// CIR: cir.func {{.*}}@func_in_section() {{.*}}section = ".custom_fn"
// LLVM: define {{.*}}@func_in_section(){{.*}}section ".custom_fn"

int get_static_section(void) {
  static int value __attribute__((section(".custom_static"))) = 7;
  return value;
}
#endif

#if defined(__APPLE__)
int get_static_darwin_section(void) {
  static const char value[] __attribute__((section("__TEXT,__oslogstring,cstring_literals"))) =
      "x";
  return value[0];
}
// DARWIN-CIR: cir.global "private"{{.*}}@get_static_darwin_section.value = #cir.const_array<"x"
// DARWIN-CIR-SAME: section = "__TEXT,__oslogstring,cstring_literals"
// DARWIN-LLVM: @get_static_darwin_section.value = internal constant{{.*}}section "__TEXT,__oslogstring,cstring_literals"
// DARWIN-OGCG: @get_static_darwin_section.value = internal constant{{.*}}section "__TEXT,__oslogstring,cstring_literals"
#endif

#if defined(CIR_PRAGMA_BSS_NEGATIVE)
#pragma clang section bss=".pragma_static"
int get_pragma_bss_section(void) {
  static int value;
  return value;
}
// BSS-ERROR: error: {{.*}}emitStaticVarDecl: CIR global BSS section attribute
#elif defined(CIR_PRAGMA_DATA_NEGATIVE)
#pragma clang section data=".pragma_static"
int get_pragma_data_section(void) {
  static int value = 1;
  return value;
}
// DATA-ERROR: error: {{.*}}emitStaticVarDecl: CIR global Data section attribute
#elif defined(CIR_PRAGMA_RODATA_NEGATIVE)
#pragma clang section rodata=".pragma_static"
int get_pragma_rodata_section(void) {
  static const int value = 1;
  return value;
}
// RODATA-ERROR: error: {{.*}}emitStaticVarDecl: CIR global Rodata section attribute
#elif defined(CIR_PRAGMA_RELRO_NEGATIVE)
typedef void (*section_function_t)(void);
static void section_function(void) {}
#pragma clang section relro=".pragma_static"
int get_pragma_relro_section(void) {
  static const section_function_t value = section_function;
  return value != 0;
}
// RELRO-ERROR: error: {{.*}}emitStaticVarDecl: CIR global Relro section attribute
#endif
