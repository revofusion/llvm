// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: printf '_ZN14MemberTemplateIiE5valueEv\n' > %t.member-roots
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-cir -fclangir-emit-selected-decls=%t.member-roots -skip-function-bodies %s -o %t-member.cir
// RUN: FileCheck --input-file=%t-member.cir %s -check-prefix=MEMBER
// RUN: printf '_Z15make_angle_testv\n_Z11fuzzOverlapv\n_ZN17callable_identity18useDecoderTemplateEv\n_Z17useNestedMetadatav\n' > %t.callable-roots
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-cir -fclangir-emit-selected-decls=%t.callable-roots -skip-function-bodies %s -o %t-callable.cir
// RUN: FileCheck --input-file=%t-callable.cir %s -check-prefix=CALLABLE-SELECTED
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s -check-prefix=LLVM
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -Wno-unused-value -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s -check-prefix=OGCG

template<typename T, typename U>
class Templ {};

template<typename T>
class Templ<T, int>{};

Templ<int, int> t;

// CIR: !rec_Templ3Cint2C_int3E = !cir.struct<class "Templ<int, int>" padded {!u8i}>
// CIR: cir.global external @t = #cir.zero : !rec_Templ3Cint2C_int3E

// LLVM: %"class.Templ<int, int>" = type { i8 }
// LLVM: @t = global %"class.Templ<int, int>" zeroinitializer

// OGCG: %class.Templ = type { i8 }
// OGCG: @t = global %class.Templ zeroinitializer

template<class T>
class X {
public:
  int f() { return 0; }
};

template<> class X<int> {
public:
  int f() { return 1; }
};

void test_double() {
  X<double> d;
  d.f();
}

// CIR: cir.func{{.*}} @_ZN1XIdE1fEv
// CIR:   cir.const #cir.int<0>
//
// CIR: cir.func{{.*}} @_Z11test_doublev()
// CIR:   cir.call @_ZN1XIdE1fEv

// LLVM: define{{.*}} i32 @_ZN1XIdE1fEv
// LLVM:   store i32 0
//
// LLVM: define{{.*}} void @_Z11test_doublev()
// LLVM:   call{{.*}} i32 @_ZN1XIdE1fEv

// OGCG: define{{.*}} void @_Z11test_doublev()
// OGCG:   call{{.*}} i32 @_ZN1XIdE1fEv
//
// OGCG: define{{.*}} i32 @_ZN1XIdE1fEv
// OGCG:   ret i32 0

void test_int() {
  X<int> n;
  n.f();
}

// CIR: cir.func{{.*}} @_ZN1XIiE1fEv
// CIR:   cir.const #cir.int<1>
//
// CIR: cir.func{{.*}} @_Z8test_intv()
// CIR:   cir.call @_ZN1XIiE1fEv

// LLVM: define{{.*}} i32 @_ZN1XIiE1fEv
// LLVM:   store i32 1
//
// LLVM: define{{.*}} void @_Z8test_intv()
// LLVM:   call{{.*}} i32 @_ZN1XIiE1fEv

// OGCG: define{{.*}} void @_Z8test_intv()
// OGCG:   call{{.*}} i32 @_ZN1XIiE1fEv
//
// OGCG: define{{.*}} i32 @_ZN1XIiE1fEv
// OGCG:   ret i32 1

void test_short() {
  X<short> s;
  s.f();
}

// CIR: cir.func{{.*}} @_ZN1XIsE1fEv
// CIR:   cir.const #cir.int<0>
//
// CIR: cir.func{{.*}} @_Z10test_shortv()
// CIR:   cir.call @_ZN1XIsE1fEv

// LLVM: define{{.*}} i32 @_ZN1XIsE1fEv
// LLVM: store i32 0
//
// LLVM: define{{.*}} void @_Z10test_shortv()
// LLVM:   call{{.*}} i32 @_ZN1XIsE1fEv

