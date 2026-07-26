// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -mmlir -mlir-print-ir-before=cir-cxxabi-lowering %s -o %t.cir 2> %t-before.cir
// RUN: FileCheck --check-prefix=CIR-BEFORE --input-file=%t-before.cir %s
// RUN: FileCheck --check-prefixes=CIR-AFTER,CIR-AFTER-X86 --input-file=%t.cir %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll --check-prefixes=LLVM,LLVM-X86 %s
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -emit-llvm %s -o %t.ll
// RUN: FileCheck --check-prefixes=OGCG,OGCG-X86 --input-file=%t.ll %s

// RUN: %clang_cc1 -triple aarch64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir -mmlir -mlir-print-ir-before=cir-cxxabi-lowering %s -o %t-arm.cir 2> %t-arm-before.cir
// RUN: FileCheck --check-prefix=CIR-BEFORE --input-file=%t-arm-before.cir %s
// RUN: FileCheck --check-prefixes=CIR-AFTER,CIR-AFTER-ARM --input-file=%t-arm.cir %s
// RUN: %clang_cc1 -triple aarch64-unknown-linux-gnu -std=c++17 -fclangir -emit-llvm %s -o %t-arm-cir.ll
// RUN: FileCheck --input-file=%t-arm-cir.ll --check-prefixes=LLVM,LLVM-ARM %s
// RUN: %clang_cc1 -triple aarch64-unknown-linux-gnu -std=c++17 -emit-llvm %s -o %t-arm.ll
// RUN: FileCheck --check-prefixes=OGCG,OGCG-ARM --input-file=%t-arm.ll %s

// RUN: %clang_cc1 -triple arm64-apple-macosx -std=c++17 -fclangir -emit-cir %s -o %t-apple-arm.cir
// RUN: FileCheck --check-prefixes=CIR-AFTER,CIR-AFTER-APPLE --input-file=%t-apple-arm.cir %s

// FIXME: Some of the differences between LLVM (via CIR) and OGCG below are
//        due to calling convention lowering being missing in the CIR path.

struct Foo {
  void m1(int);
  virtual void m2(int);
  virtual void m3(int);
};

// Global pointer to non-virtual method
void (Foo::*m1_ptr)(int) = &Foo::m1;

// CIR-BEFORE: cir.global external @m1_ptr = #cir.method<@_ZN3Foo2m1Ei> : !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>
// CIR-AFTER-DAG:     cir.global "private" constant cir_private @[[NONVIRT_RET:.*]] = #cir.const_record<{#cir.global_view<@_ZN3Foo2m1Ei>, #cir.int<0>}> : !rec_anon_struct
// CIR-AFTER-X86-DAG: cir.global "private" constant cir_private @[[VIRT_RET:.*]] = #cir.const_record<{#cir.int<9>, #cir.int<0>}> : !rec_anon_struct
// CIR-AFTER-ARM-DAG: cir.global "private" constant cir_private @[[VIRT_RET:.*]] = #cir.const_record<{#cir.int<8>, #cir.int<1>}> : !rec_anon_struct
// CIR-AFTER-APPLE-DAG: cir.global "private" constant cir_private @[[VIRT_RET:.*]] = #cir.const_record<{#cir.int<8>, #cir.int<1>}> : !rec_anon_struct
// CIR-AFTER-DAG:     cir.global "private" constant cir_private @[[NULL_RET:.*]] = #cir.const_record<{#cir.int<0>, #cir.int<0>}>
// CIR-AFTER:         cir.global external @m1_ptr = #cir.const_record<{#cir.global_view<@_ZN3Foo2m1Ei>, #cir.int<0>}> : !rec_anon_struct
// LLVM-DAG:     @m1_ptr = global { i64, i64 } { i64 ptrtoint (ptr @_ZN3Foo2m1Ei to i64), i64 0 }
// LLVM-DAG:     @[[NONVIRT_RET:.*]] = private constant { i64, i64 } { i64 ptrtoint (ptr @_ZN3Foo2m1Ei to i64), i64 0 }
// LLVM-X86-DAG: @[[VIRT_RET:.*]] = private constant { i64, i64 } { i64 9, i64 0 }
// LLVM-ARM-DAG: @[[VIRT_RET:.*]] = private constant { i64, i64 } { i64 8, i64 1 }
// LLVM-DAG:     @[[NULL_RET:.*]] = private constant { i64, i64 } zeroinitializer
// OGCG: @m1_ptr = global { i64, i64 } { i64 ptrtoint (ptr @_ZN3Foo2m1Ei to i64), i64 0 }

