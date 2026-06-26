//===--- CIRGenCall.cpp - Encapsulate calling convention details ----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// These classes wrap the information about a call or function definition used
// to handle ABI compliancy.
//
//===----------------------------------------------------------------------===//

#include "CIRGenCall.h"
#include "CIRGenCXXABI.h"
#include "CIRGenFunction.h"
#include "CIRGenFunctionInfo.h"
#include "clang/CIR/MissingFeatures.h"
#include "llvm/ADT/STLExtras.h"

using namespace clang;
using namespace clang::CIRGen;

CIRGenFunctionInfo *
CIRGenFunctionInfo::create(CanQualType resultType,
                           llvm::ArrayRef<CanQualType> argTypes,
                           RequiredArgs required) {
  // The first slot allocated for arg type slot is for the return value.
  void *buffer = operator new(
      totalSizeToAlloc<CanQualType>(argTypes.size() + 1));

  assert(!cir::MissingFeatures::opCallCIRGenFuncInfoParamInfo());

  CIRGenFunctionInfo *fi = new (buffer) CIRGenFunctionInfo();

  fi->required = required;
  fi->numArgs = argTypes.size();

  fi->getArgTypes()[0] = resultType;
  std::copy(argTypes.begin(), argTypes.end(), fi->argTypesBegin());
  assert(!cir::MissingFeatures::opCallCIRGenFuncInfoExtParamInfo());

  return fi;
}

cir::FuncType CIRGenTypes::getFunctionType(GlobalDecl gd) {
  const CIRGenFunctionInfo &fi = arrangeGlobalDeclaration(gd);
  return getFunctionType(fi);
}

cir::FuncType CIRGenTypes::getFunctionType(const CIRGenFunctionInfo &info) {
  mlir::Type resultType = convertType(info.getReturnType());
  SmallVector<mlir::Type, 8> argTypes;
  argTypes.reserve(info.getNumRequiredArgs());

  for (const CanQualType &argType : info.requiredArguments())
    argTypes.push_back(convertType(argType));

  return cir::FuncType::get(argTypes,
                            (resultType ? resultType : builder.getVoidTy()),
                            info.isVariadic());
}

cir::FuncType CIRGenTypes::getFunctionTypeForVTable(GlobalDecl gd) {
  const CXXMethodDecl *md = cast<CXXMethodDecl>(gd.getDecl());
  const FunctionProtoType *fpt = md->getType()->getAs<FunctionProtoType>();

  if (!isFuncTypeConvertible(fpt))
    cgm.errorNYI("getFunctionTypeForVTable: non-convertible function type");

  return getFunctionType(gd);
}

CIRGenCallee CIRGenCallee::prepareConcreteCallee(CIRGenFunction &cgf) const {
  if (isVirtual()) {
    const CallExpr *ce = getVirtualCallExpr();
    return cgf.cgm.getCXXABI().getVirtualFunctionPointer(
        cgf, getVirtualMethodDecl(), getThisAddress(), getVirtualFunctionType(),
        ce ? ce->getBeginLoc() : SourceLocation());
  }
  return *this;
}

void CIRGenFunction::emitAggregateStore(mlir::Value value, Address dest) {
  // In classic codegen:
  // Function to store a first-class aggregate into memory. We prefer to
  // store the elements rather than the aggregate to be more friendly to
  // fast-isel.
  // In CIR codegen:
  // Emit the most simple cir.store possible (e.g. a store for a whole
  // record), which can later be broken down in other CIR levels (or prior
  // to dialect codegen).

  mlir::Location loc = currSrcLoc ? *currSrcLoc : builder.getUnknownLoc();
  if (!value) {
    cgm.errorNYI(loc, "emitAggregateStore: value unavailable");
    return;
  }
  if (!dest.isValid()) {
    cgm.errorNYI(loc, "emitAggregateStore: destination unavailable");
    return;
  }
  if (value.getType() != dest.getElementType()) {
    cgm.errorNYI(
        loc,
        "emitAggregateStore: value type does not match destination type");
    return;
  }

  // Stored result for the callers of this function expected to be in the same
  // scope as the value, don't make assumptions about current insertion point.
  mlir::OpBuilder::InsertionGuard guard(builder);
  builder.setInsertionPointAfter(value.getDefiningOp());
  builder.createStore(loc, value, dest);
}

static void addAttributesFromFunctionProtoType(CIRGenBuilderTy &builder,
                                               mlir::NamedAttrList &attrs,
                                               const FunctionProtoType *fpt) {
  if (!fpt)
    return;

  if (!isUnresolvedExceptionSpec(fpt->getExceptionSpecType()) &&
      fpt->isNothrow())
    attrs.set(cir::CIRDialect::getNoThrowAttrName(),
              mlir::UnitAttr::get(builder.getContext()));
}

/// Construct the CIR attribute list of a function or call.
void CIRGenModule::constructAttributeList(CIRGenCalleeInfo calleeInfo,
                                          mlir::NamedAttrList &attrs) {
  assert(!cir::MissingFeatures::opCallCallConv());
  auto sideEffect = cir::SideEffect::All;

  addAttributesFromFunctionProtoType(getBuilder(), attrs,
                                     calleeInfo.getCalleeFunctionProtoType());

  const Decl *targetDecl = calleeInfo.getCalleeDecl().getDecl();

  if (targetDecl) {
    if (targetDecl->hasAttr<NoThrowAttr>())
      attrs.set(cir::CIRDialect::getNoThrowAttrName(),
                mlir::UnitAttr::get(&getMLIRContext()));

    if (const FunctionDecl *func = dyn_cast<FunctionDecl>(targetDecl)) {
      addAttributesFromFunctionProtoType(
          getBuilder(), attrs, func->getType()->getAs<FunctionProtoType>());
      assert(!cir::MissingFeatures::opCallAttrs());
    }

    assert(!cir::MissingFeatures::opCallAttrs());

    // 'const', 'pure' and 'noalias' attributed functions are also nounwind.
    if (targetDecl->hasAttr<ConstAttr>()) {
      // gcc specifies that 'const' functions have greater restrictions than
      // 'pure' functions, so they also cannot have infinite loops.
      sideEffect = cir::SideEffect::Const;
    } else if (targetDecl->hasAttr<PureAttr>()) {
      // gcc specifies that 'pure' functions cannot have infinite loops.
      sideEffect = cir::SideEffect::Pure;
    }

    assert(!cir::MissingFeatures::opCallAttrs());
  }

  assert(!cir::MissingFeatures::opCallAttrs());

  attrs.set(cir::CIRDialect::getSideEffectAttrName(),
            cir::SideEffectAttr::get(&getMLIRContext(), sideEffect));
}

