// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++17 -fclangir -emit-cir %s -o - | FileCheck %s

struct BindStateBase {};

template <class T>
struct BindStorage : BindStateBase {
  T value;
};

template <class T>
BindStorage<T> *recover_bind_storage(BindStateBase *base) {
  return static_cast<BindStorage<T> *>(base);
}
template BindStorage<int> *recover_bind_storage<int>(BindStateBase *);

struct GeneratedMessage {};
struct GeneratedMessageDefaultTypeInternal;
extern GeneratedMessageDefaultTypeInternal generated_message_default_instance;

const GeneratedMessage &generated_message_default() {
  return reinterpret_cast<const GeneratedMessage &>(
      generated_message_default_instance);
}

typedef struct opaque_form_handle_t__ *OpaqueFormHandle;
struct FormEnvironment {};

FormEnvironment *recover_form_environment(OpaqueFormHandle handle) {
  return reinterpret_cast<FormEnvironment *>(handle);
}

void consume_bytes(const void *);
void erase_handle_storage(OpaqueFormHandle *handle) {
  consume_bytes(handle);
}

struct LateValue {};
template <class T>
struct LateMatcher;
LateMatcher<LateValue> *late_matcher_pointer;

template <class T>
struct LateMatcher {
  T value;
};
struct LateMatcherView {};

LateMatcherView *refresh_late_matcher(LateMatcher<LateValue> *matcher) {
  return reinterpret_cast<LateMatcherView *>(matcher);
}

template <class T>
struct MatcherInterface {
  virtual ~MatcherInterface() = default;
};
struct StringValue {};
struct StringMatcher : MatcherInterface<StringValue> {};

StringMatcher *recover_specialized_matcher(
    MatcherInterface<StringValue> *matcher) {
  return static_cast<StringMatcher *>(matcher);
}

bool specialized_matcher_present(
    MatcherInterface<StringValue> *matcher) {
  return matcher;
}

struct AnonymousCastOwner {
  struct {
    int value;
  } state;
};
using AnonymousCastState = decltype(AnonymousCastOwner::state);

void *erase_anonymous_cast(AnonymousCastOwner *owner) {
  return &owner->state;
}

AnonymousCastState *recover_anonymous_cast(void *value) {
  return static_cast<AnonymousCastState *>(value);
}

// The specialization was first converted for an incomplete global pointee and
// completed later. Its module record-name binding must be refreshed to the
// exact same final identity attached to the cast endpoint below.
// CHECK-DAG: "BindStorage<int>" = "[[BIND_STORAGE_USR:[^"]+]]"
// CHECK-DAG: "LateMatcher<LateValue>" = "[[LATE_MATCHER_USR:[^"]+]]"
// CHECK-DAG: "MatcherInterface<StringValue>" = "[[MATCHER_INTERFACE_USR:[^"]+]]"
// CHECK-DAG: [[ANON_SCHEMA:!rec_[A-Za-z0-9_]+]] = !cir.struct<"[[ANON_NAME:anon\.[0-9]+]]"
// CHECK-DAG: [[ANON_NAME]] = "[[ANON_USR:[^"]+]]"

// A zero-offset base-to-derived cast can lower the structural source endpoint
// to byte storage later, but the direct AST endpoints remain a mutually bound
// specialization schema/USR pair.
// CHECK-LABEL: cir.func{{.*}}@_Z20recover_bind_storageIiEP11BindStorageIT_EP13BindStateBase
// CHECK: cir.derived_class_addr{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BaseToDerived"
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = [[BIND_STORAGE_SCHEMA:!rec_[^, }]+]]
// CHECK-SAME: result_record_usr = "[[BIND_STORAGE_USR]]"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = !rec_BindStateBase
// CHECK-SAME: source_record_usr = "c:@S@BindStateBase"