// Global pointer to virtual method
void (Foo::*m2_ptr)(int) = &Foo::m2;

// CIR-BEFORE: cir.global external @m2_ptr = #cir.method<vtable_offset = 0> : !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>
// CIR-AFTER-X86: cir.global external @m2_ptr = #cir.const_record<{#cir.int<1>, #cir.int<0>}> : !rec_anon_struct
// CIR-AFTER-ARM: cir.global external @m2_ptr = #cir.const_record<{#cir.int<0>, #cir.int<1>}> : !rec_anon_struct
// LLVM-X86-DAG: @m2_ptr = global { i64, i64 } { i64 1, i64 0 }
// LLVM-ARM-DAG: @m2_ptr = global { i64, i64 } { i64 0, i64 1 }
// OGCG-X86: @m2_ptr = global { i64, i64 } { i64 1, i64 0 }
// OGCG-ARM: @m2_ptr = global { i64, i64 } { i64 0, i64 1 }

// Self-referencing PMF causes a null method.
long (Foo::*pmf1)(int) = pmf1;
// CIR-BEFORE: @pmf1 = ctor : !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i) -> !s64i> in !rec_Foo> {
// CIR-AFTER: cir.global external @pmf1 = #cir.const_record<{#cir.int<0>, #cir.int<0>}>
// LLVM: @pmf1 = global { i64, i64 } zeroinitializer, align 8 
// OGCG: @pmf1 = global { i64, i64 } zeroinitializer, align 8 

auto make_non_virtual() -> void (Foo::*)(int) {
  return &Foo::m1;
}

// CIR-BEFORE: cir.func {{.*}} @_Z16make_non_virtualv() -> (!cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo> {{.*}})
// CIR-BEFORE:   %{{.*}} = cir.alloca "__retval" {{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>>
// CIR-BEFORE:   %{{.*}} = cir.const #cir.method<@_ZN3Foo2m1Ei> : !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>
// CIR-BEFORE:   cir.store %{{.*}}, %{{.*}}
// CIR-BEFORE:   %{{.*}} = cir.load %{{.*}}
// CIR-BEFORE:   cir.return %{{.*}} : !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>

// CIR-AFTER: cir.func {{.*}} @_Z16make_non_virtualv() -> (!rec_anon_struct {cir.ast_member_pointer = {{.*}}}) attributes
// CIR-AFTER:   %{{.*}} = cir.alloca "__retval" {{.*}} : !cir.ptr<!rec_anon_struct>
// CIR-AFTER:   %{{.*}} = cir.get_global @__const._Z16make_non_virtualv.__retval : !cir.ptr<!rec_anon_struct>
// CIR-AFTER:   cir.copy %{{.*}} to %{{.*}} : !cir.ptr<!rec_anon_struct>
// CIR-AFTER:   %{{.*}} = cir.load %{{.*}}
// CIR-AFTER:   cir.return %{{.*}} : !rec_anon_struct

// LLVM: define {{.*}} { i64, i64 } @_Z16make_non_virtualv()
// LLVM:   %{{.*}} = alloca { i64, i64 }
// LLVM:   call void @llvm.memcpy{{.*}}(ptr %{{.*}}, ptr @[[NONVIRT_RET]]
// LLVM:   %{{.*}} = load { i64, i64 }, ptr %{{.*}}
// LLVM:   ret { i64, i64 } %{{.*}}

// OGCG-X86: define {{.*}} { i64, i64 } @_Z16make_non_virtualv()
// OGCG-X86:   ret { i64, i64 } { i64 ptrtoint (ptr @_ZN3Foo2m1Ei to i64), i64 0 }