/// Returns the canonical formal type of the given C++ method.
static CanQual<FunctionProtoType> getFormalType(const CXXMethodDecl *md) {
  return md->getType()
      ->getCanonicalTypeUnqualified()
      .getAs<FunctionProtoType>();
}

static bool diagnoseUnsupportedExtParameterInfo(
    const CIRGenTypes &cgt, FunctionProtoType::ExtParameterInfo paramInfo) {
  if (paramInfo.hasPassObjectSize()) {
    cgt.getCGModule().errorNYI("extended parameter info: pass_object_size");
    return true;
  }
  if (paramInfo.getABI() != ParameterABI::Ordinary) {
    cgt.getCGModule().errorNYI("extended parameter info: parameter ABI");
    return true;
  }
  if (paramInfo.isConsumed()) {
    cgt.getCGModule().errorNYI("extended parameter info: consumed parameter");
    return true;
  }
  return false;
}

static bool diagnoseUnsupportedExtParameterInfos(const CIRGenTypes &cgt,
                                                 const FunctionProtoType *fpt) {
  if (!fpt->hasExtParameterInfos())
    return false;

  bool diagnosed = false;
  for (FunctionProtoType::ExtParameterInfo paramInfo :
       fpt->getExtParameterInfos())
    diagnosed |= diagnoseUnsupportedExtParameterInfo(cgt, paramInfo);
  return diagnosed;
}

/// Adds the formal parameters in FPT to the given prefix. If any parameter in
/// FPT has pass_object_size_attrs, then we'll add parameters for those, too.
/// TODO(cir): this should be shared with LLVM codegen
static void appendParameterTypes(const CIRGenTypes &cgt,
                                 SmallVectorImpl<CanQualType> &prefix,
                                 CanQual<FunctionProtoType> fpt) {
  assert(!cir::MissingFeatures::opCallExtParameterInfo());
  // Fast path: don't touch param info if we don't need to.
  if (!fpt->hasExtParameterInfos()) {
    prefix.append(fpt->param_type_begin(), fpt->param_type_end());
    return;
  }

  diagnoseUnsupportedExtParameterInfos(cgt, fpt.getTypePtr());

  auto extInfos = fpt->getExtParameterInfos();
  assert(extInfos.size() == fpt->getNumParams());
  for (unsigned i = 0, e = fpt->getNumParams(); i != e; ++i) {
    prefix.push_back(fpt->getParamType(i));
    if (extInfos[i].hasPassObjectSize())
      prefix.push_back(cgt.getASTContext().getCanonicalSizeType());
  }
}

const CIRGenFunctionInfo &
CIRGenTypes::arrangeCXXStructorDeclaration(GlobalDecl gd) {
  auto *md = cast<CXXMethodDecl>(gd.getDecl());

  llvm::SmallVector<CanQualType, 16> argTypes;
  argTypes.push_back(deriveThisType(md->getParent(), md));

  bool passParams = true;

  if (auto *cd = dyn_cast<CXXConstructorDecl>(md)) {
    // A base class inheriting constructor doesn't get forwarded arguments
    // needed to construct a virtual base (or base class thereof)
    if (auto inherited = cd->getInheritedConstructor())
      passParams = inheritingCtorHasParams(inherited, gd.getCtorType());
  }

  CanQual<FunctionProtoType> fpt = getFormalType(md);

  if (passParams)
    appendParameterTypes(*this, argTypes, fpt);

  // The structor signature may include implicit parameters.
  [[maybe_unused]] CIRGenCXXABI::AddedStructorArgCounts addedArgs =
      theCXXABI.buildStructorSignature(gd, argTypes);
  assert(!cir::MissingFeatures::opCallCIRGenFuncInfoExtParamInfo());

  RequiredArgs required =
      (passParams && md->isVariadic() ? RequiredArgs(argTypes.size())
                                      : RequiredArgs::All);

  CanQualType resultType = theCXXABI.hasThisReturn(gd) ? argTypes.front()
                           : theCXXABI.hasMostDerivedReturn(gd)
                               ? astContext.VoidPtrTy
                               : astContext.VoidTy;

  assert(!theCXXABI.hasThisReturn(gd) &&
         "Please send PR with a test and remove this");

  assert(!cir::MissingFeatures::opCallCIRGenFuncInfoExtParamInfo());
  assert(!cir::MissingFeatures::opCallFnInfoOpts());

  return arrangeCIRFunctionInfo(resultType, argTypes, required);
}

bool CIRGenTypes::inheritingCtorHasParams(
    const InheritedConstructor &inherited, CXXCtorType type) {
  // Parameters are unnecessary if we're constructing a base class subobject
  // and the inherited constructor lives in a virtual base.
  return type == Ctor_Complete ||
         !inherited.getShadowDecl()->constructsVirtualBase() ||
         !cgm.getTarget().getCXXABI().hasConstructorVariants();
}

