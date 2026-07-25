// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s

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

void bindOnce(void (Owner::*const &callback)());
void compilerCreatedMemberPointerTemp() { bindOnce(&Owner::method); }

struct Destroy {
  ~Destroy();
};

void implicitDestroy() { Destroy value; }

void explicitDestroy(Destroy *value) { value->~Destroy(); }

// CHECK-DAG: cir.global{{.*}}@__const._Z14memberPointersv.nullMethod{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}
// CHECK-DAG: cir.global{{.*}}@__const._Z14memberPointersv.method{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}

// CHECK-LABEL: cir.func{{.*}}@_Z6upcastP7Derived
// CHECK: cir.base_class_addr{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "DerivedToBase"
// CHECK-SAME: is_explicit = false
// CHECK-SAME: result_record_usr = "c:@S@Base"
// CHECK-SAME: result_type = [
// CHECK-SAME: source_record_usr = "c:@S@Derived"
// CHECK-SAME: source_type = [

// CHECK-LABEL: cir.func{{.*}}@_Z12explicitCastP7Derived
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: is_explicit = true
// CHECK-SAME: result_record_usr = "c:@S@Other"
// CHECK-SAME: source_record_usr = "c:@S@Derived"

// The source CIR type is a pointer to an array, not a named record type.
// CHECK-LABEL: cir.func{{.*}}@_Z10arrayDecayv
// CHECK: cir.cast array_to_ptrdecay{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "ArrayToPointerDecay"
// CHECK-SAME: result_record_usr = "c:@S@Element"
// CHECK-SAME: source_record_usr = "c:@S@Element"

// The result CIR endpoint is an erased !cir.ptr<!void>, but the AST source
// remains authoritative.
// CHECK-LABEL: cir.func{{.*}}@_Z12erasePointerP7Element
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: is_explicit = true
// CHECK-NOT: result_record_usr
// CHECK-SAME: result_type = [
// CHECK-SAME: source_record_usr = "c:@S@Element"

// The source CIR endpoint is an erased !cir.ptr<!void>; no source record USR
// may be fabricated from the result type.
// CHECK-LABEL: cir.func{{.*}}@_Z14recoverPointerPv
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: is_explicit = true
// CHECK-SAME: result_record_usr = "c:@S@Element"
// CHECK-NOT: source_record_usr
// CHECK-SAME: source_type = [

// CHECK-LABEL: cir.func{{.*}}@_Z14memberPointersv
// CHECK: cir.alloca{{.*}}ast_member_pointer_target = {pointee_kind = "data", record_usr = "c:@S@Owner"}
// CHECK: cir.alloca{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}
// CHECK: cir.alloca{{.*}}ast_member_pointer_target = {pointee_kind = "data", record_usr = "c:@S@Owner"}
// CHECK: cir.alloca{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}
// CHECK: cir.const{{.*}}ast_member_pointer_target = {pointee_kind = "data", record_usr = "c:@S@Owner"}
// CHECK: cir.const{{.*}}ast_member_pointer_target = {pointee_kind = "data", record_usr = "c:@S@Owner"}

// CHECK-LABEL: cir.func{{.*}}@_Z32compilerCreatedMemberPointerTempv
// CHECK: cir.alloca "ref.tmp0"{{.*}}ast_member_pointer_target = {pointee_kind = "function", record_usr = "c:@S@Owner"}

// CHECK-LABEL: cir.func{{.*}}@_Z15implicitDestroyv
// CHECK: cir.call{{.*}}@_ZN7DestroyD1Ev
// CHECK-NOT: ast_destructor_call

// CHECK-LABEL: cir.func{{.*}}@_Z15explicitDestroyP7Destroy
// CHECK: cir.call{{.*}}ast_destructor_call = {callee_symbol = "_ZN7DestroyD1Ev", destructor_usr = "{{[^"]+}}", is_explicit = true, variant = "complete"}
