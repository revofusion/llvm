// RUN: %clang_cc1 -triple x86_64-apple-darwin11 -fclangir -emit-cir -fobjc-arc -o %t.cir %s
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR

// Keep this equivalent to the RangeCheck shape in Chromium's
// base/numerics/safe_conversions_impl.h. In particular, the complete
// constructor delegates to the base constructor while ARC is enabled.
class RangeCheck {
public:
  constexpr RangeCheck() = default;
  constexpr RangeCheck(bool is_in_lower_bound, bool is_in_upper_bound)
      : is_underflow_(!is_in_lower_bound), is_overflow_(!is_in_upper_bound) {}

private:
  const bool is_underflow_ = false;
  const bool is_overflow_ = false;
};

RangeCheck makeRangeCheck(bool lower, bool upper) {
  return RangeCheck(lower, upper);
}

// CIR-LABEL: cir.func{{.*}} @_ZN10RangeCheckC1Ebb(
// CIR: %[[LOWER_ADDR:.*]] = cir.alloca "is_in_lower_bound"
// CIR: %[[UPPER_ADDR:.*]] = cir.alloca "is_in_upper_bound"
// CIR: cir.store %{{.*}}, %[[LOWER_ADDR]]
// CIR: cir.store %{{.*}}, %[[UPPER_ADDR]]
// CIR: %[[LOWER:.*]] = cir.load{{.*}} %[[LOWER_ADDR]]
// CIR: %[[UPPER:.*]] = cir.load{{.*}} %[[UPPER_ADDR]]
// CIR: cir.call{{.*}} @_ZN10RangeCheckC2Ebb{{.*}}(%{{.*}}, %[[LOWER]], %[[UPPER]])

// NSObject-qualified pointers are ARC-retainable without requiring CIR's
// Objective-C object-pointer type lowering. The consumed argument must be
// moved out before forwarding so the parameter cleanup cannot over-release it.
typedef void * __attribute__((NSObject)) ObjCRef;

struct ForwardConsumed {
  ForwardConsumed(__attribute__((ns_consumed)) ObjCRef x);
};

ForwardConsumed::ForwardConsumed(__attribute__((ns_consumed)) ObjCRef x) {}

// CIR-LABEL: cir.func{{.*}} @_ZN15ForwardConsumedC1EPv(
// CIR: %[[X_ADDR:.*]] = cir.alloca "x"{{.*}} : !cir.ptr<!cir.ptr<!void>>
// CIR: %[[X:.*]] = cir.load{{.*}} %[[X_ADDR]] : !cir.ptr<!cir.ptr<!void>>, !cir.ptr<!void>
// CIR-NEXT: %[[NULL:.*]] = cir.const #cir.ptr<null> : !cir.ptr<!void>
// CIR-NEXT: cir.store{{.*}} %[[NULL]], %[[X_ADDR]] : !cir.ptr<!void>, !cir.ptr<!cir.ptr<!void>>
// CIR: cir.call{{.*}} @_ZN15ForwardConsumedC2EPv{{.*}}(%{{.*}}, %[[X]])
