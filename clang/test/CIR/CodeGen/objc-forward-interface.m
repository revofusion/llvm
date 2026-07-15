// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --check-prefix=FORWARD --input-file=%t.cir %s
// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir -DTEST_INTERFACE_DEFINITION %s -o %t.interface-definition.cir
// RUN: FileCheck --check-prefix=INTERFACE-DEFINITION --input-file=%t.interface-definition.cir %s
// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir -DTEST_PROTOCOL_DEFINITION %s -o %t.protocol-definition.cir
// RUN: FileCheck --check-prefix=PROTOCOL-DEFINITION --input-file=%t.protocol-definition.cir %s
// RUN: %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir -DTEST_CATEGORY_DECLARATION %s -o %t.category.cir
// RUN: FileCheck --check-prefix=CATEGORY --input-file=%t.category.cir %s
// RUN: not %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir -DTEST_IMPLEMENTATION_BODY %s -o %t.implementation.cir 2>&1 | FileCheck --check-prefix=IMPLEMENTATION %s
// RUN: not %clang_cc1 -triple arm64-apple-macosx13.0.0 -fobjc-runtime=macosx-13.0 -fclangir -emit-cir -DTEST_CATEGORY_IMPLEMENTATION_BODY %s -o %t.category-implementation.cir 2>&1 | FileCheck --check-prefix=CATEGORY-IMPLEMENTATION %s

#if !defined(TEST_INTERFACE_DEFINITION) && !defined(TEST_PROTOCOL_DEFINITION) && !defined(TEST_CATEGORY_DECLARATION) && !defined(TEST_IMPLEMENTATION_BODY) && !defined(TEST_CATEGORY_IMPLEMENTATION_BODY)
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
@protocol CIRInterfaceProtocol;

@interface CIRInterfaceBase
@end

@interface CIRDefinedInterface : CIRInterfaceBase <CIRInterfaceProtocol> {
@public
  int field;
}
@property(nonatomic, copy) id value;
- (id)method:(id)value;
@end

int objc_interface_definition_is_metadata_only(void) { return 29; }

// INTERFACE-DEFINITION: cir.objc_interfaces = [
// INTERFACE-DEFINITION-DAG: "c:objc(cs)CIRDefinedInterface"
// INTERFACE-DEFINITION-DAG: runtime_name = "CIRDefinedInterface"
// INTERFACE-DEFINITION-DAG: "c:objc(cs)CIRInterfaceBase"
// INTERFACE-DEFINITION-DAG: "c:objc(pl)CIRInterfaceProtocol"
// INTERFACE-DEFINITION-DAG: "c:objc(cs)CIRDefinedInterface(im)method:"
// INTERFACE-DEFINITION-DAG: "c:objc(cs)CIRDefinedInterface(py)value"
// INTERFACE-DEFINITION-DAG: "c:objc(cs)CIRDefinedInterface@field"
// INTERFACE-DEFINITION-DAG: properties = [
// INTERFACE-DEFINITION-DAG: ivars = [
// INTERFACE-DEFINITION-DAG: getter_selector = "value"
// INTERFACE-DEFINITION-DAG: setter_selector = "setValue:"
// INTERFACE-DEFINITION-DAG: type = {
// INTERFACE-DEFINITION-DAG: type = {
// INTERFACE-DEFINITION-DAG: offset_bits =
// INTERFACE-DEFINITION-DAG: layout_size_bytes =
// INTERFACE-DEFINITION-DAG: layout_align_bytes =
// INTERFACE-DEFINITION-NOT: cir.global
// INTERFACE-DEFINITION-LABEL: cir.func{{.*}}@objc_interface_definition_is_metadata_only
#elif defined(TEST_PROTOCOL_DEFINITION)
@protocol CIRProtocolBase;
@protocol CIRDefinedProtocol <CIRProtocolBase>
@required
- (id)requiredObject:(id)value;
@optional
+ (void)optionalMethod;
@end

int objc_protocol_definition_is_metadata_only(void) { return 23; }

// PROTOCOL-DEFINITION: cir.objc_protocols = [
// PROTOCOL-DEFINITION-DAG: inherited_protocol_usrs = [
// PROTOCOL-DEFINITION-DAG: "c:objc(pl)CIRProtocolBase"
// PROTOCOL-DEFINITION-DAG: selector = "requiredObject:"
// PROTOCOL-DEFINITION-DAG: selector = "optionalMethod"
// PROTOCOL-DEFINITION-DAG: "c:objc(pl)CIRDefinedProtocol(im)requiredObject:"
// PROTOCOL-DEFINITION-DAG: "c:objc(pl)CIRDefinedProtocol(cm)optionalMethod"
// PROTOCOL-DEFINITION-DAG: is_optional = false
// PROTOCOL-DEFINITION-DAG: is_optional = true
// PROTOCOL-DEFINITION-DAG: parameter_types = [
// PROTOCOL-DEFINITION-DAG: return_type = {
// PROTOCOL-DEFINITION-NOT: cir.global
// PROTOCOL-DEFINITION-LABEL: cir.func{{.*}}@objc_protocol_definition_is_metadata_only
#elif defined(TEST_CATEGORY_DECLARATION)
@interface CIRCategoryTarget
@end

@protocol CIRCategoryProtocol;
@interface CIRCategoryTarget (CIRMetadataCategory) <CIRCategoryProtocol>
@property(nonatomic) int count;
- (id)categoryMethod:(id)value;
@end

int objc_category_declaration_is_metadata_only(void) { return 31; }

// CATEGORY: cir.objc_categories = [
// CATEGORY-DAG: "c:objc(cy)CIRCategoryTarget@CIRMetadataCategory"
// CATEGORY-DAG: name = "CIRMetadataCategory"
// CATEGORY-DAG: "c:objc(cs)CIRCategoryTarget"
// CATEGORY-DAG: "c:objc(pl)CIRCategoryProtocol"
// CATEGORY-DAG: selector = "categoryMethod:"
// CATEGORY-DAG: getter_selector = "count"
// CATEGORY-DAG: setter_selector = "setCount:"
// CATEGORY-DAG: methods = [
// CATEGORY-DAG: properties = [
// CATEGORY-DAG: type = {
// CATEGORY-NOT: cir.global
// CATEGORY-LABEL: cir.func{{.*}}@objc_category_declaration_is_metadata_only
#elif defined(TEST_IMPLEMENTATION_BODY)
@interface CIRUnsupportedImplementation
@end

@implementation CIRUnsupportedImplementation
- (void)run {
}
@end

// IMPLEMENTATION: error: ClangIR code gen Not Yet Implemented: declaration of kind: ObjCImplementation
#else
@interface CIRUnsupportedCategoryImplementation
@end

@implementation CIRUnsupportedCategoryImplementation (Extra)
- (void)run {
}
@end

// CATEGORY-IMPLEMENTATION: error: ClangIR code gen Not Yet Implemented: declaration of kind: ObjCCategoryImpl
#endif