/// Derives the 'this' type for CIRGen purposes, i.e. ignoring method CVR
/// qualification. Either or both of `rd` and `md` may be null. A null `rd`
/// indicates that there is no meaningful 'this' type, and a null `md` can occur
/// when calling a method pointer.
CanQualType CIRGenTypes::deriveThisType(const CXXRecordDecl *rd,
                                        const CXXMethodDecl *md) {
  CanQualType recTy;
  if (rd) {
    recTy = getASTContext().getCanonicalTagType(rd);
  } else {
    // This can happen with the MS ABI. It shouldn't need anything more than
    // setting recTy to VoidTy here, but we're flagging it for now because we
    // don't have the full handling implemented.
    cgm.errorNYI("deriveThisType: no record decl");
    recTy = getASTContext().VoidTy;
  }

  if (md)
    recTy = CanQualType::CreateUnsafe(getASTContext().getAddrSpaceQualType(
        recTy, md->getMethodQualifiers().getAddressSpace()));
  return getASTContext().getPointerType(recTy);
}

/// Arrange the CIR function layout for a value of the given function type, on
/// top of any implicit parameters already stored.
static const CIRGenFunctionInfo &
arrangeCIRFunctionInfo(CIRGenTypes &cgt, SmallVectorImpl<CanQualType> &prefix,
                       CanQual<FunctionProtoType> fpt) {
  assert(!cir::MissingFeatures::opCallFnInfoOpts());
  RequiredArgs required =
      RequiredArgs::getFromProtoWithExtraSlots(fpt, prefix.size());
  assert(!cir::MissingFeatures::opCallExtParameterInfo());
  appendParameterTypes(cgt, prefix, fpt);
  CanQualType resultType = fpt->getReturnType().getUnqualifiedType();
  return cgt.arrangeCIRFunctionInfo(resultType, prefix, required);
}

void CIRGenFunction::emitDelegateCallArg(CallArgList &args,
                                         const VarDecl *param,
                                         SourceLocation loc) {
  // StartFunction converted the ABI-lowered parameter(s) into a local alloca.
  // We need to turn that into an r-value suitable for emitCall
  Address local = getAddrOfLocalVar(param);

  QualType type = param->getType();

  // GetAddrOfLocalVar returns a pointer-to-pointer for references, but the
  // argument needs to be the original pointer.
  if (type->isReferenceType()) {
    args.add(
        RValue::get(builder.createLoad(getLoc(param->getSourceRange()), local)),
        type);
  } else if (getLangOpts().ObjCAutoRefCount) {
    cgm.errorNYI(param->getSourceRange(),
                 "emitDelegateCallArg: ObjCAutoRefCount");
    // For the most part, we just need to load the alloca, except that aggregate
    // r-values are actually pointers to temporaries.
  } else {
    args.add(convertTempToRValue(local, type, loc), type);
  }

  if (type->isRecordType() && !curFuncIsThunk &&
      type->castAsRecordDecl()->isParamDestroyedInCallee() &&
      param->needsDestruction(getContext())) {
    const auto *parm = dyn_cast<ParmVarDecl>(param);
    EHScopeStack::stable_iterator cleanup =
        parm ? calleeDestructedParamCleanups.lookup(parm)
             : EHScopeStack::stable_iterator::invalid();
    if (!cleanup.isValid()) {
      cgm.errorNYI(param->getSourceRange(),
                   "emitDelegateCallArg: callee-destructed param cleanup");
      return;
    }
    args.addArgCleanupDeactivation(cleanup, builder.saveInsertionPoint());
  }
}

static const CIRGenFunctionInfo &
arrangeFreeFunctionLikeCall(CIRGenTypes &cgt, CIRGenModule &cgm,
                            const CallArgList &args,
                            const FunctionType *fnType) {

  RequiredArgs required = RequiredArgs::All;

  if (const auto *proto = dyn_cast<FunctionProtoType>(fnType)) {
    if (proto->isVariadic())
      required = RequiredArgs::getFromProtoWithExtraSlots(proto, 0);
    diagnoseUnsupportedExtParameterInfos(cgt, proto);
  } else if (cgm.getTargetCIRGenInfo().isNoProtoCallVariadic(
                 cast<FunctionNoProtoType>(fnType)))
    cgm.errorNYI("call to function without a prototype");

  SmallVector<CanQualType, 16> argTypes;
  for (const CallArg &arg : args)
    argTypes.push_back(cgt.getASTContext().getCanonicalParamType(arg.ty));

  CanQualType retType = fnType->getReturnType()->getCanonicalTypeUnqualified();

  assert(!cir::MissingFeatures::opCallFnInfoOpts());
  return cgt.arrangeCIRFunctionInfo(retType, argTypes, required);
}

/// Arrange a call to a C++ method, passing the given arguments.
///
/// extraPrefixArgs is the number of ABI-specific args passed after the `this`
/// parameter.
/// passProtoArgs indicates whether `args` has args for the parameters in the
/// given CXXConstructorDecl.
const CIRGenFunctionInfo &CIRGenTypes::arrangeCXXConstructorCall(
    const CallArgList &args, const CXXConstructorDecl *d, CXXCtorType ctorKind,
    unsigned extraPrefixArgs, unsigned extraSuffixArgs, bool passProtoArgs) {

  // FIXME: Kill copy.
  llvm::SmallVector<CanQualType, 16> argTypes;
  for (const auto &arg : args)
    argTypes.push_back(astContext.getCanonicalParamType(arg.ty));

  // +1 for implicit this, which should always be args[0]
  unsigned totalPrefixArgs = 1 + extraPrefixArgs;

  CanQual<FunctionProtoType> fpt = getFormalType(d);
  RequiredArgs required = passProtoArgs
                              ? RequiredArgs::getFromProtoWithExtraSlots(
                                    fpt, totalPrefixArgs + extraSuffixArgs)
                              : RequiredArgs::All;

  GlobalDecl gd(d, ctorKind);
  if (theCXXABI.hasThisReturn(gd))
    cgm.errorNYI(d->getSourceRange(),
                 "arrangeCXXConstructorCall: hasThisReturn");
  if (theCXXABI.hasMostDerivedReturn(gd))
    cgm.errorNYI(d->getSourceRange(),
                 "arrangeCXXConstructorCall: hasMostDerivedReturn");
  CanQualType resultType = astContext.VoidTy;

  assert(!cir::MissingFeatures::opCallFnInfoOpts());
  assert(!cir::MissingFeatures::opCallCIRGenFuncInfoExtParamInfo());

  return arrangeCIRFunctionInfo(resultType, argTypes, required);
}

