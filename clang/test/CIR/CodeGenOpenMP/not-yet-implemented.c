// RUN: %clang_cc1 -fopenmp -fclangir %s -verify -emit-cir -o -
// RUN: rm -f %t.jsonl
// RUN: not env HELIOS_CIR_DIAGNOSTIC_CENSUS_PATH=%t.jsonl %clang_cc1 \
// RUN:   -fopenmp -fclangir %s -emit-cir -o /dev/null 2> %t.err
// RUN: %python -c "import json,sys; rows=[json.loads(line) for line in open(sys.argv[1])]; assert len(rows) == 3; assert all(row['code'] == 'LLVM_CIR_NYI' and row['owner'] == 'clang_decl:c:@F@do_things' and row['source_node_id'] == 'c:@F@do_things' and row['blocked_by'] == [] for row in rows)" %t.jsonl
// RUN: FileCheck %s --check-prefix=CENSUS < %t.jsonl

void do_things() {
  // expected-error@+1{{ClangIR code gen Not Yet Implemented: OpenMP OMPCriticalDirective}}
#pragma omp critical
  {}

  // expected-error@+1{{ClangIR code gen Not Yet Implemented: OpenMP OMPSingleDirective}}
#pragma omp single
  {}

  int i;
  // TODO(OMP): We might consider overloading operator<< for OMPClauseKind in
  // the future if we want to improve this.
  // expected-error@+1{{ClangIR code gen Not Yet Implemented: OpenMPClause : if}}
#pragma omp parallel if(i)
  {}
}

// CENSUS:      {"id":"LLVM_CIR_NYI:1","code":"LLVM_CIR_NYI","stage":"cir_codegen","owner":"clang_decl:c:@F@do_things","unit_id":"clang_tu:
// CENSUS-SAME: "source_node_id":"c:@F@do_things","span":{"file":
// CENSUS-SAME: "message":"ClangIR code gen Not Yet Implemented: OpenMP OMPCriticalDirective","shape":"OpenMP OMPCriticalDirective","blocked_by":[]}
// CENSUS-NEXT: {"id":"LLVM_CIR_NYI:2","code":"LLVM_CIR_NYI","stage":"cir_codegen","owner":"clang_decl:c:@F@do_things"
// CENSUS-SAME: "message":"ClangIR code gen Not Yet Implemented: OpenMP OMPSingleDirective","shape":"OpenMP OMPSingleDirective","blocked_by":[]}
// CENSUS-NEXT: {"id":"LLVM_CIR_NYI:3","code":"LLVM_CIR_NYI","stage":"cir_codegen","owner":"clang_decl:c:@F@do_things"
// CENSUS-SAME: "message":"ClangIR code gen Not Yet Implemented: OpenMPClause : if","shape":"OpenMPClause : if","blocked_by":[]}
