// RUN: %clang_cc1 -triple arm64-apple-macosx -fclangir -emit-cir -fblocks -verify %s

void takes_pointer(void *);

void block_argument_call(void) {
  // expected-error@+1 {{ClangIR code gen Not Yet Implemented: block literal runtime lowering}}
  takes_pointer((void *)^{});
}