// OGCG-ARM: define {{.*}} [2 x i64] @_Z16make_non_virtualv()
// OGCG-ARM:   %{{.*}} = alloca { i64, i64 }
// OGCG-ARM:   store { i64, i64 } { i64 ptrtoint (ptr @_ZN3Foo2m1Ei to i64), i64 0 }, ptr %{{.*}}
// OGCG-ARM:   %{{.*}} = load [2 x i64], ptr %{{.*}}
// OGCG-ARM:   ret [2 x i64] %{{.*}}

auto make_virtual() -> void (Foo::*)(int) {
  return &Foo::m3;
}

// CIR-BEFORE: cir.func {{.*}} @_Z12make_virtualv() -> (!cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo> {{.*}})
// CIR-BEFORE:   %{{.*}} = cir.alloca "__retval" {{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>>
// CIR-BEFORE:   %{{.*}} = cir.const #cir.method<vtable_offset = 8> : !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>
// CIR-BEFORE:   cir.store %{{.*}}, %{{.*}}
// CIR-BEFORE:   %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>>, !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>
// CIR-BEFORE:   cir.return %{{.*}} : !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>

// CIR-AFTER: cir.func {{.*}} @_Z12make_virtualv() -> (!rec_anon_struct {cir.ast_member_pointer = {{.*}}}) attributes
// CIR-AFTER:   %{{.*}} = cir.alloca "__retval" {{.*}} : !cir.ptr<!rec_anon_struct>
// CIR-AFTER:   %{{.*}} = cir.get_global @__const._Z12make_virtualv.__retval : !cir.ptr<!rec_anon_struct>
// CIR-AFTER:   cir.copy %{{.*}} to %{{.*}} : !cir.ptr<!rec_anon_struct>
// CIR-AFTER:   %{{.*}} = cir.load %{{.*}}
// CIR-AFTER:   cir.return %{{.*}} : !rec_anon_struct

// LLVM: define {{.*}} @_Z12make_virtualv()
// LLVM:   %{{.*}} = alloca { i64, i64 }
// LLVM:   call void @llvm.memcpy{{.*}}(ptr %{{.*}}, ptr @[[VIRT_RET]]
// LLVM:   %{{.*}} = load { i64, i64 }, ptr %{{.*}}
// LLVM:   ret { i64, i64 } %{{.*}}

// OGCG:     define {{.*}} @_Z12make_virtualv()
// OGCG-X86:   ret { i64, i64 } { i64 9, i64 0 }
// OGCG-ARM:   %{{.*}} = alloca { i64, i64 }
// OGCG-ARM:   store { i64, i64 } { i64 8, i64 1 }, ptr %{{.*}}
// OGCG-ARM:   %{{.*}} = load [2 x i64], ptr %{{.*}}
// OGCG-ARM:   ret [2 x i64] %{{.*}}

auto make_null() -> void (Foo::*)(int) {
  return nullptr;
}

// CIR-BEFORE: cir.func {{.*}} @_Z9make_nullv() -> (!cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo> {{.*}})
// CIR-BEFORE:   %{{.*}} = cir.alloca "__retval" {{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>>
// CIR-BEFORE:   %{{.*}} = cir.const #cir.method<null> : !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>
// CIR-BEFORE:   cir.store %{{.*}}, %{{.*}}
// CIR-BEFORE:   %{{.*}} = cir.load %{{.*}}
// CIR-BEFORE:   cir.return %{{.*}} : !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>

// CIR-AFTER: cir.func {{.*}} @_Z9make_nullv() -> (!rec_anon_struct {cir.ast_member_pointer = {{.*}}}) attributes
// CIR-AFTER:   %{{.*}} = cir.alloca "__retval" {{.*}} : !cir.ptr<!rec_anon_struct>
// CIR-AFTER:   %{{.*}} = cir.get_global @[[NULL_RET]] : !cir.ptr<!rec_anon_struct>
// CIR-AFTER:   cir.copy %{{.*}} to %{{.*}} : !cir.ptr<!rec_anon_struct>
// CIR-AFTER:   %{{.*}} = cir.load %{{.*}}
// CIR-AFTER:   cir.return %{{.*}} : !rec_anon_struct

