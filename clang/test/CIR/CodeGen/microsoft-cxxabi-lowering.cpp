// RUN: %clang_cc1 -triple x86_64-pc-windows-msvc -std=c++14 -fclangir -emit-cir %s -o - | FileCheck %s

namespace std {
class type_info;
}

extern void sink(const char *, ...);

void vararg_null() { sink("%p", 0); }

struct Logger {
  Logger(const char *, const char *, int, int);
};
void construct_logger() { Logger("a", "b", 2, 4); }

const std::type_info *typeid_global = &typeid(int);
const std::type_info *typeid_function() { return &typeid(long); }

struct A {
  virtual int f();
};
int virtual_call(A *a) { return a->f(); }

struct V {
  virtual int f();
};
struct WithVirtualBase : virtual V {};
int virtual_base_call(WithVirtualBase *a) { return a->f(); }

struct Deleting {
  virtual ~Deleting();
};
void delete_virtual(Deleting *p, Deleting **array) {
  delete p;
  delete[] array;
}

// CHECK: cir.triple = "x86_64-pc-windows-msvc"
// CHECK-DAG: cir.global "private" external dso_local @"??_R0H@8"
// CHECK-DAG: cir.global external dso_local @"?typeid_global@@3PEBVtype_info@std@@EB" = #cir.global_view<@"??_R0H@8"> : !cir.ptr<!rec_std3A3Atype_info>

// CHECK-LABEL: cir.func no_inline dso_local @"?vararg_null@@YAXXZ"
// CHECK: %[[ZERO:[0-9]+]] = cir.const #cir.int<0> : !s64i
// CHECK: cir.call @"?sink@@YAXPEBDZZ"(%{{[0-9]+}}, %[[ZERO]]) : (!cir.ptr<!s8i>, !s64i) -> ()

// CHECK-LABEL: cir.func no_inline dso_local @"?construct_logger@@YAXXZ"
// CHECK: %[[FOUR:[0-9]+]] = cir.const #cir.int<4> : !s32i
// CHECK: %[[TWO:[0-9]+]] = cir.const #cir.int<2> : !s32i
// CHECK: %[[B_STR:[0-9]+]] = cir.cast array_to_ptrdecay
// CHECK: %[[A_STR:[0-9]+]] = cir.cast array_to_ptrdecay
// CHECK: cir.call @"??0Logger@@QEAA@PEBD0HH@Z"(%{{[0-9]+}}, %[[A_STR]], %[[B_STR]], %[[TWO]], %[[FOUR]])

// CHECK-LABEL: cir.func no_inline dso_local @"?typeid_function@@YAPEBVtype_info@std@@XZ"
// CHECK: %[[LONG_RTTI:[0-9]+]] = cir.get_global @"??_R0J@8" : !cir.ptr<!u8i>
// CHECK: cir.cast bitcast %[[LONG_RTTI]] : !cir.ptr<!u8i> -> !cir.ptr<!rec_std3A3Atype_info>

// CHECK-LABEL: cir.func no_inline dso_local @"?virtual_call@@YAHPEAUA@@@Z"
// CHECK: cir.vtable.get_vptr
// CHECK: cir.vtable.get_virtual_fn_addr {{.*}}[0]
// CHECK: cir.call %{{[0-9]+}}(%{{[0-9]+}})

// CHECK-LABEL: cir.func no_inline dso_local @"?virtual_base_call@@YAHPEAUWithVirtualBase@@@Z"
// CHECK: cir.ptr_stride
// CHECK: cir.load align(4) {{.*}} : !cir.ptr<!s32i>, !s32i
// CHECK: cir.cast integral {{.*}} : !s32i -> !s64i
// CHECK: cir.vtable.get_virtual_fn_addr {{.*}}[0]

// CHECK-LABEL: cir.func no_inline dso_local @"?delete_virtual@@YAXPEAUDeleting@@PEAPEAU1@@Z"
// CHECK: %[[DELETE_FLAG:[0-9]+]] = cir.const #cir.int<1> : !s32i
// CHECK: cir.vtable.get_virtual_fn_addr {{.*}}[0]
// CHECK: cir.call %{{[0-9]+}}(%{{[0-9]+}}, %[[DELETE_FLAG]]) nothrow
// CHECK: cir.call @"??_V@YAXPEAX@Z"
