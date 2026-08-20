// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s
// RUN: %clang -target x86_64-unknown-linux-gnu -std=c++17 -emit-cir %s -o - | FileCheck %s --check-prefix=REPLACE

struct Base {};
struct Derived : Base {};

Base *upcast(Derived *value) { return value; }

struct Other {};
Other *explicitCast(Derived *value) {
  return reinterpret_cast<Other *>(value);
}

struct Element {};
Element elementArray[2];

Element *arrayDecay() { return elementArray; }

void *erasePointer(Element *value) {
  return reinterpret_cast<void *>(value);
}

Element *recoverPointer(void *value) {
  return reinterpret_cast<Element *>(value);
}

int scalarCast(char value) { return value; }

struct Owner {
  int field;
  void method();
};

void memberPointers() {
  int Owner::*data = &Owner::field;
  void (Owner::*method)() = &Owner::method;
  int Owner::*nullData = nullptr;
  void (Owner::*nullMethod)() = nullptr;
}

int Owner::*returnDataMember() { return &Owner::field; }

void bindOnce(void (Owner::*const &callback)());
void compilerCreatedMemberPointerTemp() { bindOnce(&Owner::method); }

struct ObjectArrayElement {
  int value;
};
template <unsigned N>
int readObjectArray(const ObjectArrayElement (&values)[N]) {
  return values[0].value + values[N - 1].value;
}
int compilerCreatedObjectArrayTemp() {
  return readObjectArray<2>({{1}, {2}});
}

template <unsigned N>
int templateStackObjectArray() {
  ObjectArrayElement values[N] = {};
  return values[N - 1].value;
}
template int templateStackObjectArray<3>();

template <unsigned Rows, unsigned Columns>
int templateStackObjectArray2D() {
  ObjectArrayElement values[Rows][Columns] = {};
  return values[Rows - 1][Columns - 1].value;
}
template int templateStackObjectArray2D<2, 3>();

struct Destroy {
  ~Destroy();
};

void implicitDestroy() { Destroy value; }

void explicitDestroy(Destroy *value) { value->~Destroy(); }

using size_t = __SIZE_TYPE__;
void *operator new(size_t, void *) noexcept;

struct AliasDestroy {
  int value;
  explicit AliasDestroy(int input = 0) : value(input) {}
  AliasDestroy(const AliasDestroy &other) : value(other.value) {}
  ~AliasDestroy() {}
};

template <class T, class... Args>
T *constructAlias(T *location, Args &&...args) {
  return ::new (location) T(static_cast<Args &&>(args)...);
}

template <class T>
void explicitDestroyTemplate(T *value) {
  value->~T();
}

void instantiateExplicitDestroyTemplate(AliasDestroy *value) {
  constructAlias(value, 1);
  explicitDestroyTemplate(value);
}

void automaticAliasDestroy() { AliasDestroy value; }

// CHECK-DAG: cir.global{{.*}}@__const._Z14memberPointersv.nullMethod{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}
// CHECK-DAG: cir.global{{.*}}@__const._Z14memberPointersv.method{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}

// CHECK-LABEL: cir.func{{.*}}@_Z6upcastP7Derived
// CHECK: cir.base_class_addr{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "DerivedToBase"
// CHECK-SAME: is_explicit = false
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = !rec_Base
// CHECK-SAME: result_record_usr = "c:@S@Base"
// CHECK-SAME: result_type = [
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = !rec_Derived
// CHECK-SAME: source_record_usr = "c:@S@Derived"
// CHECK-SAME: source_type = [

// CHECK-LABEL: cir.func{{.*}}@_Z12explicitCastP7Derived
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: is_explicit = true
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = !rec_Other
// CHECK-SAME: result_record_usr = "c:@S@Other"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = !rec_Derived
// CHECK-SAME: source_record_usr = "c:@S@Derived"

// The source CIR type is a pointer to an array, not a named record type.
// CHECK-LABEL: cir.func{{.*}}@_Z10arrayDecayv
// CHECK: cir.cast array_to_ptrdecay{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "ArrayToPointerDecay"
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = !rec_Element
// CHECK-SAME: result_record_usr = "c:@S@Element"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = !rec_Element
// CHECK-SAME: source_record_usr = "c:@S@Element"

