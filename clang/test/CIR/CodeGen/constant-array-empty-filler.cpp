// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck %s --input-file=%t.cir --check-prefix=CIR
// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck %s --input-file=%t.ll --check-prefix=LLVM

// Model libc++'s physical storage for array<T, 0>. The Empty[8] member has no
// explicit APValue elements; constant evaluation represents it with a typed
// Empty filler.
struct Empty {};

template <class T, unsigned long Size>
struct Array {
  T elems[Size];
};

template <class T>
struct Array<T, 0> {
  struct ArrayInStruct {
    T data[1];
  };

  alignas(ArrayInStruct) Empty elems[sizeof(ArrayInStruct)];
};

template <class Self, class... Deps>
struct Plugin {
  inline static constexpr Array<const void *, sizeof...(Deps)> kDepIds{
      {Deps::id...}};
};

struct Tag {};
using EmptyPlugin = Plugin<Tag>;

// Re-encounter the same inline static template declaration through two deferred
// uses. Its declaration type and constant initializer type must agree.
auto get_dep_ids_once() { return &EmptyPlugin::kDepIds; }
auto get_dep_ids_again() { return &EmptyPlugin::kDepIds; }

// The zero-size specialization remains a named outer record containing a
// homogeneous physical array, rather than an anonymous packed record.
// CIR-DAG: ![[EMPTY:[^ ]+]] = !cir.struct<"Empty" padded {!u8i}>
// CIR-DAG: ![[ARRAY:[^ ]+]] = !cir.struct<"Array<{{.*}}>" {!cir.array<![[EMPTY]] x 8>}>
// CIR: cir.global constant linkonce_odr comdat @[[DEP_IDS:_ZN6PluginI3TagJEE7kDepIdsE]] = #cir.const_record<{#cir.const_array<[#cir.undef, #cir.undef, #cir.undef, #cir.undef, #cir.undef, #cir.undef, #cir.undef, #cir.undef]>}> : ![[ARRAY]]
// CIR-NOT: cir.global {{.*}} @[[DEP_IDS]]

// Both uses must resolve to that one named-typed global.
// CIR-LABEL: cir.func{{.*}} @_Z16get_dep_ids_oncev
// CIR: cir.get_global @[[DEP_IDS]] : !cir.ptr<![[ARRAY]]>
// CIR-LABEL: cir.func{{.*}} @_Z17get_dep_ids_againv
// CIR: cir.get_global @[[DEP_IDS]] : !cir.ptr<![[ARRAY]]>

// LLVM-DAG: [[LLVM_EMPTY:%struct.Empty[^ ]*]] = type { i8 }
// LLVM-DAG: [[LLVM_ARRAY:%"[^"]+"]] = type { [8 x [[LLVM_EMPTY]]] }
// LLVM: @_ZN6PluginI3TagJEE7kDepIdsE = linkonce_odr constant [[LLVM_ARRAY]] {{.*}}, comdat
