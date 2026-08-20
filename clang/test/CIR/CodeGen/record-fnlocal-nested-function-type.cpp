// A class-template method's mangling can embed an unnamed type below a
// function- or member-pointer template argument. The function-local record
// identity must anchor on the method USR, not on a request-order-sensitive
// mangler spelling hidden below that pointer.
// RUN: printf '_Z7perturbv\n_Z3runv\n' > %t.first.roots
// RUN: printf '_Z3runv\n_Z7perturbv\n' > %t.second.roots
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir -fclangir-emit-selected-decls=%t.first.roots -skip-function-bodies %s -o %t.first.cir
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -std=c++20 -fclangir -fclangir-aeneas-metadata -emit-cir -fclangir-emit-selected-decls=%t.second.roots -skip-function-bodies %s -o %t.second.cir
// RUN: %python -c "import hashlib,re,sys; texts=[open(p).read() for p in sys.argv[1:]]; ids=[set(re.findall(r'\"([^\"]*@F@check[^\"]*#fnlocal:[0-9a-f]+)\"',t)) for t in texts]; usrs=[set(re.findall(r'cir\.func[^\n]*ast_decl_usr = \"([^\"]*@F@check#1)\"',t)) for t in texts]; assert len(ids[0])==1 and ids[0]==ids[1] and len(usrs[0])==1 and usrs[0]==usrs[1],(ids,usrs); identity=next(iter(ids[0])); actual=identity.rsplit('#fnlocal:',1)[1]; owner=next(iter(usrs[0])); expected=hashlib.sha256(('record-function-local-owner-v2'+'usr:'+owner+':0').encode()).hexdigest(); assert actual==expected,(actual,expected,owner)" %t.first.cir %t.second.cir

template <class FunctionPointer, class MemberPointer> struct Owner {
  bool check() const { return [] { return true; }(); }
};

struct Receiver {
  void method(int);
};

void perturb() {
  auto closure = [] {};
  (void)closure;
}

bool run() {
  struct {
    int marker;
  } local;
  using FunctionPointer = void (*)(decltype(local));
  using MemberPointer = void (Receiver::*)(decltype(local));
  return Owner<FunctionPointer, MemberPointer>{}.check();
}
