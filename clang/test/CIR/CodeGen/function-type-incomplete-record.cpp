// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

namespace std {
template <class T> class unique_ptr {
  T *ptr;

public:
  unique_ptr();
  ~unique_ptr();
};
}

namespace v8 {
class CppHeap;
using ReleaseCppHeapCallback = void (*)(std::unique_ptr<CppHeap>);
void SetReleaseCppHeapCallbackForTesting(ReleaseCppHeapCallback callback);
}

void callit(v8::ReleaseCppHeapCallback cb) {
  v8::SetReleaseCppHeapCallbackForTesting(cb);
}

// CHECK: [[REC:!rec_[[:alnum:]_]+]] = !cir.record<class "std::unique_ptr<v8::CppHeap>" incomplete>
// CHECK: cir.func private @_ZN2v835SetReleaseCppHeapCallbackForTestingEPFvSt10unique_ptrINS_7CppHeapEEE(!cir.ptr<!cir.func<([[REC]])>>)
// CHECK: cir.func no_inline {{.*}}@_Z6callitPFvSt10unique_ptrIN2v87CppHeapEEE(%arg0: !cir.ptr<!cir.func<([[REC]])>>
// CHECK: cir.call @_ZN2v835SetReleaseCppHeapCallbackForTestingEPFvSt10unique_ptrINS_7CppHeapEEE