// OGCG: define{{.*}} void @_Z10test_shortv()
// OGCG:   call{{.*}} i32 @_ZN1XIsE1fEv
//
// OGCG: define{{.*}} i32 @_ZN1XIsE1fEv
// OGCG:   ret i32 0

// Two lambda NTTP values instantiate this same template pattern to identical
// CIR signatures. The source point of instantiation is an exact producer fact
// that distinguishes the otherwise ambiguous specializations.
template <auto Callback>
void same_signature_nttp() {}

void instantiate_same_signature_nttp() {
  same_signature_nttp<[] {}>();
  same_signature_nttp<[] {}>();
}

// CIR: cir.func{{.*}} @[[NTTP_ONE:[^ (]*same_signature_nttp[^ (]*]]()
// CIR-SAME: ast_decl_specialization_identity = {{.*}}mangled_name = "[[NTTP_ONE]]"{{.*}}poi = "[[NTTP_ONE_POI:v1:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+]]"{{.*}}template_arguments_odr_hash = {{[0-9]+}} : i64{{.*}}template_pattern_usr = "[[NTTP_PATTERN:[^"]+]]"
// CIR: cir.func{{.*}} @[[NTTP_TWO:[^ (]*same_signature_nttp[^ (]*]]()
// CIR-NOT: poi = "[[NTTP_ONE_POI]]"
// CIR-SAME: ast_decl_specialization_identity = {{.*}}mangled_name = "[[NTTP_TWO]]"{{.*}}poi = "[[NTTP_TWO_POI:v1:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+]]"{{.*}}template_arguments_odr_hash = {{[0-9]+}} : i64{{.*}}template_pattern_usr = "[[NTTP_PATTERN]]"
 
template <typename T>
struct MemberTemplate {
  int value();
};

template <typename T>
__attribute__((used)) int MemberTemplate<T>::value() {
  return 7;
}

template struct MemberTemplate<int>;

void instantiate_member_template() {
  MemberTemplate<int> value;
  value.value();
}

// MEMBER: cir.func{{.*}} @[[MEMBER_SYMBOL:[^ (]*MemberTemplateIiE5valueEv]](
// MEMBER-SAME: ast_decl_specialization_identity = {{.*}}mangled_name = "[[MEMBER_SYMBOL]]"{{.*}}poi = "[[MEMBER_POI:v1:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+]]"{{.*}}template_pattern_usr = "[[MEMBER_PATTERN:[^"]+]]"

template <typename T>
struct CtorIdentity {
  CtorIdentity(T value);
  T value;
};

template <typename T>
CtorIdentity<T>::CtorIdentity(T value) : value(value) {}

template struct CtorIdentity<int>;

CtorIdentity<int> make_ctor_identity() {
  return CtorIdentity<int>(1);
}

// Each constructor ABI entry point carries its own exact producer symbol.
// CIR-DAG: cir.func{{.*}} @_ZN12CtorIdentityIiEC2Ei({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "_ZN12CtorIdentityIiEC2Ei"
// CIR-DAG: cir.func{{.*}} @_ZN12CtorIdentityIiEC1Ei({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "_ZN12CtorIdentityIiEC1Ei"

template <typename T>
struct ImplicitCtorIdentity {
  T value = {};
  virtual T read() const { return value; }
};

ImplicitCtorIdentity<int> make_implicit_ctor_identity() {
  return ImplicitCtorIdentity<int>();
}

// An implicit special member of a concrete class-template specialization has
// the same exact producer specialization identity as a written member.
// CIR-DAG: cir.func{{.*}} @_ZN20ImplicitCtorIdentityIiEC2Ev({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "_ZN20ImplicitCtorIdentityIiEC2Ev"
// CIR-DAG: cir.func{{.*}} @_ZN20ImplicitCtorIdentityIiEC1Ev({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "_ZN20ImplicitCtorIdentityIiEC1Ev"

// ANGLE's test base instantiates an implicit constructor from a concrete
// class-template specialization.  C1 and C2 share one source declaration, so
// each FuncOp must carry both its exact ABI variant and the canonical declaring
// RecordDecl identity.
struct PlatformParameters {
  int value = 0;
};

