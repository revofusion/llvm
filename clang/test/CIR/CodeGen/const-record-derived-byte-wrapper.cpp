// RUN: %clang_cc1 -triple arm64-apple-macosx15.0.0 -std=c++20 -fclangir -emit-cir %s -o - | FileCheck %s

template <typename SubType, int AfterLast>
class RegisterBase {
public:
  static constexpr int CodeNoReg = -1;

protected:
  explicit constexpr RegisterBase(int code) : code_(code) {}

private:
  signed char code_;
};

enum RegisterCode { RegAfterLast = 32 };

class CPURegister : public RegisterBase<CPURegister, RegAfterLast> {
public:
  enum RegisterType : signed char {
    RegisterTag,
    VectorTag,
    ScalableTag,
    NoRegisterTag
  };

  static constexpr CPURegister noReg() {
    return CPURegister{CodeNoReg, 0, NoRegisterTag};
  }

  static constexpr CPURegister create(int code, int size, RegisterType type) {
    return CPURegister{code, size, type};
  }

protected:
  unsigned char size_;
  RegisterType type_;

  constexpr CPURegister(int code, int size, RegisterType type)
      : RegisterBase(code), size_(size), type_(type) {}
};

class Register : public CPURegister {
public:
  static constexpr Register noReg() { return Register(CPURegister::noReg()); }
  static constexpr Register create(int code, int size) {
    return Register(CPURegister::create(code, size, RegisterTag));
  }

private:
  constexpr explicit Register(const CPURegister &reg) : CPURegister(reg) {}
};

class VRegister : public CPURegister {
public:
  static constexpr VRegister noReg() {
    return VRegister(CPURegister::noReg(), 0);
  }
  static constexpr VRegister create(int code, int size, int lanes = 1) {
    return VRegister(CPURegister::create(code, size, VectorTag), lanes);
  }

private:
  constexpr explicit VRegister(const CPURegister &reg, int lanes)
      : CPURegister(reg), lanes_(lanes) {}

  signed char lanes_;
};

Register no_reg = Register::noReg();
Register x0_reg = Register::create(0, 64);
VRegister d0_reg = VRegister::create(0, 64);

// CHECK: !rec_RegisterBase
// CHECK: !rec_CPURegister = !cir.record<class "CPURegister" {!rec_RegisterBase
// CHECK: !rec_Register = !cir.record<class "Register" {!rec_CPURegister}>
// CHECK: !rec_VRegister = !cir.record<class "VRegister" {!rec_CPURegister, !s8i}>
// CHECK: cir.global external @no_reg = #cir.const_record<{#cir.const_record<{#cir.const_record<{#cir.int<-1> : !s8i}> : !rec_RegisterBase{{.*}}, #cir.int<0> : !u8i, #cir.int<3> : !s8i}> : !rec_CPURegister}> : !rec_Register
// CHECK: cir.global external @x0_reg = #cir.const_record<{#cir.const_record<{#cir.zero : !rec_RegisterBase{{.*}}, #cir.int<64> : !u8i, #cir.int<0> : !s8i}> : !rec_CPURegister}> : !rec_Register
// CHECK: cir.global external @d0_reg = #cir.const_record<{#cir.const_record<{#cir.zero : !rec_RegisterBase{{.*}}, #cir.int<64> : !u8i, #cir.int<1> : !s8i}> : !rec_CPURegister, #cir.int<1> : !s8i}> : !rec_VRegister