/// Arrange a call to a C++ method, passing the given arguments.
///
/// numPrefixArgs is the number of the ABI-specific prefix arguments we have. It
/// does not count `this`.
const CIRGenFunctionInfo &CIRGenTypes::arrangeCXXMethodCall(
    const CallArgList &args, const FunctionProtoType *proto,
    RequiredArgs required, unsigned numPrefixArgs) {
  assert(!cir::MissingFeatures::opCallExtParameterInfo());
  assert(numPrefixArgs + 1 <= args.size() &&
         "Emitting a call with less args than the required prefix?");

  // FIXME: Kill copy.
  llvm::SmallVector<CanQualType, 16> argTypes;
  for (const CallArg &arg : args)
    argTypes.push_back(astContext.getCanonicalParamType(arg.ty));

  assert(!cir::MissingFeatures::opCallFnInfoOpts());
  return arrangeCIRFunctionInfo(
      proto->getReturnType()->getCanonicalTypeUnqualified(), argTypes,
      required);
}

const CIRGenFunctionInfo &
CIRGenTypes::arrangeFreeFunctionCall(const CallArgList &args,
                                     const FunctionType *fnType) {
  return arrangeFreeFunctionLikeCall(*this, cgm, args, fnType);
}

const CIRGenFunctionInfo &
CIRGenTypes::arrangeBlockFunctionCall(const CallArgList &args,
                                      const FunctionType *fnType) {
  // Block ABI calls are normal calls whose first lowered argument is the block
  // object. The AST function type for a block pointer does not include that
  // implicit self parameter, so arrange from the already-built argument list.
  RequiredArgs required = RequiredArgs::All;
  if (const auto *proto = dyn_cast<FunctionProtoType>(fnType)) {
    if (proto->isVariadic())
      required = RequiredArgs::getFromProtoWithExtraSlots(proto, 1);
    diagnoseUnsupportedExtParameterInfos(*this, proto);
  } else if (cgm.getTargetCIRGenInfo().isNoProtoCallVariadic(
                 cast<FunctionNoProtoType>(fnType))) {
    cgm.errorNYI("block call to function without a prototype");
  }

  SmallVector<CanQualType, 16> argTypes;
  for (const CallArg &arg : args)
    argTypes.push_back(astContext.getCanonicalParamType(arg.ty));
  return arrangeCIRFunctionInfo(
      fnType->getReturnType()->getCanonicalTypeUnqualified(), argTypes,
      required);
}

/// Arrange the argument and result information for a declaration or definition
/// of the given C++ non-static member function. The member function must be an
/// ordinary function, i.e. not a constructor or destructor.
const CIRGenFunctionInfo &
CIRGenTypes::arrangeCXXMethodDeclaration(const CXXMethodDecl *md) {
  assert(!isa<CXXConstructorDecl>(md) && "wrong method for constructors!");
  assert(!isa<CXXDestructorDecl>(md) && "wrong method for destructors!");

  auto prototype =
      md->getType()->getCanonicalTypeUnqualified().getAs<FunctionProtoType>();
  assert(!cir::MissingFeatures::cudaSupport());

  if (md->isInstance()) {
    // The abstract case is perfectly fine.
    auto *thisType = theCXXABI.getThisArgumentTypeForMethod(md);
    return arrangeCXXMethodType(thisType, prototype.getTypePtr(), md);
  }

  return arrangeFreeFunctionType(prototype);
}

/// Arrange the argument and result information for a call to an unknown C++
/// non-static member function of the given abstract type. (A null RD means we
/// don't have any meaningful "this" argument type, so fall back to a generic
/// pointer type). The member fucntion must be an ordinary function, i.e. not a
/// constructor or destructor.
const CIRGenFunctionInfo &
CIRGenTypes::arrangeCXXMethodType(const CXXRecordDecl *rd,
                                  const FunctionProtoType *fpt,
                                  const CXXMethodDecl *md) {
  llvm::SmallVector<CanQualType, 16> argTypes;

  // Add the 'this' pointer.
  argTypes.push_back(deriveThisType(rd, md));

  assert(!cir::MissingFeatures::opCallFnInfoOpts());
  return ::arrangeCIRFunctionInfo(
      *this, argTypes,
      fpt->getCanonicalTypeUnqualified().getAs<FunctionProtoType>());
}

/// Arrange the argument and result information for the declaration or
/// definition of the given function.
const CIRGenFunctionInfo &
CIRGenTypes::arrangeFunctionDeclaration(const FunctionDecl *fd) {
  if (const auto *md = dyn_cast<CXXMethodDecl>(fd))
    if (md->isInstance())
      return arrangeCXXMethodDeclaration(md);

  CanQualType funcTy = fd->getType()->getCanonicalTypeUnqualified();

  assert(isa<FunctionType>(funcTy));
  // TODO: setCUDAKernelCallingConvention
  assert(!cir::MissingFeatures::cudaSupport());

  // When declaring a function without a prototype, always use a non-variadic
  // type.
  if (CanQual<FunctionNoProtoType> noProto =
          funcTy.getAs<FunctionNoProtoType>()) {
    assert(!cir::MissingFeatures::opCallCIRGenFuncInfoExtParamInfo());
    assert(!cir::MissingFeatures::opCallFnInfoOpts());
    return arrangeCIRFunctionInfo(noProto->getReturnType(), {},
                                  RequiredArgs::All);
  }

  return arrangeFreeFunctionType(funcTy.castAs<FunctionProtoType>());
}

