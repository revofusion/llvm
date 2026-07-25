// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o - | FileCheck %s

typedef struct {
  int value;
} AnonRecord;

typedef union {
  int integer;
  float real;
} AnonUnion;

AnonRecord records[2];
AnonUnion unions[2];

AnonRecord *decay_record(void) { return records; }
AnonUnion *decay_union(void) { return unions; }

void *erase_record(AnonRecord *value) { return value; }
AnonRecord *recover_record(void *value) { return value; }

void *no_record_endpoint(long value) { return (void *)value; }

struct IncompleteRecord;
struct IncompleteRecord *recover_incomplete(void *value) { return value; }



// The source CIR types are pointers to arrays. The USRs come from the direct
// canonical AST record endpoints, including anonymous typedef records.
// CHECK-LABEL: cir.func{{.*}}@decay_record
// CHECK: cir.cast array_to_ptrdecay{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "ArrayToPointerDecay"
// CHECK-SAME: result_record_usr = "c:@SA@AnonRecord"
// CHECK-SAME: source_record_usr = "c:@SA@AnonRecord"

// CHECK-LABEL: cir.func{{.*}}@decay_union
// CHECK: cir.cast array_to_ptrdecay{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "ArrayToPointerDecay"
// CHECK-SAME: result_record_usr = "c:@UA@AnonUnion"
// CHECK-SAME: source_record_usr = "c:@UA@AnonUnion"

// The erased void endpoint has no record; only the exact AST record endpoint
// contributes a USR.
// CHECK-LABEL: cir.func{{.*}}@erase_record
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-NOT: result_record_usr
// CHECK-SAME: result_type = [
// CHECK-SAME: source_record_usr = "c:@SA@AnonRecord"

// CHECK-LABEL: cir.func{{.*}}@recover_record
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: result_record_usr = "c:@SA@AnonRecord"
// CHECK-NOT: source_record_usr
// CHECK-SAME: source_type = [

// Neither endpoint has a record, so no record USR may be fabricated.
// CHECK-LABEL: cir.func{{.*}}@no_record_endpoint
// CHECK-NOT: record_usr
// CHECK: cir.cast int_to_ptr{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "IntegralToPointer"
// CHECK-NOT: record_usr


// An incomplete endpoint has no projected object layout, but its compiler USR
// remains exact and must be retained for opaque-pointer provenance.
// CHECK-LABEL: cir.func{{.*}}@recover_incomplete
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: result_record_usr = "c:@S@IncompleteRecord"
// CHECK-NOT: source_record_usr