template <typename Parameters>
struct ANGLETest {
  Parameters parameters;
  virtual int read() const { return parameters.value; }
};

ANGLETest<PlatformParameters> make_angle_test() {
  return ANGLETest<PlatformParameters>();
}

// CIR-DAG: cir.func{{.*}} @_ZN9ANGLETestI18PlatformParametersEC2Ev({{.*}}abi_ctor_variant = "base"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[ANGLE_CLASS:[^"]+]]"{{.*}}method_symbol = @_ZN9ANGLETestI18PlatformParametersEC2Ev
// CIR-DAG: cir.func{{.*}} @_ZN9ANGLETestI18PlatformParametersEC1Ev({{.*}}abi_ctor_variant = "complete"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[ANGLE_CLASS]]"{{.*}}method_symbol = @_ZN9ANGLETestI18PlatformParametersEC1Ev

// The same exact identities must survive selected-root emission and its ABI
// dependency closure rather than existing only in full-TU CIR.
// CALLABLE-SELECTED-DAG: cir.func{{.*}} @_ZN9ANGLETestI18PlatformParametersEC2Ev({{.*}}abi_ctor_variant = "base"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[SELECTED_ANGLE_CLASS:[^"]+]]"{{.*}}method_symbol = @_ZN9ANGLETestI18PlatformParametersEC2Ev
// CALLABLE-SELECTED-DAG: cir.func{{.*}} @_ZN9ANGLETestI18PlatformParametersEC1Ev({{.*}}abi_ctor_variant = "complete"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[SELECTED_ANGLE_CLASS]]"{{.*}}method_symbol = @_ZN9ANGLETestI18PlatformParametersEC1Ev

// Fuzztest's OverlapOf implementation creates concrete specializations of a
// generic lambda call operator.  The function-template tuple and the closure
// RecordDecl are independent exact producer facts; both are required.
int fuzzOverlap() {
  auto generic = []<typename T>(T value) { return static_cast<int>(value); };
  return generic(1) + generic(2L);
}

// CIR-DAG: cir.func{{.*}} @[[FUZZ_INT:[^ (]*fuzzOverlap[^ (]*clIiE[^ (]*]]({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "[[FUZZ_INT]]"{{.*}}template_arguments_odr_hash = {{[0-9]+}} : i64{{.*}}template_pattern_usr = "[[FUZZ_PATTERN:[^"]+]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[FUZZ_CLASS:[^"]+]]"{{.*}}method_symbol = @[[FUZZ_INT]]
// CIR-DAG: cir.func{{.*}} @[[FUZZ_LONG:[^ (]*fuzzOverlap[^ (]*clIlE[^ (]*]]({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "[[FUZZ_LONG]]"{{.*}}template_arguments_odr_hash = {{[0-9]+}} : i64{{.*}}template_pattern_usr = "[[FUZZ_PATTERN]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[FUZZ_CLASS]]"{{.*}}method_symbol = @[[FUZZ_LONG]]

// CALLABLE-SELECTED-DAG: cir.func{{.*}} @[[SELECTED_FUZZ_INT:[^ (]*fuzzOverlap[^ (]*clIiE[^ (]*]]({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "[[SELECTED_FUZZ_INT]]"{{.*}}template_arguments_odr_hash = {{[0-9]+}} : i64{{.*}}template_pattern_usr = "[[SELECTED_FUZZ_PATTERN:[^"]+]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[SELECTED_FUZZ_CLASS:[^"]+]]"{{.*}}method_symbol = @[[SELECTED_FUZZ_INT]]
// CALLABLE-SELECTED-DAG: cir.func{{.*}} @[[SELECTED_FUZZ_LONG:[^ (]*fuzzOverlap[^ (]*clIlE[^ (]*]]({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "[[SELECTED_FUZZ_LONG]]"{{.*}}template_arguments_odr_hash = {{[0-9]+}} : i64{{.*}}template_pattern_usr = "[[SELECTED_FUZZ_PATTERN]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[SELECTED_FUZZ_CLASS]]"{{.*}}method_symbol = @[[SELECTED_FUZZ_LONG]]

