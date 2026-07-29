// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s --check-prefix=LLVM
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=OGCG

int make_int();

int test() {
  const int &x = make_int();
  return x;
}

//      CIR: cir.func {{.*}} @_Z4testv()
//      CIR:   %[[TEMP_SLOT:.*]] = cir.alloca "ref.tmp0" {{.*}} init : !cir.ptr<!s32i>
// CIR-NEXT:   %[[X:.*]] = cir.alloca "x" {{.*}} init const : !cir.ptr<!cir.ptr<!s32i>>
// CIR-NEXT:   %[[TEMP_VALUE:.*]] = cir.call @_Z8make_intv() : () -> (!s32i {llvm.noundef})
// CIR-NEXT:   cir.store{{.*}} %[[TEMP_VALUE]], %[[TEMP_SLOT]]
// CIR-NEXT:   cir.store{{.*}} %[[TEMP_SLOT]], %[[X]]

// LLVM: define {{.*}} i32 @_Z4testv()
// LLVM:   %[[RETVAL:.*]] = alloca i32
// LLVM:   %[[TEMP_SLOT:.*]] = alloca i32
// LLVM:   %[[X:.*]] = alloca ptr
// LLVM:   %[[TEMP_VALUE:.*]] = call noundef i32 @_Z8make_intv()
// LLVM:   store i32 %[[TEMP_VALUE]], ptr %[[TEMP_SLOT]]
// LLVM:   store ptr %[[TEMP_SLOT]], ptr %[[X]]

// OGCG: define {{.*}} i32 @_Z4testv()
// OGCG:   %[[X:.*]] = alloca ptr
// OGCG:   %[[TEMP_SLOT:.*]] = alloca i32
// OGCG:   %[[TEMP_VALUE:.*]] = call noundef i32 @_Z8make_intv()
// OGCG:   store i32 %[[TEMP_VALUE]], ptr %[[TEMP_SLOT]]
// OGCG:   store ptr %[[TEMP_SLOT]], ptr %[[X]]

int test_scoped() {
  int x = make_int();
  {
    const int &y = make_int();
    x = y;
  }
  return x;
}

//      CIR: cir.func {{.*}} @_Z11test_scopedv()
//      CIR:   %[[X:.*]] = cir.alloca "x" {{.*}} init : !cir.ptr<!s32i>
//      CIR:   cir.scope {
// CIR-NEXT:     %[[TEMP_SLOT:.*]] = cir.alloca "ref.tmp0" {{.*}} init : !cir.ptr<!s32i>
// CIR-NEXT:     %[[Y_ADDR:.*]] = cir.alloca "y" {{.*}} init const : !cir.ptr<!cir.ptr<!s32i>>
// CIR-NEXT:     %[[TEMP_VALUE:.*]] = cir.call @_Z8make_intv() : () -> (!s32i {llvm.noundef})
// CIR-NEXT:     cir.store{{.*}} %[[TEMP_VALUE]], %[[TEMP_SLOT]] : !s32i, !cir.ptr<!s32i>
// CIR-NEXT:     cir.store{{.*}} %[[TEMP_SLOT]], %[[Y_ADDR]] : !cir.ptr<!s32i>, !cir.ptr<!cir.ptr<!s32i>>
// CIR-NEXT:     %[[Y_REF:.*]] = cir.load %[[Y_ADDR]] : !cir.ptr<!cir.ptr<!s32i>>, !cir.ptr<!s32i>
// CIR-NEXT:     %[[Y_VALUE:.*]] = cir.load{{.*}} %[[Y_REF]] : !cir.ptr<!s32i>, !s32i
// CIR-NEXT:     cir.store{{.*}} %[[Y_VALUE]], %[[X]] : !s32i, !cir.ptr<!s32i>
// CIR-NEXT:   }

// LLVM: define {{.*}} i32 @_Z11test_scopedv()
// LLVM:   %[[TEMP_SLOT:.*]] = alloca i32
// LLVM:   %[[Y_ADDR:.*]] = alloca ptr
// LLVM:   %[[RETVAL:.*]] = alloca i32
// LLVM:   %[[X:.*]] = alloca i32
// LLVM:   %[[TEMP_VALUE1:.*]] = call noundef i32 @_Z8make_intv()
// LLVM:   store i32 %[[TEMP_VALUE1]], ptr %[[X]]
// LLVM:   br label %[[SCOPE_LABEL:.*]]
// LLVM: [[SCOPE_LABEL]]:
// LLVM:   %[[TEMP_VALUE2:.*]] = call noundef i32 @_Z8make_intv()
// LLVM:   store i32 %[[TEMP_VALUE2]], ptr %[[TEMP_SLOT]]
// LLVM:   store ptr %[[TEMP_SLOT]], ptr %[[Y_ADDR]]
// LLVM:   %[[Y_REF:.*]] = load ptr, ptr %[[Y_ADDR]]
// LLVM:   %[[Y_VALUE:.*]] = load i32, ptr %[[Y_REF]]
// LLVM:   store i32 %[[Y_VALUE]], ptr %[[X]]

// OGCG: define {{.*}} i32 @_Z11test_scopedv()
// OGCG:   %[[X:.*]] = alloca i32
// OGCG:   %[[Y_ADDR:.*]] = alloca ptr
// OGCG:   %[[TEMP_SLOT:.*]] = alloca i32
// OGCG:   %[[TEMP_VALUE1:.*]] = call noundef i32 @_Z8make_intv()
// OGCG:   store i32 %[[TEMP_VALUE1]], ptr %[[X]]
// OGCG:   %[[TEMP_VALUE2:.*]] = call noundef i32 @_Z8make_intv()
// OGCG:   store i32 %[[TEMP_VALUE2]], ptr %[[TEMP_SLOT]]
// OGCG:   store ptr %[[TEMP_SLOT]], ptr %[[Y_ADDR]]
// OGCG:   %[[Y_REF:.*]] = load ptr, ptr %[[Y_ADDR]]
// OGCG:   %[[Y_VALUE:.*]] = load i32, ptr %[[Y_REF]]
// OGCG:   store i32 %[[Y_VALUE]], ptr %[[X]]

