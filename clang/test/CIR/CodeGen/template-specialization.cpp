// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: printf '_ZN14MemberTemplateIiE5valueEv\n' > %t.member-roots
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-cir -fclangir-emit-selected-decls=%t.member-roots -skip-function-bodies %s -o %t-member.cir
// RUN: FileCheck --input-file=%t-member.cir %s -check-prefix=MEMBER
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
// CIR-SAME: ast_decl_specialization_identity = {{.*}}mangled_name = "[[NTTP_ONE]]"{{.*}}poi = "[[NTTP_ONE_POI:v1:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+]]"{{.*}}template_pattern_usr = "[[NTTP_PATTERN:[^"]+]]"
// CIR: cir.func{{.*}} @[[NTTP_TWO:[^ (]*same_signature_nttp[^ (]*]]()
// CIR-NOT: poi = "[[NTTP_ONE_POI]]"
// CIR-SAME: ast_decl_specialization_identity = {{.*}}mangled_name = "[[NTTP_TWO]]"{{.*}}poi = "[[NTTP_TWO_POI:v1:[0-9]+:[^:]+:[0-9]+:[0-9]+:[0-9]+]]"{{.*}}template_pattern_usr = "[[NTTP_PATTERN]]"
 
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
