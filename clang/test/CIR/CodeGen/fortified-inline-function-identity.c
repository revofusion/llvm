// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -disable-llvm-passes %s -o %t.cir
// RUN: FileCheck %s --check-prefix=CIR --input-file=%t.cir
// RUN: printf 'usr:c:@F@memset\nparse-usr:c:@F@memset\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -disable-llvm-passes -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.selected.cir
// RUN: FileCheck %s --check-prefix=SELECTED --input-file=%t.selected.cir

typedef __SIZE_TYPE__ size_t;

extern void *memset(void *, int, size_t);

extern __inline __attribute__((__always_inline__, __gnu_inline__, __artificial__))
void *memset(void *dest, int value, size_t length) {
  return __builtin___memset_chk(dest, value, length,
                                __builtin_object_size(dest, 0));
}

void *call_fortified_memset(void *dest, size_t length) {
  return memset(dest, 0, length);
}

// The public ABI endpoint remains declaration-only. The inline builtin body is
// a distinct private definition, but both operations retain the exact USR and
// GlobalDecl linkage name that prove their relationship.
// CIR-DAG: cir.func{{.*}} @memset({{.*}}ast_decl_linkage_name = "memset"{{.*}}ast_decl_usr = "c:@F@memset"
// CIR-DAG: cir.func{{.*}} @memset.inline({{.*}}ast_decl_linkage_name = "memset"{{.*}}ast_decl_usr = "c:@F@memset"{{.*}} {
// CIR: cir.func{{.*}} @call_fortified_memset
// CIR: cir.call @memset.inline

// A USR-selected fortified wrapper authenticates exactly the body-owning CIR
// symbol. The declaration alias remains distinct and is not counted as a
// second definition.
// SELECTED: cir.selected_decl_root_definitions = {"usr:c:@F@memset" = "memset.inline"}
// SELECTED-COUNT-1: cir.func always_inline internal private dso_local @memset.inline({{.*}}ast_decl_linkage_name = "memset"{{.*}}ast_decl_usr = "c:@F@memset"{{.*}} {
// SELECTED: cir.func private @memset({{.*}}ast_decl_linkage_name = "memset"{{.*}}ast_decl_usr = "c:@F@memset"
