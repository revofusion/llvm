// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -x objective-c++ -std=c++20 -fobjc-runtime=macosx-13.0.0 -fclangir -emit-cir %s -o - | FileCheck %s

@class NSString;

@interface NSString
+ (id)stringWithFormat:(NSString *)format, ...;
@property(readonly) const char *UTF8String;
@end

@protocol MTLBuffer
@property(copy) NSString *label;
@end

void set_label(id<MTLBuffer> buffer, void *ptr, unsigned long revision) {
  buffer.label = [NSString stringWithFormat:@"BufferMtl=%p(%lu)", ptr, revision];
}

const char *get_utf8(NSString *s) {
  return s.UTF8String;
}

// CHECK: cir.global "private" external @OBJC_CLASS_REFERENCES_NSString : !cir.ptr<!void>
// CHECK: cir.global "private" external @OBJC_SELECTOR_REFERENCES_stringWithFormat_3A : !cir.ptr<!void>
// CHECK: cir.global "private" external @OBJC_STRING_LITERAL_BufferMtl_3D_25p_28_25lu_29 : !cir.ptr<!void>
// CHECK: cir.global "private" external @OBJC_SELECTOR_REFERENCES_setLabel_3A : !cir.ptr<!void>
// CHECK: cir.global "private" external @OBJC_SELECTOR_REFERENCES_UTF8String : !cir.ptr<!void>
// CHECK: cir.func private dso_local @objc_msgSend(!cir.ptr<!void>, !cir.ptr<!void>, ...) -> !cir.ptr<!void>

// CHECK-LABEL: cir.func {{.*}}@_Z9set_label
// CHECK: cir.call @objc_msgSend
// CHECK-SAME: !u64i
// CHECK: cir.call @objc_msgSend
// CHECK-SAME: !cir.ptr<!void>

// CHECK-LABEL: cir.func {{.*}}@_Z8get_utf8P8NSString
// CHECK: cir.call @objc_msgSend
// CHECK: cir.cast bitcast