// The result CIR endpoint is an erased !cir.ptr<!void>, but the AST source
// remains authoritative.
// CHECK-LABEL: cir.func{{.*}}@_Z12erasePointerP7Element
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: is_explicit = true
// CHECK-SAME: result_record_presence = "no_record"
// CHECK-NOT: result_record_schema
// CHECK-NOT: result_record_usr
// CHECK-SAME: result_type = [
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = !rec_Element
// CHECK-SAME: source_record_usr = "c:@S@Element"

// The source CIR endpoint is an erased !cir.ptr<!void>; no source record USR
// may be fabricated from the result type.
// CHECK-LABEL: cir.func{{.*}}@_Z14recoverPointerPv
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: is_explicit = true
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = !rec_Element
// CHECK-SAME: result_record_usr = "c:@S@Element"
// CHECK-SAME: source_record_presence = "no_record"
// CHECK-NOT: source_record_schema
// CHECK-NOT: source_record_usr
// CHECK-SAME: source_type = [

// Scalar endpoints are explicitly schema-free rather than inferred from CIR
// pointer or record shapes.
// CHECK-LABEL: cir.func{{.*}}@_Z10scalarCastc
// CHECK: cir.cast integral{{.*}}ast_cast_expr = {
// CHECK-SAME: result_record_presence = "no_record"
// CHECK-NOT: result_record_schema
// CHECK-NOT: result_record_usr
// CHECK-SAME: source_record_presence = "no_record"
// CHECK-NOT: source_record_schema
// CHECK-NOT: source_record_usr
// CHECK-SAME: source_type = [

// CHECK-LABEL: cir.func{{.*}}@_Z14memberPointersv
// CHECK: cir.alloca{{.*}}ast_member_pointer_target = {pointee_kind = "data", record_usr = "c:@S@Owner"}
// CHECK: cir.alloca{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}
// CHECK: cir.alloca{{.*}}ast_member_pointer_target = {pointee_kind = "data", record_usr = "c:@S@Owner"}
// CHECK: cir.alloca{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}
// CHECK: cir.const{{.*}}ast_data_member_pointer_constant = {
// CHECK-SAME: field_decl_id = "{{[^"]+}}"
// CHECK-SAME: provenance_kind = "field_decl"
// CHECK-SAME: target_record_usr = "c:@S@Owner"
// CHECK: cir.const{{.*}}ast_data_member_pointer_constant = {
// CHECK-SAME: provenance_kind = "null"
// CHECK-SAME: target_record_usr = "c:@S@Owner"

// A data-member-returning function authenticates its exact FieldDecl and ABI
// carrier on the result itself; consumers never recover either from call bits.
// CHECK-LABEL: cir.func{{.*}}@_Z16returnDataMemberv
// CHECK-SAME: cir.ast_member_pointer = {
// CHECK-SAME: carrier_align_bits = 64 : i64
// CHECK-SAME: carrier_bits = 64 : i64
// CHECK-SAME: carrier_signed = true
// CHECK-SAME: class_type = !rec_Owner
// CHECK-SAME: field_decl_id = "{{[^"]+}}"
// CHECK-SAME: kind = "data"
// CHECK-SAME: null_value = "-1"
// CHECK-SAME: pointee_type = !s32i
// CHECK-SAME: provenance_kind = "field_decl"
// CHECK-SAME: target_record_usr = "c:@S@Owner"

// CHECK-LABEL: cir.func{{.*}}@_Z32compilerCreatedMemberPointerTempv
// CHECK: cir.alloca "ref.tmp0"{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}

// A compiler-created backing array has no VarDecl name from which a consumer
// can recover its element schema. The QualType-bearing allocation producer
// carries the fixed extent and exact terminal RecordDecl identity.
// CHECK-LABEL: cir.func{{.*}}@_Z30compilerCreatedObjectArrayTempv
// CHECK: cir.alloca{{.*}}ast_object_array_allocation = {
// CHECK-SAME: array_extents = [2]
// CHECK-SAME: element_record_schema = !rec_ObjectArrayElement
// CHECK-SAME: element_record_usr = "c:@S@ObjectArrayElement"
// CHECK-SAME: source_type = [