// LLVM: define {{.*}} @_Z9make_nullv()
// LLVM:   %{{.*}} = alloca { i64, i64 }
// LLVM:   call void @llvm.memcpy{{.*}}(ptr %{{.*}}, ptr @[[NULL_RET]]
// LLVM:   %{{.*}} = load { i64, i64 }, ptr %{{.*}}
// LLVM:   ret { i64, i64 } %{{.*}}

// OGCG:     define {{.*}} @_Z9make_nullv()
// OGCG-X86:   ret { i64, i64 } zeroinitializer
// OGCG-ARM:   %{{.*}} = alloca { i64, i64 }
// OGCG-ARM:   store { i64, i64 } zeroinitializer, ptr %{{.*}}
// OGCG-ARM:   %{{.*}} = load [2 x i64], ptr %{{.*}}
// OGCG-ARM:   ret [2 x i64] %{{.*}}

void call(Foo *obj, void (Foo::*func)(int), int arg) {
  (obj->*func)(arg);
}

// CIR-BEFORE: cir.func {{.*}} @_Z4callP3FooMS_FviEi
// CIR-BEFORE:   %{{.*}} = cir.load{{.*}} %{{.*}} : !cir.ptr<!cir.ptr<!rec_Foo>>, !cir.ptr<!rec_Foo>
// CIR-BEFORE:   %{{.*}} = cir.load{{.*}} : !cir.ptr<!cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>>, !cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>
// CIR-BEFORE:   %{{.*}}, %{{.*}} = cir.get_method %{{.*}}, %{{.*}} : (!cir.method<!cir.func<(!cir.ptr<!rec_Foo>, !s32i)> in !rec_Foo>, !cir.ptr<!rec_Foo>) -> (!cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>, !cir.ptr<!void>)
// CIR-BEFORE:   %{{.*}} = cir.load{{.*}} %{{.*}} : !cir.ptr<!s32i>, !s32i
// CIR-BEFORE:   cir.call %{{.*}}(%{{.*}}, %{{.*}}) : (!cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>, !cir.ptr<!void> {{.*}}, !s32i {{.*}}) -> ()