// Generated protobuf default instances expose only a forward-declared wrapper
// in headers. LValueBitCast still carries that opaque source declaration and
// the complete message declaration as two exact, distinct endpoints.
// CHECK-LABEL: cir.func{{.*}}@_Z25generated_message_defaultv
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "LValueBitCast"
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = !rec_GeneratedMessage
// CHECK-SAME: result_record_usr = "c:@S@GeneratedMessage"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = !rec_GeneratedMessageDefaultTypeInternal
// CHECK-SAME: source_record_usr = "c:@S@GeneratedMessageDefaultTypeInternal"

// An opaque C API handle is still a real incomplete RecordDecl endpoint. It
// must not be treated as an unauthenticated byte pointer when recovered as an
// implementation object pointer.
// CHECK-LABEL: cir.func{{.*}}@_Z24recover_form_environmentP22opaque_form_handle_t__
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = !rec_FormEnvironment
// CHECK-SAME: result_record_usr = "c:@S@FormEnvironment"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = !rec_opaque_form_handle_t__
// CHECK-SAME: source_record_usr = "c:@S@opaque_form_handle_t__"

// A handle's backing RecordDecl remains the terminal source endpoint through
// multiple pointer layers when call conversion erases the storage to void.
// CHECK-LABEL: cir.func{{.*}}@_Z20erase_handle_storagePP22opaque_form_handle_t__
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: result_record_presence = "no_record"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = !rec_opaque_form_handle_t__
// CHECK-SAME: source_record_usr = "c:@S@opaque_form_handle_t__"

// CHECK-LABEL: cir.func{{.*}}@_Z20refresh_late_matcherP11LateMatcherI9LateValueE
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BitCast"
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = !rec_LateMatcherView
// CHECK-SAME: result_record_usr = "c:@S@LateMatcherView"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = {{!rec_LateMatcher[^, }]*}}
// CHECK-SAME: source_record_usr = "[[LATE_MATCHER_USR]]"

// Template-specialization USRs can acquire ODR/argument-owner discriminators
// after implicit instantiation. The schema and refreshed module identity must
// be emitted from the same final RecordDecl.
// CHECK-LABEL: cir.func{{.*}}@_Z27recover_specialized_matcherP16MatcherInterfaceI11StringValueE
// CHECK: cir.derived_class_addr{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "BaseToDerived"
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = !rec_StringMatcher
// CHECK-SAME: result_record_usr = "c:@S@StringMatcher"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = [[MATCHER_INTERFACE_SCHEMA:!rec_MatcherInterface[^, }]*]]
// CHECK-SAME: source_record_usr = "[[MATCHER_INTERFACE_USR]]"

// unique_ptr::reset tests a specialization pointer directly. Pointer-to-bool
// erasure retains the exact source schema and its specialization identity.
// CHECK-LABEL: cir.func{{.*}}@_Z27specialized_matcher_presentP16MatcherInterfaceI11StringValueE
// CHECK: cir.cast ptr_to_bool{{.*}}ast_cast_expr = {
// CHECK-SAME: cast_kind = "PointerToBoolean"
// CHECK-SAME: result_record_presence = "no_record"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = [[MATCHER_INTERFACE_SCHEMA]]
// CHECK-SAME: source_record_usr = "[[MATCHER_INTERFACE_USR]]"

// A field-owned anonymous RecordDecl has no source spelling from which a
// consumer can rebuild its declaration. Both erase and recover casts must
// carry the same producer schema/identity pair, in opposite endpoint roles.
// CHECK-LABEL: cir.func{{.*}}@_Z{{[0-9]+}}erase_anonymous_cast
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: result_record_presence = "no_record"
// CHECK-SAME: source_record_presence = "record"
// CHECK-SAME: source_record_schema = [[ANON_SCHEMA]]
// CHECK-SAME: source_record_usr = "[[ANON_USR]]"
// CHECK-LABEL: cir.func{{.*}}@_Z{{[0-9]+}}recover_anonymous_cast
// CHECK: cir.cast bitcast{{.*}}ast_cast_expr = {
// CHECK-SAME: result_record_presence = "record"
// CHECK-SAME: result_record_schema = [[ANON_SCHEMA]]
// CHECK-SAME: result_record_usr = "[[ANON_USR]]"
// CHECK-SAME: source_record_presence = "no_record"
