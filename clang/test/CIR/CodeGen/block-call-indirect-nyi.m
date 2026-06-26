// RUN: %clang_cc1 -triple arm64-apple-macosx -fclangir -emit-cir -fblocks -verify %s

void block_parameter_call(void (^block)(int)) {
  // expected-error@+1 {{ClangIR code gen Not Yet Implemented: block indirect invoke lowering}}
  block(42);
}