// CIR-AFTER:    cir.func {{.*}} @_Z4callP3FooMS_FviEi
// CIR-AFTER-SAME: {{.*}}cir.ast_member_pointer = {{.*}}
// CIR-AFTER:      %{{.*}} = cir.load{{.*}} %{{.*}} : !cir.ptr<!cir.ptr<!rec_Foo>>, !cir.ptr<!rec_Foo>
// CIR-AFTER:      %{{.*}} = cir.load{{.*}} : !cir.ptr<!rec_anon_struct>, !rec_anon_struct
// CIR-AFTER:      %{{.*}} = cir.const #cir.int<1> : !s64i
// CIR-AFTER:      %{{.*}} = cir.const #cir.int<1> : !u64i
// CIR-AFTER:      %{{.*}} = cir.extract_member %{{.*}}[1] : !rec_anon_struct -> !s64i
// CIR-AFTER-ARM:  %{{.*}} = cir.shift(right, %{{.*}} : !s64i, %{{.*}} : !s64i) -> !s64i
// CIR-AFTER-APPLE: %{{.*}} = cir.shift(right, %{{.*}} : !s64i, %{{.*}} : !s64i) -> !s64i
// CIR-AFTER:      %{{.*}} = cir.cast bitcast %{{.*}} : !cir.ptr<!rec_Foo> -> !cir.ptr<!void>
// CIR-AFTER-X86:  %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!void>, !s64i) -> !cir.ptr<!void>
// CIR-AFTER-ARM:  %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!void>, !s64i) -> !cir.ptr<!void>
// CIR-AFTER-APPLE: %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!void>, !s64i) -> !cir.ptr<!void>
// CIR-AFTER:      %{{.*}} = cir.extract_member %{{.*}}[0] : !rec_anon_struct -> !u64i
// CIR-AFTER,CIR-AFTER-X86:  %{{.*}} = cir.and %{{.*}}, %{{.*}} : !u64i
// CIR-AFTER,CIR-AFTER-ARM:  %{{.*}} = cir.and %{{.*}}, %{{.*}} : !s64i
// CIR-AFTER,CIR-AFTER-APPLE: %{{.*}} = cir.and %{{.*}}, %{{.*}} : !s64i
// CIR-AFTER,CIR-AFTER-X86:  %{{.*}} = cir.cmp eq %{{.*}}, %{{.*}} : !u64i
// CIR-AFTER,CIR-AFTER-ARM:  %{{.*}} = cir.cmp eq %{{.*}}, %{{.*}} : !s64i
// CIR-AFTER,CIR-AFTER-APPLE: %{{.*}} = cir.cmp eq %{{.*}}, %{{.*}} : !s64i
// CIR-AFTER:      %{{.*}} = cir.ternary(%{{.*}}, true {
// CIR-AFTER:        %{{.*}} = cir.cast bitcast %{{.*}} : !cir.ptr<!void> -> !cir.ptr<!cir.ptr<!s8i>>
// CIR-AFTER:        %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!s8i>>, !cir.ptr<!s8i>
// CIR-AFTER-X86:    %{{.*}} = cir.sub %{{.*}}, %{{.*}} : !u64i
// CIR-AFTER-X86:    %{{.*}} = cir.cast integral %{{.*}} : !u64i -> !s64i
// CIR-AFTER-X86:    %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!s8i>, !s64i) -> !cir.ptr<!s8i>
// CIR-AFTER-ARM:    %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!s8i>, !u64i) -> !cir.ptr<!s8i>
// CIR-AFTER-APPLE: %{{.*}} = cir.cast integral %{{.*}} : !u64i -> !u32i
// CIR-AFTER-APPLE: %{{.*}} = cir.cast integral %{{.*}} : !u32i -> !s64i
// CIR-AFTER-APPLE: %{{.*}} = cir.ptr_stride %{{.*}}, %{{.*}} : (!cir.ptr<!s8i>, !s64i) -> !cir.ptr<!s8i>
// CIR-AFTER:        %{{.*}} = cir.cast bitcast %{{.*}} : !cir.ptr<!s8i> -> !cir.ptr<!cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>>
// CIR-AFTER:        %{{.*}} = cir.load %{{.*}} : !cir.ptr<!cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>>, !cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>
// CIR-AFTER:        cir.yield %{{.*}} : !cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>
// CIR-AFTER:      }, false {
// CIR-AFTER:        %{{.*}} = cir.cast int_to_ptr %{{.*}} : !u64i -> !cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>
// CIR-AFTER:        cir.yield %{{.*}} : !cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>
// CIR-AFTER:      }) : (!cir.bool) -> !cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>
// CIR-AFTER:      %{{.*}} = cir.load{{.*}} %{{.*}} : !cir.ptr<!s32i>, !s32i
// CIR-AFTER:      cir.call %{{.*}}(%{{.*}}, %{{.*}}) : (!cir.ptr<!cir.func<(!cir.ptr<!void>, !s32i)>>, !cir.ptr<!void> {{.*}}, !s32i {{.*}}) -> ()

