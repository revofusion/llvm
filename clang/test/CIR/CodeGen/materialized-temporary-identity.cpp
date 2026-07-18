// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR

struct Cleanup {
  Cleanup();
  Cleanup(const Cleanup &);
  ~Cleanup();
  int value();
};

void consume(const Cleanup &);
void consume_by_value(Cleanup);

// Both instantiations materialize the temporary at the same source location:
// the expression in this template definition. The producer metadata must use
// the concrete function identity as part of the key, rather than collapsing
// them by source location.
template <class T>
void same_spelling_site() {
  consume(Cleanup{});
}

void instantiate_same_spelling_site() {
  same_spelling_site<int>();
  same_spelling_site<long>();
}

// Repeated omitted arguments instantiate the same default-argument MTE twice
// in one concrete function. The producer-minted opaque tokens, not source
// locations or alloca names, distinguish those two storage instances.
void take(const Cleanup & = Cleanup{});

void default_argument_twice() {
  take();
  take();
}

// This has two standalone destructor-bearing CXXBindTemporaryExpr instances.
// Their storage needs identity even though it is not materialized for a
// reference.
void standalone_temporary_twice() {
  Cleanup{};
  Cleanup{};
}

// A by-value logging-style sink receives an aggregate through a reference
// materialization. This is emitted by AggExprEmitter rather than the ordinary
// emitMaterializeTemporaryExpr path.
void logging_style_materialized_temporary() {
  consume_by_value(static_cast<const Cleanup &>(Cleanup{}));
}

// The call owns destruction of this argument storage before the
// CXXBindTemporaryExpr visitor decides whether to push a second cleanup.
// Metadata must still attach to the actual alloca.
void cleanup_owned_bind() {
  consume_by_value(Cleanup{});
}

// NRVO reuses the function return slot for this automatic declaration. The
// cleanup identity must remain attached to that actual storage, rather than
// being recovered from the source spelling of `results`.
Cleanup automatic_cleanup_result() {
  Cleanup results;
  consume(results);
  return results;
}

// A prvalue constructed directly into the function return slot transfers its
// lifetime to the caller. The `__retval` alloca is not a local cleanup owner.
Cleanup direct_temporary_result(bool branch) {
  if (branch)
    return Cleanup{};
  return Cleanup{};
}

// Automatic declarations used as unbraced control-flow bodies still own
// alloca-attached cleanup identity. Cleanup emission is not their owner:
// it can be deferred through branch, loop, or continue cleanup paths.
void automatic_cleanup_if(bool b) {
  if (b)
    Cleanup guard;
}

void automatic_cleanup_while(bool b) {
  while (b)
    Cleanup guard;
}

void automatic_cleanup_continue(bool b) {
  while (b) {
    Cleanup guard;
    continue;
  }
}

// Each conditional full expression constructs the temporary in only one
// execution path. The cleanup guard must carry the exact alloca-owned
// temporary identity rather than recovering it from the flag or its name.
void conditional_temporary_expression(bool b) {
  b ? Cleanup{}.value() : 0;
}

void conditional_temporary_branch(bool b) {
  if (b)
    b ? Cleanup{}.value() : 0;
}

void conditional_temporary_loop(bool b) {
  while (b)
    b ? Cleanup{}.value() : 0;
}

void conditional_temporary_continue(bool b) {
  while (b) {
    b ? Cleanup{}.value() : 0;
    continue;
  }
}

// CIR-LABEL: cir.func{{.*}} @_Z18same_spelling_siteIiEvv()
// CIR-SAME: ast_decl_specialization_identity = {{.*}}mangled_name = "_Z18same_spelling_siteIiEvv"
// CIR: %[[INT_TEMP:.*]] = cir.alloca "ref.tmp0"{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[SOURCE_BEGIN:[0-9]+]] : i64{{.*}}function = @_Z18same_spelling_siteIiEvv
// CIR: cir.call @_Z7consumeRK7Cleanup(%[[INT_TEMP]])
// CIR: cir.call {{.*}}(%[[INT_TEMP]])

