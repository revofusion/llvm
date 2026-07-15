// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=FORWARD --input-file=%t.cir %s
// RUN: not %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir -DTEST_INTERFACE_DEFINITION %s -o %t.definition.cir 2>&1 | FileCheck --check-prefix=DEFINITION %s
// RUN: not %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir -DTEST_PROTOCOL_DEFINITION %s -o %t.protocol-definition.cir 2>&1 | FileCheck --check-prefix=PROTOCOL-DEFINITION %s

#if !defined(TEST_INTERFACE_DEFINITION) && !defined(TEST_PROTOCOL_DEFINITION)
@class NSArray;
@protocol CIRForwardProtocol;

int objc_forward_interface_is_type_only(void) { return 17; }
int objc_forward_protocol_is_type_only(void) { return 19; }

// FORWARD-NOT: CIRForwardProtocol
// FORWARD-LABEL: cir.func{{.*}}@objc_forward_interface_is_type_only
// FORWARD-NOT: CIRForwardProtocol
// FORWARD-LABEL: cir.func{{.*}}@objc_forward_protocol_is_type_only
// FORWARD-NOT: CIRForwardProtocol
#elif defined(TEST_INTERFACE_DEFINITION)
@interface CIRUnsupportedInterface
@end

// DEFINITION: error: ClangIR code gen Not Yet Implemented: declaration of kind: ObjCInterface definition
#else
@protocol CIRUnsupportedProtocol
@end

// PROTOCOL-DEFINITION: error: ClangIR code gen Not Yet Implemented: declaration of kind: ObjCProtocol definition
#endif
