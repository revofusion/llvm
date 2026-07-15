// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=FORWARD --input-file=%t.cir %s
// RUN: not %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir -DTEST_INTERFACE_DEFINITION %s -o %t.definition.cir 2>&1 | FileCheck --check-prefix=DEFINITION %s

#ifndef TEST_INTERFACE_DEFINITION
@class NSArray;

int objc_forward_interface_is_type_only(void) { return 17; }

// FORWARD: cir.func{{.*}}@objc_forward_interface_is_type_only
#else
@interface CIRUnsupportedInterface
@end

// DEFINITION: error: ClangIR code gen Not Yet Implemented: declaration of kind: ObjCInterface definition
#endif
