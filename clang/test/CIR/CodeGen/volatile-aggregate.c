// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -Wno-unused-value -fclangir -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s -check-prefix=LLVM

#define FD_VOLATILE_CONST(x) (*((volatile const __typeof__((x)) *)&(x)))

struct xdp_desc {
  unsigned long addr;
  unsigned int len;
  unsigned int options;
};

struct xdp_desc packet_ring[2];

void volatile_struct_init(unsigned int index) {
  struct xdp_desc frame = FD_VOLATILE_CONST(packet_ring[index]);
}

// CIR-LABEL: cir.func{{.*}} @volatile_struct_init(
// CIR:         %[[FRAME:.*]] = cir.alloca "frame" {{.*}} : !cir.ptr<!rec_xdp_desc>
// CIR:         cir.copy {{.*}} to %[[FRAME]] volatile : !cir.ptr<!rec_xdp_desc>
// LLVM-LABEL: define{{.*}} void @volatile_struct_init(
// LLVM:         call void @llvm.memcpy.p0.p0.i64(ptr %{{.*}}, ptr %{{.*}}, i64 16, i1 true)

void discarded_volatile_struct_read(unsigned int index) {
  FD_VOLATILE_CONST(packet_ring[index]);
}

// CIR-LABEL: cir.func{{.*}} @discarded_volatile_struct_read(
// CIR:         %[[TMP:.*]] = cir.alloca "agg.tmp.ensured" {{.*}} : !cir.ptr<!rec_xdp_desc>
// CIR:         cir.copy {{.*}} to %[[TMP]] volatile : !cir.ptr<!rec_xdp_desc>
// LLVM-LABEL: define{{.*}} void @discarded_volatile_struct_read(
// LLVM:         %[[LLVM_TMP:.*]] = alloca %struct.xdp_desc
// LLVM:         call void @llvm.memcpy.p0.p0.i64(ptr %[[LLVM_TMP]], ptr %{{.*}}, i64 16, i1 true)

void nonvolatile_struct_init(unsigned int index) {
  struct xdp_desc frame = packet_ring[index];
}

// CIR-LABEL: cir.func{{.*}} @nonvolatile_struct_init(
// CIR:         %[[PLAIN_FRAME:.*]] = cir.alloca "frame" {{.*}} : !cir.ptr<!rec_xdp_desc>
// CIR:         cir.copy {{.*}} to %[[PLAIN_FRAME]] : !cir.ptr<!rec_xdp_desc>
// LLVM-LABEL: define{{.*}} void @nonvolatile_struct_init(
// LLVM:         call void @llvm.memcpy.p0.p0.i64(ptr %{{.*}}, ptr %{{.*}}, i64 16, i1 false)
