// RUN: %clang_cc1 -std=c++17 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s --check-prefix=CIR

namespace std {
template <class T> class initializer_list {
  const T *begin_;
  __SIZE_TYPE__ size_;

public:
  constexpr initializer_list() : begin_(nullptr), size_(0) {}
  constexpr const T *begin() const { return begin_; }
  constexpr const T *end() const { return begin_ + size_; }
};
} // namespace std

struct Element {
  int value;
  explicit Element(const char *);
  ~Element();
};

Element make_element(int);
void consume(std::initializer_list<Element>);

void local_array_element_identity() {
  Element elements[] = {make_element(1), Element("middle"), make_element(3)};
}

void temporary_array_element_identity() {
  consume({make_element(1), Element("middle"), make_element(3)});
}

// CIR-LABEL: cir.func{{.*}} @_Z28local_array_element_identityv()
// CIR: cir.cast array_to_ptrdecay {{.*}} {ast_array_element_init = {extent = 3 : i64, index = 0 : i64}
// CIR: cir.ptr_stride {{.*}} {ast_array_element_init = {extent = 3 : i64, index = 1 : i64}
// CIR: cir.ptr_stride {{.*}} {ast_array_element_init = {extent = 3 : i64, index = 2 : i64}

// CIR-LABEL: cir.func{{.*}} @_Z32temporary_array_element_identityv()
// CIR: cir.cast array_to_ptrdecay {{.*}} {ast_array_element_init = {extent = 3 : i64, index = 0 : i64}
// CIR: cir.ptr_stride {{.*}} {ast_array_element_init = {extent = 3 : i64, index = 1 : i64}
// CIR: cir.ptr_stride {{.*}} {ast_array_element_init = {extent = 3 : i64, index = 2 : i64}
