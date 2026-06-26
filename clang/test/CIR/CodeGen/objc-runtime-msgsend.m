// RUN: %clang_cc1 -triple arm64-apple-macosx -fobjc-runtime=macosx-10.15.0 -fclangir -emit-cir %s -o - | FileCheck %s

@interface A
+ (id)new;
- (id)m:(id)x;
@end

id f(id x) { return [[A new] m:x]; }

// CHECK: cir.global{{.*}} @OBJC_CLASS_REFERENCES_A
// CHECK: cir.global{{.*}} @OBJC_SELECTOR_REFERENCES_new
// CHECK: cir.global{{.*}} @OBJC_SELECTOR_REFERENCES_m_3A
// CHECK: cir.func{{.*}} @objc_msgSend
// CHECK: cir.call @objc_msgSend
