// RUN: %clang_cc1 -std=c++20 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s

// A constexpr reference to a constant-initialized object, used in a context
// that does not odr-use it, is a NOUR_Constant DeclRefExpr of reference type.
// It must materialize the constant referent's address rather than try to load
// the (non-emitted) reference variable (which previously hit the
// "emitDeclRefLValue: NonOdrUse reference constant" NYI).

static constexpr int kValue = 42;
static constexpr const int &kRef = kValue;

int read_through_constexpr_ref() {
  // kRef is a NOUR_Constant reference here; we read through it.
  return kRef;
}

// CHECK: cir.func{{.*}} @_Z26read_through_constexpr_refv
// The reference value (the address of the referent constant) is materialized
// as a constant pointer and loaded through, rather than loading a runtime
// reference variable.
// CHECK:   %[[PTR:.*]] = cir.const #cir.global_view
// CHECK:   %[[VAL:.*]] = cir.load{{.*}} %[[PTR]]
// CHECK:   cir.return
