// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fcxx-exceptions -fexceptions -fclangir -emit-cir %s -o %t.cir
// RUN: cir-opt %t.cir --verify-roundtrip -o %t.roundtrip.cir
// RUN: FileCheck %s --input-file=%t.roundtrip.cir --check-prefix=CIR
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fcxx-exceptions -fexceptions -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck %s --input-file=%t.ll --check-prefix=LLVM
//
// The CIR round-trip verifies that all structured cir.if regions have
// terminators and that values used after cleanup/null-check regions dominate
// their uses.

using size_t = decltype(sizeof(0));

struct Nothrow {};
extern Nothrow nothrow;

void *operator new(size_t, const Nothrow &) noexcept;
void operator delete(void *, const Nothrow &) noexcept;
void *operator new[](size_t, const Nothrow &) noexcept;
void operator delete[](void *, const Nothrow &) noexcept;

struct ArrayElement {
  ArrayElement();
  ~ArrayElement();
  int value;
};

extern "C" ArrayElement *array_with_cleanup(size_t count) {
  return new ArrayElement[count];
}

// The adjusted array pointer is formed inside the operator-delete cleanup but
// is stored in a slot outside that region and reloaded only after it exits.
// The exceptional cleanup must continue to delete the unadjusted allocation.
// CIR-LABEL: cir.func {{.*}} @array_with_cleanup(
// CIR:         %[[RAW:.*]] = cir.call @_Znam
// CIR:         %[[RESULT_SLOT:.*]] = cir.alloca {{.*}} : !cir.ptr<!cir.ptr<!rec_ArrayElement>>
// CIR:         cir.cleanup.scope {
// CIR:           %[[RAW_BYTES:.*]] = cir.cast bitcast %[[RAW]] : !cir.ptr<!void> -> !cir.ptr<!u8i>
// CIR:           %[[ADJUSTED_BYTES:.*]] = cir.ptr_stride %[[RAW_BYTES]], %{{.*}} : (!cir.ptr<!u8i>, {{.*}}) -> !cir.ptr<!u8i>
// CIR:           %[[ADJUSTED_VOID:.*]] = cir.cast bitcast %[[ADJUSTED_BYTES]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:           %[[ADJUSTED:.*]] = cir.cast bitcast %[[ADJUSTED_VOID]] : !cir.ptr<!void> -> !cir.ptr<!rec_ArrayElement>
// CIR:           cir.store{{.*}} %[[ADJUSTED]], %[[RESULT_SLOT]] : !cir.ptr<!rec_ArrayElement>, !cir.ptr<!cir.ptr<!rec_ArrayElement>>
// CIR:           cir.call @_ZN12ArrayElementC1Ev
// CIR:         } cleanup eh {
// CIR:           cir.call @_ZdaPv{{.*}}(%[[RAW]]
// CIR:         }
// CIR:         %[[RESULT:.*]] = cir.load{{.*}} %[[RESULT_SLOT]] : !cir.ptr<!cir.ptr<!rec_ArrayElement>>, !cir.ptr<!rec_ArrayElement>

// LLVM-LABEL: define{{.*}} ptr @array_with_cleanup(
// LLVM:         %[[RAW:.*]] = call{{.*}} ptr @_Znam(
// LLVM:         %[[ADJUSTED:.*]] = getelementptr {{.*}}i8, ptr %[[RAW]], i64 {{.*}}
// LLVM:         store ptr %[[ADJUSTED]], ptr %[[RESULT_SLOT:.*]]
// LLVM:         invoke void @_ZN12ArrayElementC1Ev
// LLVM:         call void @_ZdaPv{{.*}}(ptr {{.*}}%[[RAW]]
// LLVM:         %[[RESULT:.*]] = load ptr, ptr %[[RESULT_SLOT]]
// LLVM:         ret ptr

extern "C" ArrayElement *conditional_nothrow_array(bool choose, size_t count) {
  return choose ? new (nothrow) ArrayElement[count] : nullptr;
}