static cir::CIRCallOpInterface
emitCallLikeOp(CIRGenFunction &cgf, mlir::Location callLoc,
               cir::FuncType indirectFuncTy, mlir::Value indirectFuncVal,
               cir::FuncOp directFuncOp,
               const SmallVectorImpl<mlir::Value> &cirCallArgs, bool isInvoke,
               const mlir::NamedAttrList &attrs) {
  CIRGenBuilderTy &builder = cgf.getBuilder();

  assert(!cir::MissingFeatures::opCallSurroundingTry());

  if (isInvoke) {
    // This call may throw and requires catch and/or cleanup handling.
    // If this call does not appear within the `try` region of an existing
    // TryOp, we must create a synthetic TryOp to contain the call. This
    // happens when a call that may throw appears within a cleanup
    // scope.

    // In OG, we build the landing pad for this scope. In CIR, we emit a
    // synthetic cir.try because this didn't come from code generating from a
    // try/catch in C++.
    assert(cgf.curLexScope && "expected scope");
    cir::TryOp tryOp = cgf.curLexScope->getClosestTryParent();
    if (!tryOp) {
      cgf.cgm.errorNYI(
          "emitCallLikeOp: call does not have an associated cir.try");
      return {};
    }

    if (tryOp.getSynthetic()) {
      cgf.cgm.errorNYI("emitCallLikeOp: tryOp synthetic");
      return {};
    }

    cir::CallOp callOpWithExceptions;
    if (indirectFuncTy) {
      cgf.cgm.errorNYI("emitCallLikeOp: indirect function type");
      return {};
    }

    callOpWithExceptions =
        builder.createCallOp(callLoc, directFuncOp, cirCallArgs);

    cgf.populateCatchHandlersIfRequired(tryOp);
    return callOpWithExceptions;
  }

  assert(builder.getInsertionBlock() && "expected valid basic block");

  cir::CallOp op;
  if (indirectFuncTy) {
    // TODO(cir): Set calling convention for indirect calls.
    assert(!cir::MissingFeatures::opCallCallConv());
    op = builder.createIndirectCallOp(callLoc, indirectFuncVal, indirectFuncTy,
                                      cirCallArgs, attrs);
  } else {
    cir::FuncType directTy = directFuncOp.getFunctionType();
    if (directFuncOp.isDeclaration() && !directTy.isVarArg() &&
        cirCallArgs.size() > directTy.getNumInputs()) {
      directFuncOp.setFunctionType(cir::FuncType::get(
          directTy.getInputs(), directTy.getReturnType(), /*isVarArg=*/true));
    }

    // The callee symbol may have been created with a slightly different
    // signature than the one derived for this call -- for example when a
    // builtin is remapped to a library function (__builtin_char_memchr ->
    // memchr) whose first declaration in this translation unit fixed a
    // differing-but-compatible pointee type.  In classic LLVM codegen this is
    // invisible because all object pointers share one `ptr` type, but CIR's
    // pointers are typed, so coerce any pointer argument whose type differs
    // from the callee's declared parameter type with an explicit bitcast.
    directTy = directFuncOp.getFunctionType();
    SmallVector<mlir::Value, 16> coercedArgs(cirCallArgs.begin(),
                                             cirCallArgs.end());
    for (unsigned i = 0, e = directTy.getNumInputs(); i != e && i < coercedArgs.size();
         ++i) {
      mlir::Type paramTy = directTy.getInput(i);
      mlir::Value arg = coercedArgs[i];
      if (arg.getType() != paramTy &&
          mlir::isa<cir::PointerType>(arg.getType()) &&
          mlir::isa<cir::PointerType>(paramTy))
        coercedArgs[i] = builder.createBitcast(arg, paramTy);
    }
    op = builder.createCallOp(callLoc, directFuncOp, coercedArgs, attrs);
  }

  return op;
}

static void deactivateArgCleanupsBeforeCall(CIRGenFunction &cgf,
                                            const CallArgList &args) {
  for (const auto &cleanup : llvm::reverse(args.getCleanupsToDeactivate()))
    cgf.DeactivateCleanupBlock(cleanup.cleanup, cleanup.isActiveIP);
}

const CIRGenFunctionInfo &
CIRGenTypes::arrangeFreeFunctionType(CanQual<FunctionProtoType> fpt) {
  SmallVector<CanQualType, 16> argTypes;
  assert(!cir::MissingFeatures::opCallFnInfoOpts());
  return ::arrangeCIRFunctionInfo(*this, argTypes, fpt);
}

const CIRGenFunctionInfo &
CIRGenTypes::arrangeFreeFunctionType(CanQual<FunctionNoProtoType> fnpt) {
  CanQualType resultType = fnpt->getReturnType().getUnqualifiedType();
  assert(!cir::MissingFeatures::opCallFnInfoOpts());
  return arrangeCIRFunctionInfo(resultType, {}, RequiredArgs(0));
}