// CIR-LABEL: cir.func{{.*}} @_Z18same_spelling_siteIlEvv()
// CIR-SAME: ast_decl_specialization_identity = {{.*}}mangled_name = "_Z18same_spelling_siteIlEvv"
// CIR: %[[LONG_TEMP:.*]] = cir.alloca "ref.tmp0"{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[SOURCE_BEGIN]] : i64{{.*}}function = @_Z18same_spelling_siteIlEvv
// CIR: cir.call @_Z7consumeRK7Cleanup(%[[LONG_TEMP]])
// CIR: cir.call {{.*}}(%[[LONG_TEMP]])

// CIR: cir.func{{.*}} @[[DEFAULT_FUNCTION:[^ (]*default_argument_twice[^ (]*]]()
// CIR: %[[DEFAULT_TEMP0:.*]] = cir.alloca "ref.tmp0"{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[DEFAULT_BEGIN:[0-9]+]] : i64{{.*}}function = @[[DEFAULT_FUNCTION]]{{.*}}instance_token = "mte.instance.0"
// CIR: %[[DEFAULT_TEMP1:.*]] = cir.alloca "ref.tmp1"{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[DEFAULT_BEGIN]] : i64{{.*}}function = @[[DEFAULT_FUNCTION]]{{.*}}instance_token = "mte.instance.1"
// CIR-LABEL: cir.func{{.*}} @_Z26standalone_temporary_twicev()
// CIR: %[[STANDALONE_TEMP0:.*]] = cir.alloca{{.*}}ast_temporary_object_identity = {{.*}}begin_raw = [[STANDALONE_BEGIN0:[0-9]+]] : i64{{.*}}cleanup_kind = "cxx_destructor"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}end_raw = [[STANDALONE_END0:[0-9]+]] : i64{{.*}}function = @_Z26standalone_temporary_twicev{{.*}}instance_token = "cxx.temporary.instance.0"
// CIR: %[[STANDALONE_TEMP1:.*]] = cir.alloca{{.*}}ast_temporary_object_identity = {{.*}}begin_raw = [[STANDALONE_BEGIN1:[0-9]+]] : i64{{.*}}cleanup_kind = "cxx_destructor"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}end_raw = [[STANDALONE_END1:[0-9]+]] : i64{{.*}}function = @_Z26standalone_temporary_twicev{{.*}}instance_token = "cxx.temporary.instance.1"
// CIR-LABEL: cir.func{{.*}} @_Z36logging_style_materialized_temporaryv()
// CIR: %[[AGGREGATE_MTE_TEMP:.*]] = cir.alloca{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[AGGREGATE_BEGIN:[0-9]+]] : i64{{.*}}function = @_Z36logging_style_materialized_temporaryv{{.*}}instance_token = "mte.instance.0"
// CIR-SAME: ast_temporary_object_identity = {{.*}}instance_token = "cxx.temporary.instance.1"
// CIR: cir.call
// CIR-LABEL: cir.func{{.*}} @_Z18cleanup_owned_bindv()
// CIR: %[[CLEANUP_OWNED_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_Z18cleanup_owned_bindv{{.*}}instance_token = "cxx.temporary.instance.0"
// CIR-LABEL: cir.func{{.*}} @_Z24automatic_cleanup_resultv()
// CIR: cir.alloca{{.*}}ast_automatic_object_identity = {{.*}}begin_raw = [[AUTO_BEGIN:[0-9]+]] : i64{{.*}}cleanup_kind = "cxx_destructor"{{.*}}constructor_symbol = "_ZN7CleanupC1Ev"{{.*}}declaration_usr = "{{[^"]+}}"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}end_raw = [[AUTO_END:[0-9]+]] : i64{{.*}}function = @_Z24automatic_cleanup_resultv{{.*}}requires_observed_constructor_call = true
// CIR-LABEL: cir.func{{.*}} @_Z23direct_temporary_resultb(
// CIR: cir.alloca "__retval"
// CIR-NOT: ast_temporary_object_identity
// CIR: cir.return
// CIR-LABEL: cir.func{{.*}} @_Z20automatic_cleanup_ifb(
// CIR: cir.alloca "guard"{{.*}}ast_automatic_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}constructor_symbol = "_ZN7CleanupC1Ev"{{.*}}declaration_usr = "{{[^"]+}}"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_Z20automatic_cleanup_ifb{{.*}}requires_observed_constructor_call = true
// CIR-LABEL: cir.func{{.*}} @_Z23automatic_cleanup_whileb(
// CIR: cir.alloca "guard"{{.*}}ast_automatic_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}constructor_symbol = "_ZN7CleanupC1Ev"{{.*}}declaration_usr = "{{[^"]+}}"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_Z23automatic_cleanup_whileb{{.*}}requires_observed_constructor_call = true
// CIR-LABEL: cir.func{{.*}} @_Z26automatic_cleanup_continueb(
// CIR: cir.alloca "guard"{{.*}}ast_automatic_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}constructor_symbol = "_ZN7CleanupC1Ev"{{.*}}declaration_usr = "{{[^"]+}}"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_Z26automatic_cleanup_continueb{{.*}}requires_observed_constructor_call = true
// The identity attached to each guard is copied from the alloca it destroys.
// The common token proves the association without cleanup-flag-name inference.
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*conditional_temporary_expression[^ (]*}}(
// CIR: %[[CONDITIONAL_EXPR_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @{{[^ (]*conditional_temporary_expression[^ (]*}}{{.*}}instance_token = "[[CONDITIONAL_EXPR_TOKEN:cxx\.temporary\.instance\.[0-9]+]]"
// CIR: cir.if %[[CONDITIONAL_EXPR_FLAG:.*]] {
// CIR: cir.call @_ZN7CleanupD1Ev(%[[CONDITIONAL_EXPR_TEMP]])
// CIR: } {ast_conditional_cleanup_identity = {{.*}}instance_token = "[[CONDITIONAL_EXPR_TOKEN]]"
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*conditional_temporary_branch[^ (]*}}(
// CIR: %[[CONDITIONAL_BRANCH_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @{{[^ (]*conditional_temporary_branch[^ (]*}}{{.*}}instance_token = "[[CONDITIONAL_BRANCH_TOKEN:cxx\.temporary\.instance\.[0-9]+]]"
// CIR: cir.if %[[CONDITIONAL_BRANCH_FLAG:.*]] {
// CIR: cir.call @_ZN7CleanupD1Ev(%[[CONDITIONAL_BRANCH_TEMP]])
// CIR: } {ast_conditional_cleanup_identity = {{.*}}instance_token = "[[CONDITIONAL_BRANCH_TOKEN]]"
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*conditional_temporary_loop[^ (]*}}(
// CIR: %[[CONDITIONAL_LOOP_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @{{[^ (]*conditional_temporary_loop[^ (]*}}{{.*}}instance_token = "[[CONDITIONAL_LOOP_TOKEN:cxx\.temporary\.instance\.[0-9]+]]"
// CIR: cir.if %[[CONDITIONAL_LOOP_FLAG:.*]] {
// CIR: cir.call @_ZN7CleanupD1Ev(%[[CONDITIONAL_LOOP_TEMP]])
// CIR: } {ast_conditional_cleanup_identity = {{.*}}instance_token = "[[CONDITIONAL_LOOP_TOKEN]]"
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*conditional_temporary_continue[^ (]*}}(
// CIR: %[[CONDITIONAL_CONTINUE_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @{{[^ (]*conditional_temporary_continue[^ (]*}}{{.*}}instance_token = "[[CONDITIONAL_CONTINUE_TOKEN:cxx\.temporary\.instance\.[0-9]+]]"
// CIR: cir.if %[[CONDITIONAL_CONTINUE_FLAG:.*]] {
// CIR: cir.call @_ZN7CleanupD1Ev(%[[CONDITIONAL_CONTINUE_TEMP]])
// CIR: } {ast_conditional_cleanup_identity = {{.*}}instance_token = "[[CONDITIONAL_CONTINUE_TOKEN]]"
// CIR: cir.continue
// A concrete specialization whose argument is an anonymous closure still has
// producer-owned identity even when Clang cannot form a declaration USR for
// the specialization itself.
template <class T>
void anonymous_specialization(T) {}

void instantiate_anonymous_specialization() {
  anonymous_specialization([] {});
}

// CIR: cir.func{{.*}} @[[ANONYMOUS_FUNCTION:[^ (]*anonymous_specialization[^ (]*]](
// CIR-SAME: ast_decl_specialization_identity = {{.*}}mangled_name = "[[ANONYMOUS_FUNCTION]]"
