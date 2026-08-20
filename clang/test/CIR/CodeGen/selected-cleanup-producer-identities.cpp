// RUN: echo "_Z19selected_conversionI3BigEbRKT_" > %t.conversion.roots
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -fclangir-emit-selected-decls=%t.conversion.roots -skip-function-bodies %s -o %t.conversion.cir
// RUN: FileCheck --check-prefix=CONVERSION --input-file=%t.conversion.cir %s
// RUN: echo "_Z27selected_structured_cleanupv" > %t.automatic.roots
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir -fclangir-emit-selected-decls=%t.automatic.roots -skip-function-bodies %s -o %t.automatic.cir
// RUN: FileCheck --check-prefix=AUTOMATIC --input-file=%t.automatic.cir %s

struct Big {
  int value;
};

struct Small {
  int value;
  Small(const Big &big) : value(big.value) {}
  ~Small();
};

template <class T> struct View {
  using const_reference = const T &;
  static const_reference ConstReference(const T &value) { return value; }
};

template <class Lhs> bool selected_conversion(const Lhs &lhs) {
  using LhsView = View<Lhs>;
  using ConvertedReference = const Small &;
  ConvertedReference converted = LhsView::ConstReference(lhs);
  return converted.value != 0;
}

using SelectedConversion = bool (*)(const Big &);
SelectedConversion force_selected_conversion = &selected_conversion<Big>;

// CONVERSION-LABEL: cir.func{{.*}} @_Z19selected_conversionI3BigEbRKT_
// CONVERSION: cir.alloca "ref.tmp
// CONVERSION-SAME: ast_temporary_object_identities
// CONVERSION-SAME: declaration_ordinal = 4611686018427387904 : i64
// CONVERSION-SAME: producer_kind = "materialized_conversion"

struct Item {
  Item();
  ~Item();
};

struct Pair {
  Item first;
  Item second;
};

Pair make_pair();

void selected_structured_cleanup() {
  auto [first, second] = make_pair();
}
// AUTOMATIC-LABEL: cir.func{{.*}} @_Z27selected_structured_cleanupv
// AUTOMATIC: cir.alloca
// AUTOMATIC-SAME: ast_automatic_object_identity
// AUTOMATIC-SAME: declaration_ordinal = 0 : i64
// AUTOMATIC-SAME: transferred_to_automatic_decl_ordinal = 0 : i64
