// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fexceptions -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

@class CIRPointerInterface;
@protocol CIRPointerProtocol;

CIRPointerInterface * _Nullable
objc_interface_pointer(CIRPointerInterface * _Nullable value) {
  return value;
}

id<CIRPointerProtocol> _Nonnull
objc_protocol_pointer(id<CIRPointerProtocol> _Nonnull value) {
  return value;
}

// CHECK-LABEL: cir.func{{.*}} @objc_interface_pointer(%{{.*}}: !cir.ptr<!void>{{.*}}) -> !cir.ptr<!void>
// CHECK-SAME: ast_param_source_types = {{.*}}kind = "objc_object_pointer"{{.*}}objc_interface_usr = "c:objc(cs)CIRPointerInterface"{{.*}}objc_nullability = "_Nullable"{{.*}}ast_return_source_type = {{.*}}kind = "objc_object_pointer"{{.*}}objc_interface_usr = "c:objc(cs)CIRPointerInterface"{{.*}}objc_nullability = "_Nullable"
// CHECK-LABEL: cir.func{{.*}} @objc_protocol_pointer(%{{.*}}: !cir.ptr<!void>{{.*}}) -> !cir.ptr<!void>
// CHECK-SAME: ast_param_source_types = {{.*}}kind = "objc_object_pointer"{{.*}}objc_nullability = "_Nonnull"{{.*}}objc_protocol_qualifiers = ["c:objc(pl)CIRPointerProtocol"]{{.*}}ast_return_source_type = {{.*}}kind = "objc_object_pointer"{{.*}}objc_nullability = "_Nonnull"{{.*}}objc_protocol_qualifiers = ["c:objc(pl)CIRPointerProtocol"]

void may_throw(void);

void objc_pool_normal(void) {
  @autoreleasepool {
    may_throw();
  }
}

int objc_pool_early_return(int flag) {
  @autoreleasepool {
    if (flag)
      return 7;
  }
  return 0;
}

// CHECK-LABEL: cir.func{{.*}} @objc_pool_normal()
// CHECK: %[[NORMAL_POOL:.*]] = cir.call @objc_autoreleasePoolPush(){{.*}} -> !cir.ptr<!void>
// CHECK: cir.cleanup.scope {
// CHECK: cir.call @may_throw(){{.*}} -> ()
// CHECK: cir.yield
// CHECK: } cleanup all {
// CHECK: cir.call @objc_autoreleasePoolPop(%[[NORMAL_POOL]]){{.*}} -> ()
// CHECK: cir.yield
// CHECK: }
// CHECK: cir.return
// CHECK-LABEL: cir.func{{.*}} @objc_pool_early_return
// CHECK: %[[EARLY_POOL:.*]] = cir.call @objc_autoreleasePoolPush(){{.*}} -> !cir.ptr<!void>
// CHECK: cir.cleanup.scope {
// CHECK: cir.if
// CHECK: cir.return
// CHECK: } cleanup all {
// CHECK: cir.call @objc_autoreleasePoolPop(%[[EARLY_POOL]]){{.*}} -> ()
// CHECK: cir.yield
// CHECK: }
// CHECK: cir.return
