// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR

// The producer must keep the direct Clang USR when one is available. For a
// local record with no USR, it must prefer stable complete source provenance
// over ABI RTTI-name mangling when that provenance is available.
template <class T>
void local_record_identity() {
  struct ScopedEvent436 {
    struct EventFinalizer {};
    EventFinalizer finalizer;
  } scoped_event;
  typename ScopedEvent436::EventFinalizer event_finalizer;
  (void)scoped_event;
  (void)event_finalizer;
}

void instantiate_local_record_identities() {
  local_record_identity<int>();
  local_record_identity<long>();
}

// The local record belongs to one template-pattern source site, but the two
// lambda NTTP arguments give its ABI RTTI name distinct lambda ordinals. The
// source identity must therefore be byte-for-byte identical across both
// instantiations rather than using that unstable ABI spelling.
template <auto Callback>
void lambda_nttp_record_identity() {
  [] {
    struct LambdaEventFinalizer {
      int value;
      void touch() {}
    };
    LambdaEventFinalizer finalizer{0};
    finalizer.touch();
  }();
}

void instantiate_lambda_nttp_record_identities() {
  lambda_nttp_record_identity<[] {}>();
  lambda_nttp_record_identity<[] {}>();
}

// The spelling location is shared by both expansions, while their expansion
// locations differ. v2 carries both locations so these macro records cannot
// collide.
#define DEFINE_LOCAL_MACRO_RECORD(Type, Variable)                              \
  struct Type {                                                                 \
    int value;                                                                  \
    void touch() {}                                                             \
  };                                                                            \
  Type Variable{0};                                                             \
  Variable.touch()

template <auto Callback>
void repeated_macro_record_identity() {
  [] {
    DEFINE_LOCAL_MACRO_RECORD(MacroEventFinalizerOne, first);
    DEFINE_LOCAL_MACRO_RECORD(MacroEventFinalizerTwo, second);
  }();
}

void instantiate_repeated_macro_record_identities() {
  repeated_macro_record_identity<[] {}>();
  repeated_macro_record_identity<[] {}>();
}

// CIR-NOT: structural:
// CIR: cir.empty_record_schemas = {{[^}]*}}"local_record_identity()::ScopedEvent436::EventFinalizer"{{[^}]*}}"local_record_identity()::ScopedEvent436::EventFinalizer.0"{{[^}]*}}
// CIR-DAG: ScopedEvent436 = "c:@F@local_record_identity<#I>#@S@ScopedEvent436"
// CIR-DAG: ScopedEvent436.0 = "c:@F@local_record_identity<#L>#@S@ScopedEvent436"
// CIR-DAG: "local_record_identity()::ScopedEvent436::EventFinalizer" = "c:@F@local_record_identity<#I>#@S@ScopedEvent436@S@EventFinalizer"
// CIR-DAG: "local_record_identity()::ScopedEvent436::EventFinalizer.0" = "c:@F@local_record_identity<#L>#@S@ScopedEvent436@S@EventFinalizer"
// CIR-DAG: {{[^,}]*LambdaEventFinalizer[^=]*}} = "[[LAMBDA_SOURCE_ID:cxx-source-record:v1:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+:struct:[0-9]+:[^"]*]]"
// CIR-DAG: {{[^,}]*LambdaEventFinalizer[^=]*}} = "[[LAMBDA_SOURCE_ID]]"
// CIR-DAG: {{[^,}]*MacroEventFinalizerOne[^=]*}} = "[[MACRO_ONE_SOURCE_ID:cxx-source-record:v2:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+:struct:[0-9]+:[^"]*]]"
// CIR-DAG: {{[^,}]*MacroEventFinalizerOne[^=]*}} = "[[MACRO_ONE_SOURCE_ID]]"
// CIR-DAG: {{[^,}]*MacroEventFinalizerTwo[^=]*}} = "[[MACRO_TWO_SOURCE_ID:cxx-source-record:v2:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+:struct:[0-9]+:[^"]*]]"
// CIR-DAG: {{[^,}]*MacroEventFinalizerTwo[^=]*}} = "[[MACRO_TWO_SOURCE_ID]]"
// CIR-NOT: cxx-rtti-name:
// CIR-NOT: structural:
