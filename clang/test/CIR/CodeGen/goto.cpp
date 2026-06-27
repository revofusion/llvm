// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-cir %s -o %t.cir
// RUN: FileCheck --input-file=%t.cir %s -check-prefix=CIR
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -fclangir -emit-llvm %s -o %t-cir.ll
// RUN: FileCheck --input-file=%t-cir.ll %s --check-prefix=LLVM
// RUN: %clang_cc1 -triple x86_64-unknown-linux-gnu -emit-llvm %s -o %t.ll
// RUN: FileCheck --input-file=%t.ll %s --check-prefix=OGCG

int shouldNotGenBranchRet(int x) {
  if (x > 5)
    goto err;
  return 0;
err:
  return -1;
}
// CIR:  cir.func {{.*}} @_Z21shouldNotGenBranchReti
// CIR:    %[[X:.*]] = cir.alloca !s32i, !cir.ptr<!s32i>, ["x", init]
// CIR:    %[[RETVAL:.*]] = cir.alloca !s32i, !cir.ptr<!s32i>, ["__retval"]
// CIR:    cir.store %arg{{.*}}, %[[X]] : !s32i, !cir.ptr<!s32i>
// CIR:    cir.scope {
// CIR:      %[[X_VAL:.*]] = cir.load{{.*}} %[[X]] : !cir.ptr<!s32i>, !s32i
// CIR:      %[[FIVE:.*]] = cir.const #cir.int<5> : !s32i
// CIR:      %[[COND:.*]] = cir.cmp(gt, %[[X_VAL]], %[[FIVE]]) : !s32i, !cir.bool
// CIR:      cir.if %[[COND]] {
// CIR:        cir.goto "err"
// CIR:      }
// CIR:    }
// CIR:    %[[ZERO:.*]] = cir.const #cir.int<0> : !s32i
// CIR:    cir.store %[[ZERO]], %[[RETVAL]] : !s32i, !cir.ptr<!s32i>
// CIR:    %[[RET:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s32i>, !s32i
// CIR:    cir.return %[[RET]] : !s32i
// CIR:  ^bb[[#]]:
// CIR:    cir.label "err"
// CIR:    %[[MINUS_ONE:.*]] = cir.const #cir.int<-1> : !s32i
// CIR:    cir.store %[[MINUS_ONE]], %[[RETVAL]] : !s32i, !cir.ptr<!s32i>
// CIR:    %[[RET2:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s32i>, !s32i
// CIR:    cir.return %[[RET2]] : !s32i

// LLVM: define dso_local i32 @_Z21shouldNotGenBranchReti
// LLVM:   [[COND:%.*]] = load i32, ptr {{.*}}, align 4
// LLVM:   [[CMP:%.*]] = icmp sgt i32 [[COND]], 5
// LLVM:   br i1 [[CMP]], label %[[IFTHEN:.*]], label %[[IFEND:.*]]
// LLVM: [[IFTHEN]]:
// LLVM:   br label %[[ERR:.*]]
// LLVM: [[IFEND]]:
// LLVM:   br label %[[BB:.*]]
// LLVM: [[BB]]:
// LLVM:   store i32 0, ptr %[[RETVAL:.*]], align 4
// LLVM:   [[RET0:%.*]] = load i32, ptr %[[RETVAL]], align 4
// LLVM:   ret i32 [[RET0]]
// LLVM: [[ERR]]:
// LLVM:   store i32 -1, ptr %[[RETVAL]], align 4
// LLVM:   [[RET1:%.*]] = load i32, ptr %[[RETVAL]], align 4
// LLVM:   ret i32 [[RET1]]

// OGCG: define dso_local noundef i32 @_Z21shouldNotGenBranchReti
// OGCG: if.then:
// OGCG:   br label %err
// OGCG: if.end:
// OGCG:   br label %return
// OGCG: err:
// OGCG:   br label %return
// OGCG: return:

