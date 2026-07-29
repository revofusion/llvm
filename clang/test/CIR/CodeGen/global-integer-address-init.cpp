// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR \
// RUN:   --implicit-check-not=_ZGVZ19get_reference_table
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s --check-prefix=LLVM \
// RUN:   --implicit-check-not=_ZGVZ19get_reference_table
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=OGCG \
// RUN:   --implicit-check-not=_ZGVZ19get_reference_table

using intptr_t = __INTPTR_TYPE__;
using uintptr_t = __UINTPTR_TYPE__;

extern "C" void runtime_function();
extern "C" void constructor_callback();
extern "C" void attribute_callback();

static const uintptr_t kFunctionAddress =
    reinterpret_cast<uintptr_t>(runtime_function);

extern "C" uintptr_t get_function_address() { return kFunctionAddress; }

struct RuntimeFunction {
  uintptr_t entry;
};

static const RuntimeFunction kIntrinsicFunctions[] = {
    {reinterpret_cast<uintptr_t>(runtime_function)},
};

extern "C" const RuntimeFunction *get_intrinsic_functions() {
  return kIntrinsicFunctions;
}

struct WrapperTypeInfo {
  int tag;
  int payload;
};

struct V8EventTarget {
  static WrapperTypeInfo wrapper_type_info_;

  static constexpr const WrapperTypeInfo *GetWrapperTypeInfo() {
    return &wrapper_type_info_;
  }

  static constexpr const int *GetWrapperPayload() {
    return &wrapper_type_info_.payload;
  }
};

WrapperTypeInfo V8EventTarget::wrapper_type_info_;

extern "C" const intptr_t *get_reference_table() {
  static const intptr_t kReferenceTable[] = {
      reinterpret_cast<intptr_t>(V8EventTarget::GetWrapperTypeInfo()),
      reinterpret_cast<intptr_t>(constructor_callback),
      reinterpret_cast<intptr_t>(attribute_callback),
      reinterpret_cast<intptr_t>(V8EventTarget::GetWrapperPayload()),
  };
  return kReferenceTable;
}

// The destination integer signedness is part of each symbolic relocation.
// CIR-DAG: !rec_RuntimeFunction = !cir.struct<"RuntimeFunction" {!u64i}>
// CIR-DAG: cir.global {{.*}} = #cir.global_view<@runtime_function> : !u64i
// CIR-DAG: cir.global {{.*}} = #cir.const_array<[#cir.const_record<{#cir.global_view<@runtime_function>}>]> : !cir.array<!rec_RuntimeFunction x 1>
// CIR-DAG: cir.global {{.*}} = #cir.const_array<[#cir.global_view<@_ZN13V8EventTarget18wrapper_type_info_E>, #cir.global_view<@constructor_callback>, #cir.global_view<@attribute_callback>, #cir.global_view<@_ZN13V8EventTarget18wrapper_type_info_E, [1 : i32]>]> : !cir.array<!s64i x 4>

// Integer address arrays cannot use dense numeric data: every symbolic leaf
// must survive as a relocatable ptrtoint constant expression.
// LLVM-DAG: @{{[^ ]+}} = internal constant i64 ptrtoint (ptr @runtime_function to i64)
// LLVM-DAG: @{{[^ ]+}} = internal constant [1 x %struct.RuntimeFunction] [%struct.RuntimeFunction { i64 ptrtoint (ptr @runtime_function to i64) }]
// LLVM-DAG: @{{[^ ]+}} = internal constant [4 x i64] [i64 ptrtoint (ptr @_ZN13V8EventTarget18wrapper_type_info_E to i64), i64 ptrtoint (ptr @constructor_callback to i64), i64 ptrtoint (ptr @attribute_callback to i64), i64 ptrtoint (ptr getelementptr {{.*}}ptr @_ZN13V8EventTarget18wrapper_type_info_E{{.*}} to i64)]

// OGCG-DAG: @{{[^ ]+}} = internal constant [1 x %struct.RuntimeFunction] [%struct.RuntimeFunction { i64 ptrtoint (ptr @runtime_function to i64) }]
// OGCG-DAG: @{{[^ ]+}} = internal constant [4 x i64] [i64 ptrtoint (ptr @_ZN13V8EventTarget18wrapper_type_info_E to i64), i64 ptrtoint (ptr @constructor_callback to i64), i64 ptrtoint (ptr @attribute_callback to i64), i64 ptrtoint (ptr getelementptr {{.*}}ptr @_ZN13V8EventTarget18wrapper_type_info_E{{.*}} to i64)]
