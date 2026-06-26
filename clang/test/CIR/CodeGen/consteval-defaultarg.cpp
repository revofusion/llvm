// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s --check-prefix=LLVM
// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=OGCG

struct SourceLocation {
  consteval static SourceLocation current() noexcept { return {}; }
};

void sink(const SourceLocation &loc = SourceLocation::current());

void test_consteval_default_arg() {
  sink();
}

namespace std {
class source_location {
  struct __impl {
    const char *_M_file_name;
    const char *_M_function_name;
    unsigned _M_line;
    unsigned _M_column;
  };
  const __impl *__ptr_ = nullptr;
  using __bsl_ty = decltype(__builtin_source_location());

public:
  static consteval source_location
  current(__bsl_ty __ptr = __builtin_source_location()) noexcept {
    source_location __sl;
    __sl.__ptr_ = static_cast<const __impl *>(__ptr);
    return __sl;
  }
};
}

void sink_builtin(const std::source_location &loc =
                      std::source_location::current());

void test_builtin_source_location_default_arg() {
  sink_builtin();
}

// CIR-LABEL: cir.func {{.*}} @_Z26test_consteval_default_argv()
// CIR: %[[TMP:.*]] = cir.alloca !rec_SourceLocation, !cir.ptr<!rec_SourceLocation>, ["ref.tmp0"]
// CIR: %[[CONST:.*]] = cir.const
// CIR: cir.store{{.*}} %[[CONST]], %[[TMP]]
// CIR: cir.call @_Z4sinkRK14SourceLocation(%[[TMP]])
// CIR-NOT: current
// CIR-LABEL: cir.func {{.*}} @{{.*test_builtin_source_location_default_arg.*}}()
// CIR: cir.store
// CIR: cir.call @{{.*sink_builtin.*}}
// CIR-NOT: current

// LLVM-LABEL: define{{.*}} @_Z26test_consteval_default_argv()
// LLVM: %[[TMP:.*]] = alloca %struct.SourceLocation
// LLVM: call void @_Z4sinkRK14SourceLocation(ptr {{.*}}%[[TMP]])
// LLVM-NOT: current
// LLVM-LABEL: define{{.*}} @{{.*test_builtin_source_location_default_arg.*}}()
// LLVM: store
// LLVM: call void @{{.*sink_builtin.*}}
// LLVM-NOT: current

// OGCG-LABEL: define{{.*}} @_Z26test_consteval_default_argv()
// OGCG: %[[TMP:.*]] = alloca %struct.SourceLocation
// OGCG: call void @_Z4sinkRK14SourceLocation(ptr {{.*}}%[[TMP]])
// OGCG-NOT: current
// OGCG-LABEL: define{{.*}} @{{.*test_builtin_source_location_default_arg.*}}()
// OGCG: store
// OGCG: call void @{{.*sink_builtin.*}}
// OGCG-NOT: current
