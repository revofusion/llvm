// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm -disable-llvm-passes %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s -check-prefix=LLVM
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm -disable-llvm-passes %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s -check-prefix=OGCG

typedef unsigned long size_t;

// Normal inline builtin declaration
// When a builtin is redefined with extern inline + always_inline attributes,
// the compiler creates a .inline version to avoid conflicts with the builtin

extern inline __attribute__((always_inline)) __attribute__((gnu_inline))
void *memcpy(void *a, const void *b, size_t c) {
  return __builtin_memcpy(a, b, c);
}

void *test_inline_builtin_memcpy(void *a, const void *b, size_t c) {
  return memcpy(a, b, c);
}

// The inline body and external fallback declaration have distinct symbols.
// Declaration-only FuncOps must be private to satisfy the module verifier.
// CIR-DAG: cir.func always_inline internal private{{.*}}@memcpy.inline({{.*}}) -> !cir.ptr<!void>
// CIR-DAG: cir.func always_inline internal private{{.*}}@memcmp.inline({{.*}}) -> !s32i
// CIR-DAG: cir.func always_inline internal private{{.*}}@memset.inline({{.*}}) -> !cir.ptr<!void>
// CIR: cir.func private{{.*}}@memcpy({{.*}}) -> !cir.ptr<!void>

// CIR-LABEL: cir.func{{.*}} @test_inline_builtin_memcpy(
// CIR:         cir.call @memcpy.inline({{.*}}){{.*}} : (!cir.ptr<!void>{{.*}}, !cir.ptr<!void>{{.*}}, !u64i{{.*}}) -> !cir.ptr<!void>
// CIR:       }

// LLVM: define internal ptr @memcpy.inline(ptr{{.*}}, ptr{{.*}}, i64{{.*}}) #{{[0-9]+}}

// LLVM-LABEL: @test_inline_builtin_memcpy(
// LLVM:         call ptr @memcpy.inline(

// OGCG-LABEL: @test_inline_builtin_memcpy(
// OGCG:         call ptr @memcpy.inline(

// OGCG: define internal ptr @memcpy.inline(ptr{{.*}} %a, ptr{{.*}} %b, i64{{.*}} %c) #{{[0-9]+}}

extern inline __attribute__((always_inline)) __attribute__((gnu_inline))
int memcmp(const void *a, const void *b, size_t c) {
  return __builtin_memcmp(a, b, c);
}

int test_inline_builtin_memcmp(const void *a, const void *b, size_t c) {
  return memcmp(a, b, c);
}

// CIR: cir.func private{{.*}}@memcmp({{.*}}) -> !s32i
//
// CIR-LABEL: cir.func{{.*}} @test_inline_builtin_memcmp(
// CIR:         cir.call @memcmp.inline({{.*}}){{.*}} : (!cir.ptr<!void>{{.*}}, !cir.ptr<!void>{{.*}}, !u64i{{.*}}) -> !s32i
// CIR:       }

extern inline __attribute__((always_inline)) __attribute__((gnu_inline))
void *memset(void *a, int value, size_t c) {
  return __builtin_memset(a, value, c);
}

void *test_inline_builtin_memset(void *a, int value, size_t c) {
  return memset(a, value, c);
}

// CIR: cir.func private{{.*}}@memset({{.*}}) -> !cir.ptr<!void>
//
// CIR-LABEL: cir.func{{.*}} @test_inline_builtin_memset(
// CIR:         cir.call @memset.inline({{.*}}){{.*}} : (!cir.ptr<!void>{{.*}}, !s32i{{.*}}, !u64i{{.*}}) -> !cir.ptr<!void>
// CIR:       }

// Shadowing case
// When a non-inline function definition shadows an inline builtin declaration,
// the .inline version should be replaced with the regular function and removed.

extern inline __attribute__((always_inline)) __attribute__((gnu_inline))
void *memmove(void *a, const void *b, size_t c) {
  return __builtin_memmove(a, b, c);
}

void *memmove(void *a, const void *b, size_t c) {
  char *dst = (char *)a;
  const char *src = (const char *)b;
  if (dst < src) {
    for (size_t i = 0; i < c; i++) {
      dst[i] = src[i];
    }
  } else {
    for (size_t i = c; i > 0; i--) {
      dst[i-1] = src[i-1];
    }
  }
  return a;
}

void *test_shadowed_memmove(void *a, const void *b, size_t c) {
  return memmove(a, b, c);
}

// CIR: cir.func{{.*}}@memmove({{.*}}) -> !cir.ptr<!void>{{.*}}{
// CIR-NOT: @memmove.inline

// CIR-LABEL: @test_shadowed_memmove(
// CIR: cir.call @memmove(
// CIR-NOT: @memmove.inline
// CIR: }

// LLVM: define dso_local ptr @memmove(ptr{{.*}}, ptr{{.*}}, i64{{.*}}) #{{[0-9]+}}
// LLVM-NOT: @memmove.inline

// LLVM-LABEL: @test_shadowed_memmove(
// TODO - this deviation from OGCG is expected until we implement the nobuiltin
// attribute. See CIRGenFunction::emitDirectCallee
// LLVM: call ptr @memmove(
// LLVM-NOT: @memmove.inline
// LLVM: }

// OGCG: define dso_local ptr @memmove(ptr{{.*}} %a, ptr{{.*}} %b, i64{{.*}} %c) #{{[0-9]+}}
// OGCG-NOT: @memmove.inline

// OGCG-LABEL: @test_shadowed_memmove(
// OGCG: call void @llvm.memmove.p0.p0.i64(
// OGCG-NOT: @memmove.inline
// OGCG: }