int shouldGenBranch(int x) {
  if (x > 5)
    goto err;
  x++;
err:
  return -1;
}
// CIR:  cir.func {{.*}} @_Z15shouldGenBranchi
// CIR:    %[[X:.*]] = cir.alloca !s32i, !cir.ptr<!s32i>, ["x", init]
// CIR:    %[[RETVAL:.*]] = cir.alloca !s32i, !cir.ptr<!s32i>, ["__retval"]
// CIR:    cir.store %arg{{.*}}, %[[X]] : !s32i, !cir.ptr<!s32i>
// CIR:    cir.scope {
// CIR:      %[[X_VAL:.*]] = cir.load{{.*}} %[[X]] : !cir.ptr<!s32i>, !s32i
// CIR:      %[[FIVE:.*]] = cir.const #cir.int<5> : !s32i
// CIR:      %[[COND:.*]] = cir.cmp(gt, %[[X_VAL]], %[[FIVE]]) : !s32i, !cir.bool
// CIR:      cir.if %[[COND]] {
// CIR:        cir.goto "err"
// CIR:      }
// CIR:    }
// CIR:    %[[X2:.*]] = cir.load{{.*}} %[[X]] : !cir.ptr<!s32i>, !s32i
// CIR:    %[[INC:.*]] = cir.unary(inc, %[[X2]]) nsw : !s32i, !s32i
// CIR:    cir.store{{.*}} %[[INC]], %[[X]] : !s32i, !cir.ptr<!s32i>
// CIR:    cir.br ^bb[[#]]
// CIR:  ^bb[[#]]:
// CIR:    cir.label "err"
// CIR:    %[[MINUS_ONE:.*]] = cir.const #cir.int<-1> : !s32i
// CIR:    cir.store %[[MINUS_ONE]], %[[RETVAL]] : !s32i, !cir.ptr<!s32i>
// CIR:    %[[RET:.*]] = cir.load %[[RETVAL]] : !cir.ptr<!s32i>, !s32i
// CIR:    cir.return %[[RET]] : !s32i

// LLVM: define dso_local i32 @_Z15shouldGenBranchi
// LLVM:   br i1 [[CMP:%.*]], label %[[IFTHEN:.*]], label %[[IFEND:.*]]
// LLVM: [[IFTHEN]]:
// LLVM:   br label %[[ERR:.*]]
// LLVM: [[IFEND]]:
// LLVM:   br label %[[BB9:.*]]
// LLVM: [[BB9]]:
// LLVM:   br label %[[ERR]]
// LLVM: [[ERR]]:
// LLVM:   ret i32 [[RET:%.*]]

// OGCG: define dso_local noundef i32 @_Z15shouldGenBranchi
// OGCG: if.then:
// OGCG:   br label %err
// OGCG: if.end:
// OGCG:   br label %err
// OGCG: err:
// OGCG:   ret

void severalLabelsInARow(int a) {
  int b = a;
  goto end1;
  b = b + 1;
  goto end2;
end1:
end2:
  b = b + 2;
}
// CIR:  cir.func {{.*}} @_Z19severalLabelsInARowi
// CIR:    %[[A:.*]] = cir.alloca !s32i, !cir.ptr<!s32i>, ["a", init]
// CIR:    %[[B:.*]] = cir.alloca !s32i, !cir.ptr<!s32i>, ["b", init]
// CIR:    cir.store %arg{{.*}}, %[[A]] : !s32i, !cir.ptr<!s32i>
// CIR:    %[[A_VAL:.*]] = cir.load{{.*}} %[[A]] : !cir.ptr<!s32i>, !s32i
// CIR:    cir.store{{.*}} %[[A_VAL]], %[[B]] : !s32i, !cir.ptr<!s32i>
// CIR:    cir.goto "end1"
// CIR:  ^bb[[#]]:
// CIR:    %[[B_VAL:.*]] = cir.load{{.*}} %[[B]] : !cir.ptr<!s32i>, !s32i
// CIR:    %[[ONE:.*]] = cir.const #cir.int<1> : !s32i
// CIR:    %[[ADD:.*]] = cir.binop(add, %[[B_VAL]], %[[ONE]]) nsw : !s32i
// CIR:    cir.store{{.*}} %[[ADD]], %[[B]] : !s32i, !cir.ptr<!s32i>
// CIR:    cir.goto "end2"
// CIR:  ^bb[[#]]:
// CIR:    cir.label "end1"
// CIR:    cir.br ^bb[[#]]
// CIR:  ^bb[[#]]:
// CIR:    cir.label "end2"
// CIR:    %[[B_VAL2:.*]] = cir.load{{.*}} %[[B]] : !cir.ptr<!s32i>, !s32i
// CIR:    %[[TWO:.*]] = cir.const #cir.int<2> : !s32i
// CIR:    %[[ADD2:.*]] = cir.binop(add, %[[B_VAL2]], %[[TWO]]) nsw : !s32i
// CIR:    cir.store{{.*}} %[[ADD2]], %[[B]] : !s32i, !cir.ptr<!s32i>
// CIR:    cir.return

