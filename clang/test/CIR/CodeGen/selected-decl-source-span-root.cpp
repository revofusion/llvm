// RUN: printf 'source-root:11:18:11:50:%s|_ZL18tint_static_init_0\n_ZL18tint_static_init_0\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir

#define CAT_INNER(A, B) A##B
#define CAT(A, B) CAT_INNER(A, B)
#define STATIC_INIT CAT(tint_static_init_, __COUNTER__)

enum { consumed_counter = __COUNTER__ };

[[maybe_unused]] static const bool STATIC_INIT = true;

// The authority pass selected suffix 0 in its preprocessing state. The
// selective CIR pass consumed the same stateful macro once earlier and owns
// suffix 1. The exact expansion span authenticates that declaration without
// name, prefix, or suffix inference.
// CHECK: cir.selected_decl_root_definitions = {_ZL18tint_static_init_0 = "_ZL18tint_static_init_1"}
// CHECK: cir.global{{.*}} @_ZL18tint_static_init_1 = #true