RValue CIRGenFunction::emitCall(const CIRGenFunctionInfo &funcInfo,
                                const CIRGenCallee &callee,
                                ReturnValueSlot returnValue,
                                const CallArgList &args,
                                cir::CIRCallOpInterface *callOp,
                                mlir::Location loc) {
  QualType retTy = funcInfo.getReturnType();
  cir::FuncType cirFuncTy = getTypes().getFunctionType(funcInfo);

  auto getUnavailableCallResult = [&]() -> RValue {
    switch (getEvaluationKind(retTy)) {
    case cir::TEK_Aggregate: {
      Address dest = returnValue.getValue();
      if (!dest.isValid())
        dest = createMemTemp(retTy, loc, getCounterAggTmpAsString());
      return RValue::getAggregate(dest);
    }
    case cir::TEK_Scalar:
      return getUndefRValue(retTy);
    case cir::TEK_Complex: {
      mlir::Type retCIRTy = convertType(retTy);
      return RValue::getComplex(
          builder.getConstant(loc, cir::UndefAttr::get(retCIRTy)));
    }
    }
    llvm_unreachable("Invalid evaluation kind");
  };

  if (llvm::any_of(args, [](const CallArg &arg) { return arg.unavailable(); }))
    return getUnavailableCallResult();

  SmallVector<mlir::Value, 16> cirCallArgs(args.size());

  assert(!cir::MissingFeatures::emitLifetimeMarkers());

  // Translate all of the arguments as necessary to match the CIR lowering.
  for (auto [argNo, arg, canQualArgType] :
       llvm::enumerate(args, funcInfo.argTypes())) {

    // Insert a padding argument to ensure proper alignment.
    assert(!cir::MissingFeatures::opCallPaddingArgs());

    mlir::Type argType = convertType(canQualArgType);
    if (!mlir::isa<cir::RecordType>(argType) &&
        !mlir::isa<cir::ComplexType>(argType)) {
      mlir::Value v;
      if (arg.isAggregate())
        cgm.errorNYI(loc, "emitCall: aggregate call argument");
      v = arg.getKnownRValue().getValue();

      // An upstream NYI may have produced an argument with no backing value
      // (e.g. an unsupported l-value expression that returned an empty
      // RValue). Don't dereference it -- substitute a poison value so the call
      // can still be formed and the already-emitted diagnostic is reported,
      // rather than crashing here.
      if (!v) {
        cgm.errorNYI(loc, "emitCall: call argument without a value");
        v = builder.getConstant(loc, cir::PoisonAttr::get(argType));
      }

      // We might have to widen integers, but we should never truncate.
      if (argType != v.getType() && mlir::isa<cir::IntType>(v.getType()))
        cgm.errorNYI(loc, "emitCall: widening integer call argument");

      // If the argument doesn't match, perform a bitcast to coerce it. This
      // can happen due to trivial type mismatches.
      // TODO(cir): When getFunctionType is added, assert that this isn't
      // needed.
      assert(!cir::MissingFeatures::opCallBitcastArg());
      cirCallArgs[argNo] = v;
    } else {
      Address src = Address::invalid();
      if (!arg.isAggregate()) {
        src = createMemTemp(arg.ty, loc, "coerce");
        arg.copyInto(*this, src, loc);
      } else {
        src = arg.hasLValue() ? arg.getKnownLValue().getAddress()
                              : arg.getKnownRValue().getAggregateAddress();
      }

      // Fast-isel and the optimizer generally like scalar values better than
      // FCAs, so we flatten them if this is safe to do for this argument.
      mlir::Type srcTy = src.getElementType();
      // FIXME(cir): get proper location for each argument.
      mlir::Location argLoc = loc;

      // If the source type is smaller than the destination type of the
      // coerce-to logic, copy the source value into a temp alloca the size
      // of the destination type to allow loading all of it. The bits past
      // the source value are left undef.
      // FIXME(cir): add data layout info and compare sizes instead of
      // matching the types.
      //
      // uint64_t SrcSize = CGM.getDataLayout().getTypeAllocSize(SrcTy);
      // uint64_t DstSize = CGM.getDataLayout().getTypeAllocSize(STy);
      // if (SrcSize < DstSize) {
      assert(!cir::MissingFeatures::dataLayoutTypeAllocSize());
      if (srcTy != argType) {
        cgm.errorNYI(loc, "emitCall: source type does not match argument type");
      } else {
        // FIXME(cir): this currently only runs when the types are exactly the
        // same, but should be when alloc sizes are the same, fix this as soon
        // as datalayout gets introduced.
        assert(!cir::MissingFeatures::dataLayoutTypeAllocSize());
      }

      // assert(NumCIRArgs == STy.getMembers().size());
      // In LLVMGen: Still only pass the struct without any gaps but mark it
      // as such somehow.
      //
      // In CIRGen: Emit a load from the "whole" struct,
      // which shall be broken later by some lowering step into multiple
      // loads.
      assert(!cir::MissingFeatures::lowerAggregateLoadStore());
      cirCallArgs[argNo] = builder.createLoad(argLoc, src);
    }
  }

  const CIRGenCallee &concreteCallee = callee.prepareConcreteCallee(*this);
  mlir::Operation *calleePtr = concreteCallee.getFunctionPointer();

  assert(!cir::MissingFeatures::opCallInAlloca());

  mlir::NamedAttrList attrs;
  StringRef funcName;
  if (auto calleeFuncOp = dyn_cast<cir::FuncOp>(calleePtr))
    funcName = calleeFuncOp.getName();

  assert(!cir::MissingFeatures::opCallCallConv());
  assert(!cir::MissingFeatures::opCallAttrs());
  cgm.constructAttributeList(callee.getAbstractInfo(), attrs);

  cir::FuncType indirectFuncTy;
  mlir::Value indirectFuncVal;
  cir::FuncOp directFuncOp;
  if (auto fnOp = dyn_cast<cir::FuncOp>(calleePtr)) {
    directFuncOp = fnOp;
  } else if (auto getGlobalOp = mlir::dyn_cast<cir::GetGlobalOp>(calleePtr)) {
    // FIXME(cir): This peephole optimization avoids indirect calls for
    // builtins. This should be fixed in the builtin declaration instead by
    // not emitting an unecessary get_global in the first place.
    // However, this is also used for no-prototype functions.
    mlir::Operation *globalOp = cgm.getGlobalValue(getGlobalOp.getName());
    assert(globalOp && "undefined global function");
    directFuncOp = mlir::cast<cir::FuncOp>(globalOp);
  } else {
    [[maybe_unused]] mlir::ValueTypeRange<mlir::ResultRange> resultTypes =
        calleePtr->getResultTypes();
    [[maybe_unused]] auto funcPtrTy =
        mlir::dyn_cast<cir::PointerType>(resultTypes.front());
    assert(funcPtrTy && mlir::isa<cir::FuncType>(funcPtrTy.getPointee()) &&
           "expected pointer to function");

    indirectFuncTy = cirFuncTy;
    indirectFuncVal = calleePtr->getResult(0);
  }

  assert(!cir::MissingFeatures::msvcCXXPersonality());
  assert(!cir::MissingFeatures::functionUsesSEHTry());
  assert(!cir::MissingFeatures::nothrowAttr());

  deactivateArgCleanupsBeforeCall(*this, args);

  bool cannotThrow = attrs.getNamed("nothrow").has_value();
  bool isInvoke = !cannotThrow && isCatchOrCleanupRequired();

  mlir::Location callLoc = loc;
  cir::CIRCallOpInterface theCall =
      emitCallLikeOp(*this, loc, indirectFuncTy, indirectFuncVal, directFuncOp,
                     cirCallArgs, isInvoke, attrs);

  if (callOp)
    *callOp = theCall;

  assert(!cir::MissingFeatures::opCallMustTail());
  assert(!cir::MissingFeatures::opCallReturn());

  mlir::Type retCIRTy = convertType(retTy);
  if (isa<cir::VoidType>(retCIRTy))
    return getUndefRValue(retTy);
  switch (getEvaluationKind(retTy)) {
  case cir::TEK_Aggregate: {
    Address destPtr = returnValue.getValue();

    if (!destPtr.isValid())
      destPtr = createMemTemp(retTy, callLoc, getCounterAggTmpAsString());

    mlir::ResultRange results = theCall->getOpResults();
    assert(results.size() <= 1 && "multiple returns from a call");

    SourceLocRAIIObject loc{*this, callLoc};
    emitAggregateStore(results[0], destPtr);
    return RValue::getAggregate(destPtr);
  }
  case cir::TEK_Scalar: {
    mlir::ResultRange results = theCall->getOpResults();
    assert(results.size() == 1 && "unexpected number of returns");

    // If the result type doesn't match, perform a bitcast to coerce it. This
    // can happen due to trivial type mismatches, e.g. when a builtin is
    // remapped to a library function whose declared return type differs only
    // in pointee type (as for __builtin_char_memchr -> memchr, which returns
    // `char *` rather than `void *`).  Unlike classic LLVM codegen, where all
    // object pointers share a single `ptr` type, CIR distinguishes pointee
    // types, so the coercion must be made explicit here.  Only pointer-to-
    // pointer coercions are safe to bitcast; other scalar mismatches remain
    // unhandled.
    mlir::Value callResult = results[0];
    if (callResult.getType() != retCIRTy) {
      if (mlir::isa<cir::PointerType>(callResult.getType()) &&
          mlir::isa<cir::PointerType>(retCIRTy))
        callResult = builder.createBitcast(callResult, retCIRTy);
      else
        cgm.errorNYI(loc, "bitcast on function return value");
    }

    mlir::Region *region = builder.getBlock()->getParent();
    if (region != theCall->getParentRegion())
      cgm.errorNYI(loc, "function calls with cleanup");

    return RValue::get(callResult);
  }
  case cir::TEK_Complex: {
    mlir::ResultRange results = theCall->getOpResults();
    assert(!results.empty() &&
           "Expected at least one result for complex rvalue");
    return RValue::getComplex(results[0]);
  }
  }
  llvm_unreachable("Invalid evaluation kind");
}