// A dependent local array becomes a fixed object-array alloca when its
// function template is materialized. The instantiated VarDecl remains the
// QualType authority for the alloca metadata.
// CHECK-LABEL: cir.func{{.*}}@_Z24templateStackObjectArrayILj3EEiv
// CHECK: cir.alloca "values"{{.*}}ast_object_array_allocation = {
// CHECK-SAME: array_extents = [3]
// CHECK-SAME: element_record_schema = !rec_ObjectArrayElement
// CHECK-SAME: element_record_usr = "c:@S@ObjectArrayElement"
// CHECK-SAME: source_type = [{align_bits = 32 : i64, bit_width = 96 : i64, clang_address_space = 0 : i64, is_atomic = false, is_const = false, is_restrict = false, is_volatile = false, kind = "value", target_address_space = 0 : i64}]

// Every dependent array dimension is materialized outer-to-inner, while the
// schema and declaration identity remain those of the terminal record.
// CHECK-LABEL: cir.func{{.*}}templateStackObjectArray2D
// CHECK: cir.alloca "values"{{.*}}ast_object_array_allocation = {
// CHECK-SAME: array_extents = [2, 3]
// CHECK-SAME: element_record_schema = !rec_ObjectArrayElement
// CHECK-SAME: element_record_usr = "c:@S@ObjectArrayElement"
// CHECK-SAME: source_type = [{align_bits = 32 : i64, bit_width = 192 : i64, clang_address_space = 0 : i64, is_atomic = false, is_const = false, is_restrict = false, is_volatile = false, kind = "value", target_address_space = 0 : i64}]

// CHECK-LABEL: cir.func{{.*}}@_Z15implicitDestroyv
// CHECK: cir.call{{.*}}@_ZN7DestroyD1Ev
// CHECK-NOT: ast_destructor_call

// CHECK-LABEL: cir.func{{.*}}@_Z15explicitDestroyP7Destroy
// CHECK: cir.call{{.*}}ast_destructor_call = {callee_symbol = "_ZN7DestroyD1Ev", destructor_usr = "c:@S@Destroy@F@~Destroy#", is_explicit = true, variant = "complete"}

// Constructor calls preserve both the emitted ABI entry point and the
// canonical complete-object identity joined to cleanup metadata.
// CHECK-LABEL: cir.func{{.*}}@_Z21automaticAliasDestroyv
// CHECK: cir.call @_ZN12AliasDestroyC1Ei{{.*}}ast_constructor_call = {callee_symbol = "_ZN12AliasDestroyC1Ei", canonical_symbol = "_ZN12AliasDestroyC1Ei", constructor_usr = "c:@S@AliasDestroy@F@AliasDestroy#I#", variant = "complete"}

// Replacing the complete destructor with its emitted base entry point must
// normalize both halves of the exact producer identity.
// REPLACE-LABEL: cir.func{{.*}}explicitDestroyTemplateI12AliasDestroy
// REPLACE: cir.call{{.*}}@_ZN12AliasDestroyD2Ev{{.*}}ast_destructor_call = {callee_symbol = "_ZN12AliasDestroyD2Ev", destructor_usr = "c:@S@AliasDestroy@F@~AliasDestroy#", is_explicit = true, variant = "base"}

// Function replacement updates the exact ABI symbols used by cleanup and the
// call, while canonical_symbol preserves the complete-object C1 join.
// REPLACE-LABEL: cir.func{{.*}}@_Z21automaticAliasDestroyv
// REPLACE: cir.alloca "value"{{.*}}ast_automatic_object_identity = {begin_raw = {{[0-9]+}} : i64, cleanup_kind = "cxx_destructor", constructor_owner_record_usr = "c:@S@AliasDestroy", constructor_symbol = "_ZN12AliasDestroyC2Ei"
// REPLACE-SAME: constructor_usr = "c:@S@AliasDestroy@F@AliasDestroy#I#"
// REPLACE-SAME: destructor_symbol = "_ZN12AliasDestroyD2Ev"
// REPLACE-SAME: destructor_usr = "c:@S@AliasDestroy@F@~AliasDestroy#"
// REPLACE: cir.call @_ZN12AliasDestroyC2Ei
// REPLACE-SAME: ast_constructor_call = {callee_symbol = "_ZN12AliasDestroyC2Ei", canonical_symbol = "_ZN12AliasDestroyC1Ei", constructor_usr = "c:@S@AliasDestroy@F@AliasDestroy#I#", variant = "base"}
// REPLACE: cir.call @_ZN12AliasDestroyD2Ev