// The new-expression result slot is in the ternary arm but outside the nested
// allocation null-check. It starts as nullptr, is overwritten only by the
// non-null path with the cookie-adjusted pointer, and is reloaded after the
// cir.if. The verifier RUN line also checks that the null-check cir.if and any
// cleanup cir.if have terminators.
// CIR-LABEL: cir.func {{.*}} @conditional_nothrow_array(
// CIR:         cir.ternary({{.*}}, true {
// CIR:           %[[RAW:.*]] = cir.call @_ZnamRK7Nothrow
// CIR:           %[[RESULT_SLOT:.*]] = cir.alloca {{.*}} : !cir.ptr<!cir.ptr<!rec_ArrayElement>>
// CIR:           %[[NULL_RESULT:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_ArrayElement>
// CIR:           cir.store{{.*}} %[[NULL_RESULT]], %[[RESULT_SLOT]] : !cir.ptr<!rec_ArrayElement>, !cir.ptr<!cir.ptr<!rec_ArrayElement>>
// CIR:           %[[RAW_NULL:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
// CIR:           %[[NONNULL:.*]] = cir.cmp ne %[[RAW]], %[[RAW_NULL]] : !cir.ptr<!void>
// CIR:           cir.if %[[NONNULL]] {
// CIR:             cir.cleanup.scope {
// CIR:               %[[RAW_BYTES:.*]] = cir.cast bitcast %[[RAW]] : !cir.ptr<!void> -> !cir.ptr<!u8i>
// CIR:               %[[ADJUSTED_BYTES:.*]] = cir.ptr_stride %[[RAW_BYTES]], %{{.*}} : (!cir.ptr<!u8i>, {{.*}}) -> !cir.ptr<!u8i>
// CIR:               %[[ADJUSTED_VOID:.*]] = cir.cast bitcast %[[ADJUSTED_BYTES]] : !cir.ptr<!u8i> -> !cir.ptr<!void>
// CIR:               %[[ADJUSTED:.*]] = cir.cast bitcast %[[ADJUSTED_VOID]] : !cir.ptr<!void> -> !cir.ptr<!rec_ArrayElement>
// CIR:               cir.store{{.*}} %[[ADJUSTED]], %[[RESULT_SLOT]] : !cir.ptr<!rec_ArrayElement>, !cir.ptr<!cir.ptr<!rec_ArrayElement>>
// CIR:               cir.call @_ZN12ArrayElementC1Ev
// CIR:             } cleanup eh {
// CIR:               cir.call @_ZdaPvRK7Nothrow(%{{.*}},
// CIR:             }
// CIR:           }
// CIR:           %[[TRUE_RESULT:.*]] = cir.load{{.*}} %[[RESULT_SLOT]] : !cir.ptr<!cir.ptr<!rec_ArrayElement>>, !cir.ptr<!rec_ArrayElement>
// CIR:           cir.yield %{{.*}} : !cir.ptr<!rec_ArrayElement>
// CIR:         }, false {
// CIR:           %[[FALSE_RESULT:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_ArrayElement>
// CIR:           cir.yield %[[FALSE_RESULT]] : !cir.ptr<!rec_ArrayElement>
// CIR:         }) : (!cir.bool) -> !cir.ptr<!rec_ArrayElement>

// LLVM-LABEL: define{{.*}} ptr @conditional_nothrow_array(
// LLVM:         %[[RAW:.*]] = call{{.*}} ptr @_ZnamRK7Nothrow(
// LLVM:         store ptr null, ptr %[[RESULT_SLOT:.*]]
// LLVM:         %[[NONNULL:.*]] = icmp ne ptr %[[RAW]], null
// LLVM:         br i1 %[[NONNULL]], label %[[INIT:.*]], label %[[AFTER_INIT:.*]]
// LLVM:       [[INIT]]:
// LLVM:         %[[ADJUSTED:.*]] = getelementptr {{.*}}i8, ptr %[[RAW]], i64 {{.*}}
// LLVM:         store ptr %[[ADJUSTED]], ptr %[[RESULT_SLOT]]
// LLVM:         invoke void @_ZN12ArrayElementC1Ev
// LLVM:         call void @_ZdaPvRK7Nothrow(ptr {{.*}}%{{.*}},
// LLVM:         %[[TRUE_RESULT:.*]] = load ptr, ptr %[[RESULT_SLOT]]
// LLVM:         phi ptr {{.*}}null
// LLVM:         ret ptr