// LLVM: define dso_local void @_Z19severalLabelsInARowi
// LLVM:   br label %[[END1:.*]]
// LLVM: [[UNRE:.*]]:                                                ; No predecessors!
// LLVM:   br label %[[END2:.*]]
// LLVM: [[END1]]:
// LLVM:   br label %[[END2]]
// LLVM: [[END2]]:
// LLVM:   ret

// OGCG: define dso_local void @_Z19severalLabelsInARowi
// OGCG:   br label %end1
// OGCG: end1:
// OGCG:   br label %end2
// OGCG: end2:
// OGCG:   ret

void severalGotosInARow(int a) {
  int b = a;
  goto end;
  goto end;
end:
  b = b + 2;
}
// CIR:  cir.func {{.*}} @_Z18severalGotosInARowi
// CIR:    %[[A:.*]] = cir.alloca !s32i, !cir.ptr<!s32i>, ["a", init]
// CIR:    %[[B:.*]] = cir.alloca !s32i, !cir.ptr<!s32i>, ["b", init]
// CIR:    cir.store %arg{{.*}}, %[[A]] : !s32i, !cir.ptr<!s32i>
// CIR:    %[[A_VAL:.*]] = cir.load{{.*}} %[[A]] : !cir.ptr<!s32i>, !s32i
// CIR:    cir.store{{.*}} %[[A_VAL]], %[[B]] : !s32i, !cir.ptr<!s32i>
// CIR:    cir.goto "end"
// CIR:  ^bb[[#]]:
// CIR:    cir.goto "end"
// CIR:  ^bb[[#]]:
// CIR:    cir.label "end"
// CIR:    %[[B_VAL:.*]] = cir.load{{.*}} %[[B]] : !cir.ptr<!s32i>, !s32i
// CIR:    %[[TWO:.*]] = cir.const #cir.int<2> : !s32i
// CIR:    %[[ADD:.*]] = cir.binop(add, %[[B_VAL]], %[[TWO]]) nsw : !s32i
// CIR:    cir.store{{.*}} %[[ADD]], %[[B]] : !s32i, !cir.ptr<!s32i>
// CIR:    cir.return

// LLVM: define dso_local void @_Z18severalGotosInARowi
// LLVM:   br label %[[END:.*]]
// LLVM: [[UNRE:.*]]:                                                ; No predecessors!
// LLVM:   br label %[[END]]
// LLVM: [[END]]:
// LLVM:   ret void

// OGCG: define dso_local void @_Z18severalGotosInARowi(i32 noundef %a) #0 {
// OGCG:   br label %end
// OGCG: end:
// OGCG:   ret void

extern "C" void action1();
extern "C" void action2();
extern "C" void multiple_non_case(int v) {
  switch (v) {
    default:
        action1();
      l2:
        action2();
        break;
  }
}

// CIR: cir.func {{.*}} @multiple_non_case
// CIR: cir.switch
// CIR: cir.case(default, []) {
// CIR: cir.call @action1()
// CIR: cir.br ^bb[[#]]
// CIR: ^bb[[#]]:
// CIR: cir.label "l2"
// CIR: cir.call @action2()
// CIR: cir.break

// LLVM: define dso_local void @multiple_non_case
// LLVM: [[SWDEFAULT:.*]]:
// LLVM:   call void @action1()
// LLVM:   br label %[[L2:.*]]
// LLVM: [[L2]]:
// LLVM:   call void @action2()
// LLVM:   br label %[[BREAK:.*]]

// OGCG: define dso_local void @multiple_non_case
// OGCG: sw.default:
// OGCG:   call void @action1()
// OGCG:   br label %l2
// OGCG: l2:
// OGCG:   call void @action2()
// OGCG:   br label [[BREAK:%.*]]

extern "C" void case_follow_label(int v) {
  switch (v) {
    case 1:
    label:
    case 2:
      action1();
      break;
    default:
      action2();
      goto label;
  }
}

