// RUN: %clang_cc1 -triple arm64-apple-macos -fclangir -emit-cir -o - %s | FileCheck %s

typedef unsigned char uint8_t;
typedef long long int64_t;
typedef uint8_t uint8x8_t __attribute__((neon_vector_type(8)));
typedef uint8_t uint8x16_t __attribute__((neon_vector_type(16)));
typedef int64_t int64x1_t __attribute__((neon_vector_type(1)));

uint8x8_t load8(const uint8_t *p) {
  return __builtin_neon_vld1_v(p, 16);
}

// CHECK-LABEL: cir.func{{.*}} @load8
// CHECK: cir.load align(1) {{.*}} : !cir.ptr<!cir.vector<8 x !s8i>>, !cir.vector<8 x !s8i>

uint8x16_t load16(const uint8_t *p) {
  return __builtin_neon_vld1q_v(p, 48);
}

// CHECK-LABEL: cir.func{{.*}} @load16
// CHECK: cir.load align(1) {{.*}} : !cir.ptr<!cir.vector<16 x !s8i>>, !cir.vector<16 x !s8i>

void store16(uint8_t *p, uint8x16_t v) {
  __builtin_neon_vst1q_v(p, v, 48);
}

// CHECK-LABEL: cir.func{{.*}} @store16
// CHECK: cir.store align(1) {{.*}} : !cir.vector<16 x !s8i>, !cir.ptr<!cir.vector<16 x !s8i>>

int64_t lane64(int64x1_t v) {
  return __builtin_neon_vget_lane_i64(v, 0);
}

// CHECK-LABEL: cir.func{{.*}} @lane64
// CHECK: cir.vec.extract {{.*}} : !cir.vector<1 x !s64i>