struct Text {
  Text();
  ~Text();
};

struct FormatArg {
  FormatArg(const Text &);
  ~FormatArg();
};

struct Formatter {
  Formatter(const FormatArg &);
};

extern "C" Formatter *formatter_nothrow_new() {
  return new (nothrow) Formatter(FormatArg(Text()));
}

// Both non-trivial argument temporaries are built and destroyed inside the
// successful null-check arm. Deactivating the operator-delete cleanup creates
// another cir.if; verification requires both it and the outer null-check to be
// terminated. The result slot still carries nullptr through the skipped arm.
// CIR-LABEL: cir.func {{.*}} @formatter_nothrow_new()
// CIR:         %[[RAW:.*]] = cir.call @_ZnwmRK7Nothrow
// CIR:         %[[RESULT_SLOT:.*]] = cir.alloca {{.*}} : !cir.ptr<!cir.ptr<!rec_Formatter>>
// CIR:         %[[NULL_RESULT:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!rec_Formatter>
// CIR:         cir.store{{.*}} %[[NULL_RESULT]], %[[RESULT_SLOT]] : !cir.ptr<!rec_Formatter>, !cir.ptr<!cir.ptr<!rec_Formatter>>
// CIR:         %[[RAW_NULL:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
// CIR:         %[[NONNULL:.*]] = cir.cmp ne %[[RAW]], %[[RAW_NULL]] : !cir.ptr<!void>
// CIR:         cir.if %[[NONNULL]] {
// CIR:           cir.cleanup.scope {
// CIR:             %[[OBJECT:.*]] = cir.cast bitcast %[[RAW]] : !cir.ptr<!void> -> !cir.ptr<!rec_Formatter>
// CIR:             cir.store{{.*}} %[[OBJECT]], %[[RESULT_SLOT]] : !cir.ptr<!rec_Formatter>, !cir.ptr<!cir.ptr<!rec_Formatter>>
// CIR:             cir.call @_ZN4TextC1Ev
// CIR:             cir.call @_ZN9FormatArgC1ERK4Text
// CIR:             cir.call @_ZN9FormatterC1ERK9FormatArg(%[[OBJECT]],
// CIR:             cir.call @_ZN9FormatArgD1Ev
// CIR:             cir.call @_ZN4TextD1Ev
// CIR:           } cleanup eh {
// CIR:             %[[DELETE_ACTIVE:.*]] = cir.load{{.*}} : !cir.ptr<!cir.bool>, !cir.bool
// CIR:             cir.if %[[DELETE_ACTIVE]] {
// CIR:               cir.call @_ZdlPvRK7Nothrow(%[[RAW]],
// CIR:             }
// CIR:           }
// CIR:         }
// CIR:         %[[RESULT:.*]] = cir.load{{.*}} %[[RESULT_SLOT]] : !cir.ptr<!cir.ptr<!rec_Formatter>>, !cir.ptr<!rec_Formatter>

// LLVM-LABEL: define{{.*}} ptr @formatter_nothrow_new()
// LLVM:         %[[RAW:.*]] = call{{.*}} ptr @_ZnwmRK7Nothrow(
// LLVM:         store ptr null, ptr %[[RESULT_SLOT:.*]]
// LLVM:         %[[NONNULL:.*]] = icmp ne ptr %[[RAW]], null
// LLVM:         br i1 %[[NONNULL]], label %[[INIT:.*]], label %[[AFTER_INIT:.*]]
// LLVM:       [[INIT]]:
// LLVM:         store ptr %[[RAW]], ptr %[[RESULT_SLOT]]
// LLVM:         invoke void @_ZN4TextC1Ev
// LLVM:         invoke void @_ZN9FormatArgC1ERK4Text
// LLVM:         invoke void @_ZN9FormatterC1ERK9FormatArg(ptr {{.*}} %[[RAW]],
// LLVM:         call void @_ZN9FormatArgD1Ev
// LLVM:         call void @_ZN4TextD1Ev
// LLVM:         call void @_ZdlPvRK7Nothrow(ptr {{.*}}%[[RAW]],
// LLVM:         %[[RESULT:.*]] = load ptr, ptr %[[RESULT_SLOT]]
// LLVM:         ret ptr