// CIR: cir.func {{.*}} @case_follow_label
// CIR: cir.switch
// CIR: cir.case(equal, [#cir.int<1> : !s32i]) {
// CIR:   cir.br ^bb[[#]]
// CIR: ^bb[[#]]:
// CIR:   cir.label "label"
// CIR: cir.case(equal, [#cir.int<2> : !s32i]) {
// CIR:   cir.call @action1()
// CIR:   cir.yield
// CIR: cir.case(default, []) {
// CIR:   cir.call @action2()
// CIR:   cir.goto "label"

// LLVM: define dso_local void @case_follow_label
// LLVM:  switch i32 {{.*}}, label %[[SWDEFAULT:.*]] [
// LLVM:    i32 1, label %[[CASE1:.*]]
// LLVM:    i32 2, label %[[CASE2:.*]]
// LLVM:  ]
// LLVM: [[CASE1]]:
// LLVM:   br label %[[LABEL:.*]]
// LLVM: [[LABEL]]:
// LLVM:   br label %[[CASE2]]
// LLVM: [[CASE2]]:
// LLVM:   call void @action1()
// LLVM:   br label %[[BREAK:.*]]
// LLVM: [[BREAK]]:
// LLVM:   br label %[[END:.*]]
// LLVM: [[SWDEFAULT]]:
// LLVM:   call void @action2()
// LLVM:   br label %[[LABEL]]
// LLVM: [[END]]:
// LLVM:   br label %[[RET:.*]]
// LLVM: [[RET]]:
// LLVM:   ret void

// OGCG: define dso_local void @case_follow_label
// OGCG: sw.bb:
// OGCG:   br label %label
// OGCG: label:
// OGCG:   br label %sw.bb1
// OGCG: sw.bb1:
// OGCG:   call void @action1()
// OGCG:   br label %sw.epilog
// OGCG: sw.default:
// OGCG:   call void @action2()
// OGCG:   br label %label
// OGCG: sw.epilog:
// OGCG:   ret void

extern "C" void default_follow_label(int v) {
  switch (v) {
    case 1:
    case 2:
      action1();
      break;
    label:
    default:
      action2();
      goto label;
  }
}

// CIR: cir.func {{.*}} @default_follow_label
// CIR: cir.switch
// CIR: cir.case(equal, [#cir.int<1> : !s32i]) {
// CIR:   cir.yield
// CIR: cir.case(equal, [#cir.int<2> : !s32i]) {
// CIR:   cir.call @action1()
// CIR:   cir.break
// CIR: ^bb[[#]]:
// CIR:   cir.label "label"
// CIR: cir.case(default, []) {
// CIR:   cir.call @action2()
// CIR:   cir.goto "label"

// LLVM: define dso_local void @default_follow_label
// LLVM: [[CASE1:.*]]:
// LLVM:   br label %[[BB8:.*]]
// LLVM: [[BB8]]:
// LLVM:   br label %[[CASE2:.*]]
// LLVM: [[CASE2]]:
// LLVM:   call void @action1()
// LLVM:   br label %[[BREAK:.*]]
// LLVM: [[LABEL:.*]]:
// LLVM:   br label %[[SWDEFAULT:.*]]
// LLVM: [[SWDEFAULT]]:
// LLVM:   call void @action2()
// LLVM:   br label %[[BB9:.*]]
// LLVM: [[BB9]]:
// LLVM:   br label %[[LABEL]]
// LLVM: [[BREAK]]:
// LLVM:   br label %[[RET:.*]]
// LLVM: [[RET]]:
// LLVM:   ret void

// OGCG: define dso_local void @default_follow_label
// OGCG: sw.bb:
// OGCG:   call void @action1()
// OGCG:   br label %sw.epilog
// OGCG: label:
// OGCG:   br label %sw.default
// OGCG: sw.default:
// OGCG:   call void @action2()
// OGCG:   br label %label
// OGCG: sw.epilog:
// OGCG:   ret void

void g3() {
label:
  goto label;
}

// CIR:  cir.func {{.*}} @_Z2g3v
// CIR:    cir.br ^bb[[#]]
// CIR:  ^bb[[#]]:
// CIR:    cir.label "label"
// CIR:    cir.goto "label"

// LLVM: define dso_local void @_Z2g3v()
// LLVM:   br label %1
// LLVM: 1:
// LLVM:   br label %1

// OGCG: define dso_local void @_Z2g3v()
// OGCG:   br label %label
// OGCG: label:
// OGCG:   br label %label