namespace callable_identity {
namespace {
template <typename Decoder>
struct DecoderTemplateTest {
  int CreateConfig() { return sizeof(Decoder); }
};
} // namespace

int useDecoderTemplate() {
  return DecoderTemplateTest<int>().CreateConfig();
}

// An anonymous-namespace class-template member specialization is authenticated
// by its exact internal-linkage symbol, canonical method USR, template pattern,
// and concrete declaring RecordDecl.  No qualified-name reconstruction is
// sufficient for this shape.
// CIR-DAG: cir.func{{.*}} @[[DECODER:[^ (]*DecoderTemplateTestIiE12CreateConfigEv]]({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "[[DECODER]]"{{.*}}template_pattern_usr = "[[DECODER_PATTERN:[^"]+]]"{{.*}}usr = "[[DECODER_USR:[^"]+]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[DECODER_CLASS:[^"]+]]"{{.*}}method_symbol = @[[DECODER]]{{.*}}method_usr = "[[DECODER_USR]]"

// CALLABLE-SELECTED-DAG: cir.func{{.*}} @[[SELECTED_DECODER:[^ (]*DecoderTemplateTestIiE12CreateConfigEv]]({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "[[SELECTED_DECODER]]"{{.*}}template_pattern_usr = "[[SELECTED_DECODER_PATTERN:[^"]+]]"{{.*}}usr = "[[SELECTED_DECODER_USR:[^"]+]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[SELECTED_DECODER_CLASS:[^"]+]]"{{.*}}method_symbol = @[[SELECTED_DECODER]]{{.*}}method_usr = "[[SELECTED_DECODER_USR]]"
} // namespace callable_identity

template <typename UI>
struct SidePanelWebUIViewT {
  struct SidePanelWebUIViewT_MetaData {
    static int BuildMetaData() { return sizeof(UI); }
  };
};

int useNestedMetadata() {
  return SidePanelWebUIViewT<PlatformParameters>::
      SidePanelWebUIViewT_MetaData::BuildMetaData();
}

// A method of a nested record instantiated below a class template is not
// itself owned directly by ClassTemplateSpecializationDecl.  Clang's exact
// instantiated-member relation plus the nested declaring RecordDecl must
// survive into CIR.
// CIR-DAG: cir.func{{.*}} @[[METADATA:[^ (]*SidePanelWebUIViewTI18PlatformParametersE28SidePanelWebUIViewT_MetaData13BuildMetaDataEv]]({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "[[METADATA]]"{{.*}}template_pattern_usr = "[[METADATA_PATTERN:[^"]+]]"{{.*}}usr = "[[METADATA_USR:[^"]+]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[METADATA_CLASS:[^"]+]]"{{.*}}method_symbol = @[[METADATA]]{{.*}}method_usr = "[[METADATA_USR]]"

// CALLABLE-SELECTED-DAG: cir.func{{.*}} @[[SELECTED_METADATA:[^ (]*SidePanelWebUIViewTI18PlatformParametersE28SidePanelWebUIViewT_MetaData13BuildMetaDataEv]]({{.*}}ast_decl_specialization_identity = {{.*}}mangled_name = "[[SELECTED_METADATA]]"{{.*}}template_pattern_usr = "[[SELECTED_METADATA_PATTERN:[^"]+]]"{{.*}}usr = "[[SELECTED_METADATA_USR:[^"]+]]"{{.*}}ast_method_callable_identity = {{.*}}method_declaring_class_usr = "[[SELECTED_METADATA_CLASS:[^"]+]]"{{.*}}method_symbol = @[[SELECTED_METADATA]]{{.*}}method_usr = "[[SELECTED_METADATA_USR]]"
