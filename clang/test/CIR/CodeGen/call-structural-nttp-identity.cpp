// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir %s -o - | FileCheck %s --implicit-check-not='ast_callee_usr = ""'

enum class ExternalPointerTag : unsigned { First = 2 };

template <typename Tag, Tag First, Tag Last>
struct TagRange {
  Tag first = First;
  Tag last = Last;
};

template <TagRange Range>
struct ExternalPointerMember {
  unsigned load() const { return static_cast<unsigned>(Range.first); }
};

unsigned invoke(ExternalPointerMember<TagRange<ExternalPointerTag,
                                               ExternalPointerTag::First,
                                               ExternalPointerTag::First>{}> *p) {
  return p->load();
}

// A class-valued template argument is represented by an unnamed
// TemplateParamObjectDecl. Its typed APValue, not that declaration's absent
// spelling, owns the exact specialization identity.
// CHECK: cir.func{{.*}} @[[LOAD:_ZNK21ExternalPointerMember[^ (]+4loadEv]](
// CHECK-SAME: ast_decl_usr = "[[USR:c:[^"]+]]"
// CHECK: cir.func{{.*}} @_Z6invoke
// CHECK: cir.call @[[LOAD]](
// CHECK-SAME: ast_callee_usr = "[[USR]]"