// LLVM:     define {{.*}} @_Z4callP3FooMS_FviEi
// LLVM:       %{{.*}} = load ptr, ptr %{{.*}}
// LLVM:       %{{.*}} = load { i64, i64 }, ptr %{{.*}}
// LLVM:       %{{.*}} = extractvalue { i64, i64 } %{{.*}}, 1
// LLVM-X86:   %{{.*}} = getelementptr i8, ptr %{{.*}}, i64 %{{.*}}
// LLVM-ARM:   %{{.*}} = ashr i64 %{{.*}}, 1
// LLVM-ARM:   %{{.*}} = getelementptr i8, ptr %{{.*}}, i64 %{{.*}}
// LLVM:       %{{.*}} = extractvalue { i64, i64 } %{{.*}}, 0
// LLVM-ARM:   %{{.*}} = and i64 %{{.*}}, 1
// LLVM-X86:   %{{.*}} = and i64 %{{.*}}, 1
// LLVM:       %{{.*}} = icmp eq i64 %{{.*}}, 1
// LLVM:       br i1 %{{.*}}, label %[[HANDLE_VIRTUAL:[0-9]+]], label %[[HANDLE_NON_VIRTUAL:[0-9]+]]
// LLVM:     [[HANDLE_VIRTUAL]]:
// LLVM:       %{{.*}} = load ptr, ptr %{{.*}}
// LLVM-X86:   %{{.*}} = sub i64 %{{.*}}, 1
// LLVM-X86:   %{{.*}} = getelementptr i8, ptr %{{.*}}, i64 %{{.*}}
// LLVM-ARM:   %{{.*}} = getelementptr i8, ptr %{{.*}}, i64 %{{.*}}
// LLVM:       %{{.*}} = load ptr, ptr %{{.*}}
// LLVM:       br label %[[CONTINUE:[0-9]+]]
// LLVM:     [[HANDLE_NON_VIRTUAL]]:
// LLVM:       %{{.*}} = inttoptr i64 %{{.*}} to ptr
// LLVM:       br label %[[CONTINUE]]
// LLVM:     [[CONTINUE]]:
// LLVM:       %{{.*}} = phi ptr [ %{{.*}}, %{{.*}} ], [ %{{.*}}, %{{.*}} ]
// LLVM:       %{{.*}} = load i32, ptr %{{.+}}
// LLVM:       call void %{{.*}}(ptr {{.*}} %{{.*}}, i32 {{.*}} %{{.*}})
// LLVM:     }

// OGCG:     define {{.*}} @_Z4callP3FooMS_FviEi
// OGCG:       %{{.*}} = load ptr, ptr %{{.*}}
// OGCG:       %{{.*}} = load { i64, i64 }, ptr %{{.*}}
// OGCG:       %{{.*}} = extractvalue { i64, i64 } %{{.*}}, 1
// OGCG-X86:   %{{.*}} = getelementptr inbounds i8, ptr %{{.*}}, i64 %{{.*}}
// OGCG-ARM:   %{{.*}} = ashr i64 %{{.*}}, 1
// OGCG-ARM:   %{{.*}} = getelementptr inbounds i8, ptr %{{.*}}, i64 %{{.*}}
// OGCG:       %{{.*}} = extractvalue { i64, i64 } %{{.*}}, 0
// OGCG-X86:   %{{.*}} = and i64 %{{.*}}, 1
// OGCG-ARM:   %{{.*}} = and i64 %{{.*}}, 1
// OGCG:       %{{.*}} = icmp ne i64 %{{.*}}, 0
// OGCG:       br i1 %{{.*}}, label %[[HANDLE_VIRTUAL:[a-zA-Z0-9._]+]], label %[[HANDLE_NON_VIRTUAL:[a-zA-Z0-9._]+]]
// OGCG:     [[HANDLE_VIRTUAL]]:
// OGCG:       %{{.*}} = load ptr, ptr %{{.*}}
// OGCG-X86:   %{{.*}} = sub i64 %{{.*}}, 1
// OGCG-X86:   %{{.*}} = getelementptr i8, ptr %{{.*}}, i64 %{{.*}}
// OGCG-ARM:   %{{.*}} = getelementptr i8, ptr %{{.*}}, i64 %{{.*}}
// OGCG:       %{{.*}} = load ptr, ptr %{{.*}}
// OGCG:       br label %[[CONTINUE:[a-zA-Z0-9._]+]]
// OGCG:     [[HANDLE_NON_VIRTUAL]]:
// OGCG:       %{{.*}} = inttoptr i64 %{{.*}} to ptr
// OGCG:       br label %[[CONTINUE]]
// OGCG:     [[CONTINUE]]:
// OGCG:       %{{.*}} = phi ptr [ %{{.*}}, %{{.*}} ], [ %{{.*}}, %{{.*}} ]
// OGCG:       %{{.*}} = load i32, ptr %{{.+}}
// OGCG:       call void %{{.*}}(ptr {{.*}} %{{.*}}, i32 {{.*}} %{{.*}})
// OGCG:     }
