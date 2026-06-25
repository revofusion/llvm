// RUN: %clang_cc1 -triple aarch64-none-linux-android21 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=CIR --input-file=%t.cir %s
// RUN: %clang_cc1 -triple aarch64-none-linux-android21 -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --check-prefix=LLVM --input-file=%t-cir.ll %s
// RUN: %clang_cc1 -triple aarch64-none-linux-android21 -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefix=OGCG --input-file=%t.ll %s

// A constant l-value that points into the *middle* of a string literal used to
// crash CIRGen ("UNREACHABLE ... unexpected type" in
// computeGlobalViewIndicesFromFlatOffset): the string literal's GlobalViewAttr
// has the element type (`!s8i`) as its pointee, but the byte offset must be
// resolved against the underlying array global's type. ConstantLValueEmitter
// now walks the referenced global's storage type, so the offset is lowered to a
// GlobalViewAttr index into the array.

const char *ptr_into_literal = "0123456789abcdefghij" + 7;

// A non-zero offset into the middle of a string literal stored into a static
// local constant initializer exercises the same constant-l-value path inside a
// function body (this is the exact shape that crashed in ANGLE's
// ResourceManager.cpp).
const char *static_local_offset() {
  static const char *s = "abcdefghijklmnopqrstuvwxyz0123456789" + 30;
  return s;
}

// CIR: cir.global "private" internal dso_local @_ZZ19static_local_offsetvE1s = #cir.global_view<@{{.+}}, [30 : i32]> : !cir.ptr<!s8i>
// CIR: cir.global "private" constant cir_private dso_local @[[STR:.+]] = #cir.const_array<"0123456789abcdefghij\00" : !cir.array<!s8i x 21>> : !cir.array<!s8i x 21>
// CIR: cir.global external @ptr_into_literal = #cir.global_view<@[[STR]], [7 : i32]> : !cir.ptr<!s8i>

// LLVM: @_ZZ19static_local_offsetvE1s = internal global ptr getelementptr inbounds nuw (i8, ptr @{{.+}}, i64 30)
// LLVM: @[[STR:.+]] = private constant [21 x i8] c"0123456789abcdefghij\00"
// LLVM: @ptr_into_literal = global ptr getelementptr inbounds nuw (i8, ptr @[[STR]], i64 7)

// OGCG: @[[STR:.+]] = private unnamed_addr constant [21 x i8] c"0123456789abcdefghij\00"
// OGCG: @ptr_into_literal = global ptr getelementptr (i8, ptr @[[STR]], i64 7)