enum VulkanStage : unsigned { Vertex };
extern VulkanStage vulkan_stage;
void consume_unsigned(unsigned);

void test_scalar_enum_materialization() {
  consume_unsigned(static_cast<const unsigned &>(vulkan_stage));
}

// CIR-LABEL: cir.func{{.*}} @{{[^ (]*test_scalar_enum_materialization[^ (]*}}()
// CIR: %[[SCALAR_TEMP:.*]] = cir.alloca {{.*}} : !cir.ptr<!u32i>
// CIR: %[[STAGE_ADDR:.*]] = cir.get_global @vulkan_stage : !cir.ptr<!u32i>
// CIR-NEXT: %[[STAGE_VALUE:.*]] = cir.load{{.*}} %[[STAGE_ADDR]] : !cir.ptr<!u32i>, !u32i
// CIR-NEXT: cir.store{{.*}} %[[STAGE_VALUE]], %[[SCALAR_TEMP]] : !u32i, !cir.ptr<!u32i>
// CIR-NEXT: %[[SCALAR_VALUE:.*]] = cir.load{{.*}} %[[SCALAR_TEMP]] : !cir.ptr<!u32i>, !u32i
// CIR-NEXT: cir.call @{{[^ (]*consume_unsigned[^ (]*}}(%[[SCALAR_VALUE]]) : (!u32i{{.*}}) -> ()

// LLVM-LABEL: define {{.*}} void @{{[^ (]*test_scalar_enum_materialization[^ (]*}}()
// LLVM: %[[SCALAR_TEMP:.*]] = alloca i32
// LLVM: %[[STAGE_VALUE:.*]] = load i32, ptr @vulkan_stage
// LLVM: store i32 %[[STAGE_VALUE]], ptr %[[SCALAR_TEMP]]
// LLVM-NEXT: %[[SCALAR_VALUE:.*]] = load i32, ptr %[[SCALAR_TEMP]]
// LLVM-NEXT: call void @{{[^ (]*consume_unsigned[^ (]*}}(i32 {{.*}}%[[SCALAR_VALUE]])

typedef long long BlinkSourceVector __attribute__((vector_size(32)));
typedef signed char BlinkByteVector __attribute__((vector_size(32)));
extern BlinkSourceVector blink_parens;
void consume_byte(signed char);

void test_vector_xvalue_materialization() {
  consume_byte(((BlinkByteVector)blink_parens)[31]);
}

// CIR-LABEL: cir.func{{.*}} @{{[^ (]*test_vector_xvalue_materialization[^ (]*}}()
// CIR: %[[VECTOR_TEMP:.*]] = cir.alloca {{.*}} : !cir.ptr<!cir.vector<32 x !s8i>>
// CIR: %[[PARENS_ADDR:.*]] = cir.get_global @blink_parens : !cir.ptr<!cir.vector<4 x !s64i>>
// CIR-NEXT: %[[PARENS_VALUE:.*]] = cir.load{{.*}} %[[PARENS_ADDR]] : !cir.ptr<!cir.vector<4 x !s64i>>, !cir.vector<4 x !s64i>
// CIR-NEXT: %[[BYTE_VALUE:.*]] = cir.cast bitcast %[[PARENS_VALUE]] : !cir.vector<4 x !s64i> -> !cir.vector<32 x !s8i>
// CIR-NEXT: cir.store{{.*}} %[[BYTE_VALUE]], %[[VECTOR_TEMP]] : !cir.vector<32 x !s8i>, !cir.ptr<!cir.vector<32 x !s8i>>
// CIR-NEXT: %[[VECTOR_VALUE:.*]] = cir.load{{.*}} %[[VECTOR_TEMP]] : !cir.ptr<!cir.vector<32 x !s8i>>, !cir.vector<32 x !s8i>
// CIR-NEXT: %[[INDEX:.*]] = cir.const #cir.int<31> : !s32i
// CIR-NEXT: %[[ELEMENT:.*]] = cir.vec.extract %[[VECTOR_VALUE]][%[[INDEX]] : !s32i]{{.*}} : !cir.vector<32 x !s8i>
// CIR-NEXT: cir.call @{{[^ (]*consume_byte[^ (]*}}(%[[ELEMENT]]) : (!s8i{{.*}}) -> ()

// LLVM-LABEL: define {{.*}} void @{{[^ (]*test_vector_xvalue_materialization[^ (]*}}()
// LLVM: %[[VECTOR_TEMP:.*]] = alloca <32 x i8>
// LLVM: %[[PARENS_VALUE:.*]] = load <4 x i64>, ptr @blink_parens
// LLVM-NEXT: %[[BYTE_VALUE:.*]] = bitcast <4 x i64> %[[PARENS_VALUE]] to <32 x i8>
// LLVM-NEXT: store <32 x i8> %[[BYTE_VALUE]], ptr %[[VECTOR_TEMP]]
// LLVM-NEXT: %[[VECTOR_VALUE:.*]] = load <32 x i8>, ptr %[[VECTOR_TEMP]]
// LLVM-NEXT: %[[ELEMENT:.*]] = extractelement <32 x i8> %[[VECTOR_VALUE]], i32 31
// LLVM-NEXT: call void @{{[^ (]*consume_byte[^ (]*}}(i8 {{.*}}%[[ELEMENT]])
