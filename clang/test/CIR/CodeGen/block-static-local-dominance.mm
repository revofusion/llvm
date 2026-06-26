// RUN: %clang_cc1 -triple arm64-apple-macosx -x objective-c++ -std=c++23 -fclangir -emit-cir -fblocks %s -o - | FileCheck %s

void takes_pointer(void *);
void log_string(const char *);
int make_int(void *);

void static_local_after_block_scope(void) {
  static int x;
  takes_pointer((void *)^{
    if (x != 0)
      log_string(__FUNCTION__);
    x = make_int((void *)0);
  });
}

// CHECK: cir.func internal private @__cir_block_invoke
// CHECK: cir.scope
// CHECK: cir.get_global @_ZZ30static_local_after_block_scopevE1x
// CHECK: cir.call @_Z8make_intPv
// CHECK: cir.get_global @_ZZ30static_local_after_block_scopevE1x
// CHECK: cir.store
