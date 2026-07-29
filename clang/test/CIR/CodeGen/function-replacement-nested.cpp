// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fbracket-depth 2048 \
// RUN:   -mconstructor-aliases -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck %s --check-prefix=CIR --input-file=%t.cir \
// RUN:   --implicit-check-not=@_ZN15ReplacementLeafC1Ev \
// RUN:   --implicit-check-not=@_ZN15ReplacementLeafD1Ev \
// RUN:   --implicit-check-not=@_ZN16ReplacementOwnerC1Ev \
// RUN:   --implicit-check-not=@_ZN16ReplacementOwnerD1Ev
// RUN: FileCheck %s --check-prefix=NEST --input-file=%t.cir
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fbracket-depth 2048 \
// RUN:   -mconstructor-aliases -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck %s --check-prefix=LLVM --input-file=%t.ll \
// RUN:   --implicit-check-not=@_ZN15ReplacementLeafC1Ev \
// RUN:   --implicit-check-not=@_ZN15ReplacementLeafD1Ev \
// RUN:   --implicit-check-not=@_ZN16ReplacementOwnerC1Ev \
// RUN:   --implicit-check-not=@_ZN16ReplacementOwnerD1Ev

struct ReplacementLeaf {
  int value;
  ReplacementLeaf() : value(7) {}
  ~ReplacementLeaf() { value = 0; }
};

struct ReplacementOwner {
  ReplacementLeaf leaf;
  ReplacementOwner() {}
  ~ReplacementOwner() {}
};

// Keep the source small while producing a deeply nested operation tree. Inline
// structors use RAUW on ELF: all four complete variants are recorded for
// replacement with their base variants. The old SymbolUserMap-based module
// traversal recursively visited this whole tree while holding users across
// function erasure.
#define NEST_1(BODY) if (enter) { BODY }
#define NEST_2(BODY) NEST_1(NEST_1(BODY))
#define NEST_4(BODY) NEST_2(NEST_2(BODY))
#define NEST_8(BODY) NEST_4(NEST_4(BODY))
#define NEST_16(BODY) NEST_8(NEST_8(BODY))
#define NEST_32(BODY) NEST_16(NEST_16(BODY))
#define NEST_64(BODY) NEST_32(NEST_32(BODY))
#define NEST_128(BODY) NEST_64(NEST_64(BODY))
#define NEST_256(BODY) NEST_128(NEST_128(BODY))
#define NEST_512(BODY) NEST_256(NEST_256(BODY))

void exercise_replacements(bool enter) {
  NEST_512(ReplacementOwner owner;)
}

// Every complete structor declaration is replaced and erased. References in
// both the deeply nested caller and the replacement targets name the surviving
// base variants.
// CIR-DAG: cir.func{{.*}} @_ZN15ReplacementLeafC2Ev(
// CIR-DAG: cir.func{{.*}} @_ZN15ReplacementLeafD2Ev(
// CIR-DAG: cir.func{{.*}} @_ZN16ReplacementOwnerC2Ev(
// CIR-DAG: cir.func{{.*}} @_ZN16ReplacementOwnerD2Ev(
// CIR-DAG: cir.call @_ZN15ReplacementLeafC2Ev(
// CIR-DAG: cir.call @_ZN15ReplacementLeafD2Ev(
// CIR-DAG: cir.call @_ZN16ReplacementOwnerC2Ev(
// CIR-DAG: cir.call @_ZN16ReplacementOwnerD2Ev(

// NEST-LABEL: cir.func{{.*}} @_Z21exercise_replacementsb(
// NEST-COUNT-512: cir.if
// NEST: cir.call @_ZN16ReplacementOwnerC2Ev(
// NEST: cir.call @_ZN16ReplacementOwnerD2Ev(

// LLVM-DAG: define linkonce_odr void @_ZN15ReplacementLeafC2Ev(
// LLVM-DAG: define linkonce_odr void @_ZN15ReplacementLeafD2Ev(
// LLVM-DAG: define linkonce_odr void @_ZN16ReplacementOwnerC2Ev(
// LLVM-DAG: define linkonce_odr void @_ZN16ReplacementOwnerD2Ev(
// LLVM-DAG: call void @_ZN15ReplacementLeafC2Ev(
// LLVM-DAG: call void @_ZN15ReplacementLeafD2Ev(
// LLVM-DAG: call void @_ZN16ReplacementOwnerC2Ev(
// LLVM-DAG: call void @_ZN16ReplacementOwnerD2Ev(
