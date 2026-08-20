// Selected declarations may name pre-C++17-style in-class static const
// integral members that have no out-of-class definition because their uses do
// not odr-use storage.  Selected CIR still needs the exact typed initializer,
// but it must not claim the strong definition that an out-of-class definition
// would own.
//
// The three roots model the campaign ownership shapes: an internal class member
// used as a value, an externally linked class member used as a value, and an
// internal class member used as a non-type template argument.
// RUN: printf '_ZN12_GLOBAL__N_128MetadataMultilayerEncodeTest17kNumSpatialLayersE\n_ZN2v88internal8compiler17MoveOptimizerTest6kF32_2E\n_ZN5blink12_GLOBAL__N_121LineBreakIteratorPool9kCapacityE\n' > %t.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++14 -fclangir -emit-cir -fclangir-emit-selected-decls=%t.roots -skip-function-bodies %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++14 -fclangir -emit-cir %s -o %t.ordinary.cir
// RUN: FileCheck %s --check-prefix=ORDINARY --implicit-check-not=@_ZN12_GLOBAL__N_128MetadataMultilayerEncodeTest17kNumSpatialLayersE --implicit-check-not=@_ZN2v88internal8compiler17MoveOptimizerTest6kF32_2E --implicit-check-not=@_ZN5blink12_GLOBAL__N_121LineBreakIteratorPool9kCapacityE --input-file=%t.ordinary.cir

namespace {
struct MetadataMultilayerEncodeTest {
  static const int kNumSpatialLayers = 3;
  int get() const { return kNumSpatialLayers; }
};
} // namespace

namespace v8 {
namespace internal {
namespace compiler {
struct MoveOptimizerTest {
  static const int kF32_2 = 5;
  int get() const { return kF32_2; }
};
} // namespace compiler
} // namespace internal
} // namespace v8

namespace blink {
namespace {
template <unsigned long Capacity>
struct FixedPool {
  int entries[Capacity];
};

struct LineBreakIteratorPool {
  static const unsigned long kCapacity = 4;
  FixedPool<kCapacity> pool;
};
} // namespace
} // namespace blink

// CHECK: cir.selected_decl_root_definitions = {
// CHECK-DAG: _ZN12_GLOBAL__N_128MetadataMultilayerEncodeTest17kNumSpatialLayersE = "_ZN12_GLOBAL__N_128MetadataMultilayerEncodeTest17kNumSpatialLayersE"
// CHECK-DAG: _ZN2v88internal8compiler17MoveOptimizerTest6kF32_2E = "_ZN2v88internal8compiler17MoveOptimizerTest6kF32_2E"
// CHECK-DAG: _ZN5blink12_GLOBAL__N_121LineBreakIteratorPool9kCapacityE = "_ZN5blink12_GLOBAL__N_121LineBreakIteratorPool9kCapacityE"
// CHECK-DAG: cir.global constant available_externally @_ZN12_GLOBAL__N_128MetadataMultilayerEncodeTest17kNumSpatialLayersE = #cir.int<3> : !s32i
// CHECK-DAG: cir.global constant available_externally @_ZN2v88internal8compiler17MoveOptimizerTest6kF32_2E = #cir.int<5> : !s32i
// CHECK-DAG: cir.global constant available_externally @_ZN5blink12_GLOBAL__N_121LineBreakIteratorPool9kCapacityE = #cir.int<4> : !u64i

// ORDINARY: module