void CallArg::copyInto(CIRGenFunction &cgf, Address addr,
                       mlir::Location loc) const {
  LValue dst = cgf.makeAddrLValue(addr, ty);
  if (!hasLV && rv.isScalar())
    cgf.cgm.errorNYI(loc, "copyInto scalar value");
  else if (!hasLV && rv.isComplex())
    cgf.emitStoreOfComplex(loc, rv.getComplexValue(), dst, /*isInit=*/true);
  else
    cgf.cgm.errorNYI(loc, "copyInto hasLV");
  isUsed = true;
}

mlir::Value CIRGenFunction::emitRuntimeCall(mlir::Location loc,
                                            cir::FuncOp callee,
                                            ArrayRef<mlir::Value> args) {
  // TODO(cir): set the calling convention to this runtime call.
  assert(!cir::MissingFeatures::opFuncCallingConv());

  cir::CallOp call = builder.createCallOp(loc, callee, args);
  assert(call->getNumResults() <= 1 &&
         "runtime functions have at most 1 result");

  if (call->getNumResults() == 0)
    return nullptr;

  return call->getResult(0);
}

void CIRGenFunction::emitCallArg(CallArgList &args, const clang::Expr *e,
                                 clang::QualType argType) {
  // In well-formed code the value category of the argument expression matches
  // the reference-ness of the parameter type: a reference parameter binds to a
  // glvalue, a by-value parameter takes a prvalue. If that invariant is broken
  // here, the prototype's parameter types and the call's argument expressions
  // are misaligned (something CIRGen does not model correctly yet). Emit a
  // clean NYI diagnostic instead of asserting/crashing -- producing a wrong
  // value here would be unsound for a downstream consumer of the CIR.
  if (argType->isReferenceType() != e->isGLValue()) {
    cgm.errorNYI(e->getSourceRange(),
                 "emitCallArg: argument value category does not match "
                 "parameter type");
    // The TU is now poisoned, but emission continues; downstream code (e.g.
    // emitCall) still expects exactly one well-typed value to have been
    // appended. Append a poison value of the (memory) parameter type so we
    // don't crash before the diagnostic is reported.
    mlir::Location loc = getLoc(e->getSourceRange());
    mlir::Type ty = convertType(argType);
    mlir::Value poison =
        builder.getConstant(loc, cir::PoisonAttr::get(ty));
    args.addUnavailable(RValue::get(poison), argType);
    return;
  }

  if (e->isGLValue()) {
    assert(e->getObjectKind() == OK_Ordinary);
    return args.add(emitReferenceBindingToExpr(e), argType);
  }

  bool hasAggregateEvalKind = hasAggregateEvaluationKind(argType);

  // When aggregate arguments are destructed by the callee (the Itanium C++ ABI
  // rule for non-trivially-destructible by-value params, and all by-value
  // record params in the Microsoft ABI), construct the argument into a
  // temporary and mark the slot as externally destructed so we don't also
  // destroy it on the caller side.
  if (argType->isRecordType() &&
      argType->castAsRecordDecl()->isParamDestroyedInCallee()) {
    // inalloca is an MS-ABI x86 concept that is not yet modeled in CIR.
    assert(!cir::MissingFeatures::opCallInAlloca());
    AggValueSlot slot =
        createAggTemp(argType, getLoc(e->getSourceRange()), "agg.tmp");

    bool destroyedInCallee = true, needsCleanup = true;
    if (const auto *rd = argType->getAsCXXRecordDecl())
      destroyedInCallee = rd->hasNonTrivialDestructor();
    else
      needsCleanup = argType.isDestructedType();

    if (destroyedInCallee)
      slot.setExternallyDestructed();

    emitAggExpr(e, slot);
    RValue rv = slot.asRValue();
    args.add(rv, argType);

    if (destroyedInCallee && needsCleanup) {
      // The callee destroys the argument. The caller-side EH-only cleanup that
      // would run if we unwind before the call (and its deactivation at the
      // call site) is not yet modeled in CIR; the externally-destructed slot
      // already prevents a double destroy on the normal path.
      assert(!cir::MissingFeatures::cleanupsToDeactivate());
    }
    return;
  }

  if (hasAggregateEvalKind && isa<ImplicitCastExpr>(e) &&
      cast<CastExpr>(e)->getCastKind() == CK_LValueToRValue) {
    LValue lv = emitLValue(cast<CastExpr>(e)->getSubExpr());
    assert(lv.isSimple());
    args.addUncopiedAggregate(lv, argType);
    return;
  }

  RValue rv = emitAnyExprToTemp(e);
  if (rv.isScalar() && !rv.getValue()) {
    mlir::Location loc = getLoc(e->getSourceRange());
    mlir::Type ty = convertType(argType);
    mlir::Value poison =
        builder.getConstant(loc, cir::PoisonAttr::get(ty));
    args.addUnavailable(RValue::get(poison), argType);
    return;
  }

  args.add(rv, argType);
}

