// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR

struct Cleanup {
  Cleanup();
  explicit Cleanup(int);
  Cleanup(const Cleanup &);
  Cleanup(Cleanup &&);
  ~Cleanup();
  int value();
};
struct ArrayCleanup {
  ArrayCleanup();
  ~ArrayCleanup();
};
using ArrayTemporary = ArrayCleanup[2];

struct TemporaryMatrix {
  explicit TemporaryMatrix(int);
  ~TemporaryMatrix();
};
void consume_matrix(const TemporaryMatrix &);


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
void array_temporary_identity() {
  const ArrayTemporary &values = ArrayTemporary{};
  (void)values;
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

// Constructor initializer expressions participate in the same stable
// declaration preorder as the body. Initializers are visited in semantic
// member-initialization order, not their written order.
struct CtorInitSink {
  explicit CtorInitSink(const Cleanup &);
};
struct CtorInitOwner {
  CtorInitSink first;
  CtorInitSink second;
  CtorInitOwner();
};
CtorInitOwner::CtorInitOwner()
    : second(Cleanup{2}), first(Cleanup{1}) {
  Cleanup{};
}

void transferred_argument(Cleanup value) {
  consume_by_value(static_cast<Cleanup &&>(value));
}

// Structured-operation builders emit their regions before attaching the
// enclosing operation to the function. Temporary identity must still use the
// concrete function being emitted rather than an incomplete parent chain.
void detached_region_temporaries(bool b) {
  if (b)
    consume(Cleanup{});
  else
    (Cleanup)0;
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

void conditional_matrix(bool b) {
  consume_matrix(b ? TemporaryMatrix{1} : TemporaryMatrix{2});
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
// CIR: %[[INT_TEMP:.*]] = cir.alloca "ref.tmp0"{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[SOURCE_BEGIN:[0-9]+]] : i64{{.*}}declaration_ordinal = {{[0-9]+}} : i64{{.*}}function = @_Z18same_spelling_siteIiEvv
// CIR: cir.call @_Z7consumeRK7Cleanup(%[[INT_TEMP]])
// CIR: cir.call {{.*}}(%[[INT_TEMP]])

// CIR-LABEL: cir.func{{.*}} @_Z18same_spelling_siteIlEvv()
// CIR-SAME: ast_decl_specialization_identity = {{.*}}mangled_name = "_Z18same_spelling_siteIlEvv"
// CIR: %[[LONG_TEMP:.*]] = cir.alloca "ref.tmp0"{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[SOURCE_BEGIN]] : i64{{.*}}declaration_ordinal = {{[0-9]+}} : i64{{.*}}function = @_Z18same_spelling_siteIlEvv
// CIR: cir.call @_Z7consumeRK7Cleanup(%[[LONG_TEMP]])
// CIR: cir.call {{.*}}(%[[LONG_TEMP]])

// CIR: cir.func{{.*}} @[[DEFAULT_FUNCTION:[^ (]*default_argument_twice[^ (]*]]()
// CIR: %[[DEFAULT_TEMP0:.*]] = cir.alloca "ref.tmp0"{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[DEFAULT_BEGIN:[0-9]+]] : i64{{.*}}declaration_ordinal = [[DEFAULT_ORDINAL:[0-9]+]] : i64{{.*}}function = @[[DEFAULT_FUNCTION]]{{.*}}instance_token = "mte.instance.0"{{.*}}ast_temporary_object_identities = {{.*}}declaration_ordinal = [[DEFAULT_ORDINAL]] : i64{{.*}}instance_token = "mte.instance.0"
// CIR: %[[DEFAULT_TEMP1:.*]] = cir.alloca "ref.tmp1"{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[DEFAULT_BEGIN]] : i64{{.*}}declaration_ordinal = [[DEFAULT_ORDINAL]] : i64{{.*}}function = @[[DEFAULT_FUNCTION]]{{.*}}instance_token = "mte.instance.1"{{.*}}ast_temporary_object_identities = {{.*}}declaration_ordinal = [[DEFAULT_ORDINAL]] : i64{{.*}}instance_token = "mte.instance.1"
// CIR-LABEL: cir.func{{.*}} @_Z26standalone_temporary_twicev()
// CIR: %[[STANDALONE_TEMP0:.*]] = cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}begin_raw = [[STANDALONE_BEGIN0:[0-9]+]] : i64{{.*}}cleanup_kind = "cxx_destructor"{{.*}}declaration_ordinal = [[STANDALONE_ORDINAL0:[0-9]+]] : i64{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}end_raw = [[STANDALONE_END0:[0-9]+]] : i64{{.*}}function = @_Z26standalone_temporary_twicev{{.*}}instance_token = "{{cxx\.temporary\.instance\.[0-9]+}}"
// CIR: %[[STANDALONE_TEMP1:.*]] = cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}begin_raw = [[STANDALONE_BEGIN1:[0-9]+]] : i64{{.*}}cleanup_kind = "cxx_destructor"{{.*}}declaration_ordinal = [[STANDALONE_ORDINAL1:[0-9]+]] : i64{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}end_raw = [[STANDALONE_END1:[0-9]+]] : i64{{.*}}function = @_Z26standalone_temporary_twicev{{.*}}instance_token = "{{cxx\.temporary\.instance\.[0-9]+}}"
// CIR-LABEL: cir.func{{.*}} @_Z24array_temporary_identityv()
// CIR: cir.alloca "ref.tmp0"{{.*}}ast_temporary_object_identities = {{.*}}constructor_symbol = "_ZN12ArrayCleanupC1Ev"{{.*}}constructor_usr = "{{[^"]+}}"{{.*}}destructor_symbol = "_ZN12ArrayCleanupD1Ev"{{.*}}requires_observed_constructor_call = true
// CIR-LABEL: cir.func{{.*}} @_Z36logging_style_materialized_temporaryv()
// CIR: %[[AGGREGATE_MTE_TEMP:.*]] = cir.alloca{{.*}}ast_materialize_temporary_identity = {{.*}}begin_raw = [[AGGREGATE_BEGIN:[0-9]+]] : i64{{.*}}declaration_ordinal = [[AGGREGATE_ORDINAL:[0-9]+]] : i64{{.*}}function = @_Z36logging_style_materialized_temporaryv{{.*}}instance_token = "[[AGGREGATE_TOKEN:mte\.instance\.[0-9]+]]"{{.*}}ast_temporary_object_identities = {{.*}}declaration_ordinal = [[AGGREGATE_ORDINAL]] : i64{{.*}}instance_token = "[[AGGREGATE_TOKEN]]"
// CIR: cir.call
// CIR-LABEL: cir.func{{.*}} @_Z18cleanup_owned_bindv()
// CIR: %[[CLEANUP_OWNED_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_Z18cleanup_owned_bindv{{.*}}instance_token = "{{cxx\.temporary\.instance\.[0-9]+}}"
// The written initializer order is second/first, but the declaration ordinals
// follow semantic initialization order first/second, then the function body.
// CIR-LABEL: cir.func{{.*}} @_ZN13CtorInitOwnerC2Ev(
// CIR: cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}declaration_ordinal = 0 : i64{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_ZN13CtorInitOwnerC2Ev{{.*}}instance_token = "{{mte\.instance\.[0-9]+}}"
// CIR: cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}declaration_ordinal = 1 : i64{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_ZN13CtorInitOwnerC2Ev{{.*}}instance_token = "{{mte\.instance\.[0-9]+}}"
// CIR: cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}declaration_ordinal = 2 : i64{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_ZN13CtorInitOwnerC2Ev{{.*}}instance_token = "{{cxx\.temporary\.instance\.[0-9]+}}"
// Repeated discovery of one by-value argument binding must not mint a second
// runtime identity for the same exact AST temporary and physical alloca.
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*transferred_argument[^ (]*}}(
// CIR: cir.alloca "agg.tmp0"{{.*}}ast_temporary_object_identities = {{.*}}instance_token = "[[TRANSFERRED_TOKEN:cxx\.temporary\.instance\.[0-9]+]]"
// CIR-NOT: instance_token =
// CIR: cir.call @_ZN7CleanupC1EOS_
// Both temporary allocas are emitted while the enclosing cir.if is detached.
// The reference-bound temporary carries both AST identities, while the
// standalone C-style cast carries its CXXBindTemporaryExpr identity.
// CIR-LABEL: cir.func{{.*}} @_Z27detached_region_temporariesb(
// CIR: cir.if
// CIR: cir.alloca{{.*}}ast_materialize_temporary_identity = {{.*}}function = @_Z27detached_region_temporariesb{{.*}}ast_temporary_object_identities = {{.*}}function = @_Z27detached_region_temporariesb
// CIR: cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}function = @_Z27detached_region_temporariesb
// CIR-LABEL: cir.func{{.*}} @_Z24automatic_cleanup_resultv()
// CIR: cir.alloca{{.*}}ast_automatic_object_identity = {{.*}}begin_raw = [[AUTO_BEGIN:[0-9]+]] : i64{{.*}}cleanup_kind = "cxx_destructor"{{.*}}constructor_symbol = "_ZN7CleanupC1Ev"{{.*}}declaration_usr = "{{[^"]+}}"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}end_raw = [[AUTO_END:[0-9]+]] : i64{{.*}}function = @_Z24automatic_cleanup_resultv{{.*}}requires_observed_constructor_call = true
// CIR-LABEL: cir.func{{.*}} @_Z23direct_temporary_resultb(
// CIR: cir.alloca "__retval"
// CIR-NOT: ast_temporary_object_identities
// CIR: cir.return
// CIR-LABEL: cir.func{{.*}} @_Z20automatic_cleanup_ifb(
// CIR: cir.alloca "guard"{{.*}}ast_automatic_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}constructor_symbol = "_ZN7CleanupC1Ev"{{.*}}declaration_usr = "{{[^"]+}}"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_Z20automatic_cleanup_ifb{{.*}}requires_observed_constructor_call = true
// CIR-LABEL: cir.func{{.*}} @_Z23automatic_cleanup_whileb(
// CIR: cir.alloca "guard"{{.*}}ast_automatic_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}constructor_symbol = "_ZN7CleanupC1Ev"{{.*}}declaration_usr = "{{[^"]+}}"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_Z23automatic_cleanup_whileb{{.*}}requires_observed_constructor_call = true
// CIR-LABEL: cir.func{{.*}} @_Z26automatic_cleanup_continueb(
// CIR: cir.alloca "guard"{{.*}}ast_automatic_object_identity = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}constructor_symbol = "_ZN7CleanupC1Ev"{{.*}}declaration_usr = "{{[^"]+}}"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @_Z26automatic_cleanup_continueb{{.*}}requires_observed_constructor_call = true
// The identity attached to each guard is copied from the alloca it destroys.
// The common token proves the association without cleanup-flag-name inference.
// The active flag is allocated before its false initialization; initialization
// precedes the outermost ternary, and activation occurs only in its taken path.
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*conditional_temporary_expression[^ (]*}}(
// CIR: %[[CONDITIONAL_EXPR_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}declaration_ordinal = [[CONDITIONAL_EXPR_ORDINAL:[0-9]+]] : i64{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @{{[^ (]*conditional_temporary_expression[^ (]*}}{{.*}}instance_token = "[[CONDITIONAL_EXPR_TOKEN:mte\.instance\.[0-9]+]]"
// CIR: %[[CONDITIONAL_EXPR_ACTIVE:.*]] = cir.alloca "cleanup.cond"{{.*}} : !cir.ptr<!cir.bool>
// CIR: cir.cleanup.scope
// CIR: %[[CONDITIONAL_EXPR_FALSE:.*]] = cir.const #false
// CIR: cir.store %[[CONDITIONAL_EXPR_FALSE]], %[[CONDITIONAL_EXPR_ACTIVE]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR: cir.ternary
// CIR: cir.call @_ZN7CleanupC1Ev(%[[CONDITIONAL_EXPR_TEMP]])
// CIR: %[[CONDITIONAL_EXPR_TRUE:.*]] = cir.const #true
// CIR: cir.store %[[CONDITIONAL_EXPR_TRUE]], %[[CONDITIONAL_EXPR_ACTIVE]] : !cir.bool, !cir.ptr<!cir.bool>
// CIR: %[[CONDITIONAL_EXPR_FLAG:.*]] = cir.load {{.*}}%[[CONDITIONAL_EXPR_ACTIVE]]
// CIR: cir.if %[[CONDITIONAL_EXPR_FLAG]] {
// CIR: cir.call @_ZN7CleanupD1Ev(%[[CONDITIONAL_EXPR_TEMP]])
// CIR: } {ast_conditional_cleanup_identities = {{.*}}instance_token = "[[CONDITIONAL_EXPR_TOKEN]]"
// A conditional operator with two destructor-bearing branches shares one
// materialized alloca but retains both exact producer tuples in AST order.
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*conditional_matrix[^ (]*}}(
// CIR: %[[MATRIX_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}instance_token = "[[MATRIX_TOKEN0:cxx\.temporary\.instance\.0]]"{{.*}}instance_token = "[[MATRIX_TOKEN1:cxx\.temporary\.instance\.1]]"
// CIR: cir.cleanup.scope
// CIR: } cleanup normal {
// CIR: cir.call @_ZN15TemporaryMatrixD1Ev(%[[MATRIX_TEMP]])
// CIR-NOT: cir.call @_ZN15TemporaryMatrixD1Ev(%[[MATRIX_TEMP]])
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*conditional_temporary_branch[^ (]*}}(
// CIR: %[[CONDITIONAL_BRANCH_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}destructor_symbol = "_ZN7CleanupD1Ev"{{.*}}function = @{{[^ (]*conditional_temporary_branch[^ (]*}}{{.*}}instance_token = "[[CONDITIONAL_BRANCH_TOKEN:mte\.instance\.[0-9]+]]"
// CIR: cir.if %[[CONDITIONAL_BRANCH_FLAG:.*]] {
// CIR: cir.call @_ZN7CleanupD1Ev(%[[CONDITIONAL_BRANCH_TEMP]])
// CIR: } {ast_conditional_cleanup_identities = {{.*}}instance_token = "[[CONDITIONAL_BRANCH_TOKEN]]"
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*conditional_temporary_loop[^ (]*}}(
// CIR: %[[CONDITIONAL_LOOP_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}cleanup_kind = "cxx_destructor"{{.*}}function = @{{[^ (]*conditional_temporary_loop[^ (]*}}{{.*}}instance_token = "[[CONDITIONAL_LOOP_TOKEN:mte\.instance\.[0-9]+]]"
// CIR: cir.if %[[CONDITIONAL_LOOP_FLAG:.*]] {
// CIR: cir.call @_ZN7CleanupD1Ev(%[[CONDITIONAL_LOOP_TEMP]])
// CIR: } {ast_conditional_cleanup_identities = {{.*}}instance_token = "[[CONDITIONAL_LOOP_TOKEN]]"
// CIR-LABEL: cir.func{{.*}} @{{[^ (]*conditional_temporary_continue[^ (]*}}(
// CIR: %[[CONDITIONAL_CONTINUE_TEMP:.*]] = cir.alloca{{.*}}ast_temporary_object_identities = {{.*}}function = @{{[^ (]*conditional_temporary_continue[^ (]*}}{{.*}}instance_token = "[[CONDITIONAL_CONTINUE_TOKEN:mte\.instance\.[0-9]+]]"
// CIR: cir.if %[[CONDITIONAL_CONTINUE_FLAG:.*]] {
// CIR: cir.call @_ZN7CleanupD1Ev(%[[CONDITIONAL_CONTINUE_TEMP]])
// CIR: } {ast_conditional_cleanup_identities = {{.*}}instance_token = "[[CONDITIONAL_CONTINUE_TOKEN]]"
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