QualType CIRGenFunction::getVarArgType(const Expr *arg) {
  // System headers on Windows define NULL to 0 instead of 0LL on Win64. MSVC
  // implicitly widens null pointer constants that are arguments to varargs
  // functions to pointer-sized ints.
  if (!getTarget().getTriple().isOSWindows())
    return arg->getType();

  assert(!cir::MissingFeatures::msabi());
  cgm.errorNYI(arg->getSourceRange(), "getVarArgType: NYI for Windows target");
  return arg->getType();
}

/// Similar to emitAnyExpr(), however, the result will always be accessible
/// even if no aggregate location is provided.
RValue CIRGenFunction::emitAnyExprToTemp(const Expr *e) {
  AggValueSlot aggSlot = AggValueSlot::ignored();

  if (hasAggregateEvaluationKind(e->getType()))
    aggSlot = createAggTemp(e->getType(), getLoc(e->getSourceRange()),
                            getCounterAggTmpAsString());

  return emitAnyExpr(e, aggSlot);
}

void CIRGenFunction::emitCallArgs(
    CallArgList &args, PrototypeWrapper prototype,
    llvm::iterator_range<clang::CallExpr::const_arg_iterator> argRange,
    AbstractCallee callee, unsigned paramsToSkip) {
  llvm::SmallVector<QualType, 16> argTypes;

  assert(!cir::MissingFeatures::opCallCallConv());

  // First, if a prototype was provided, use those argument types.
  bool isVariadic = false;
  if (prototype.p) {
    assert(!cir::MissingFeatures::opCallObjCMethod());

    const auto *fpt = cast<const FunctionProtoType *>(prototype.p);
    isVariadic = fpt->isVariadic();
    assert(!cir::MissingFeatures::opCallCallConv());
    argTypes.assign(fpt->param_type_begin() + paramsToSkip,
                    fpt->param_type_end());
  }

  // If we still have any arguments, emit them using the type of the argument.
  for (const clang::Expr *a : llvm::drop_begin(argRange, argTypes.size()))
    argTypes.push_back(isVariadic ? getVarArgType(a) : a->getType());
  assert(argTypes.size() == (size_t)(argRange.end() - argRange.begin()));

  // We must evaluate arguments from right to left in the MS C++ ABI, because
  // arguments are destroyed left to right in the callee. As a special case,
  // there are certain language constructs taht require left-to-right
  // evaluation, and in those cases we consider the evaluation order requirement
  // to trump the "destruction order is reverse construction order" guarantee.
  auto leftToRight = true;
  assert(!cir::MissingFeatures::msabi());

  auto maybeEmitImplicitObjectSize = [&](size_t i, const Expr *arg,
                                         RValue emittedArg) {
    if (!callee.hasFunctionDecl() || i >= callee.getNumParams())
      return;
    auto *ps = callee.getParamDecl(i)->getAttr<PassObjectSizeAttr>();
    if (!ps)
      return;

    assert(!cir::MissingFeatures::opCallImplicitObjectSizeArgs());
    cgm.errorNYI("emit implicit object size for call arg");
  };

  // Evaluate each argument in the appropriate order.
  size_t callArgsStart = args.size();
  for (size_t i = 0; i != argTypes.size(); ++i) {
    size_t idx = leftToRight ? i : argTypes.size() - i - 1;
    CallExpr::const_arg_iterator currentArg = argRange.begin() + idx;
    size_t initialArgSize = args.size();

    emitCallArg(args, *currentArg, argTypes[idx]);

    // In particular, we depend on it being the last arg in Args, and the
    // objectsize bits depend on there only being one arg if !LeftToRight.
    assert(initialArgSize + 1 == args.size() &&
           "The code below depends on only adding one arg per emitCallArg");
    (void)initialArgSize;

    // Since pointer argument are never emitted as LValue, it is safe to emit
    // non-null argument check for r-value only.
    if (!args.back().hasLValue()) {
      RValue rvArg = args.back().getKnownRValue();
      assert(!cir::MissingFeatures::sanitizers());
      maybeEmitImplicitObjectSize(idx, *currentArg, rvArg);
    }

    if (!leftToRight)
      std::reverse(args.begin() + callArgsStart, args.end());
  }
}
