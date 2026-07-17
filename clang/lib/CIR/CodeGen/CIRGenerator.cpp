//===--- CIRGenerator.cpp - Emit CIR from ASTs ----------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This builds an AST and converts it to CIR.
//
//===----------------------------------------------------------------------===//

#include "CIRGenModule.h"

#include "mlir/Dialect/DLTI/DLTI.h"
#include "mlir/Dialect/OpenACC/OpenACC.h"
#include "mlir/Dialect/OpenMP/OpenMPDialect.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Target/LLVMIR/Import.h"

#include "clang/AST/DeclGroup.h"
#include "clang/CIR/CIRGenerator.h"
#include "clang/CIR/InitAllDialects.h"
#include "clang/Sema/Sema.h"
#include "llvm/IR/DataLayout.h"

using namespace cir;
using namespace clang;

void CIRGenerator::anchor() {}

CIRGenerator::CIRGenerator(clang::DiagnosticsEngine &diags,
                           llvm::IntrusiveRefCntPtr<llvm::vfs::FileSystem> vfs,
                           const CodeGenOptions &cgo)
    : diags(diags), fs(std::move(vfs)), codeGenOpts{cgo},
      handlingTopLevelDecls{0} {}
CIRGenerator::~CIRGenerator() {
  // There should normally not be any leftover inline method definitions.
  assert(deferredInlineMemberFuncDefs.empty() || diags.hasErrorOccurred());
}

static void setMLIRDataLayout(mlir::ModuleOp &mod, const llvm::DataLayout &dl) {
  mlir::MLIRContext *mlirContext = mod.getContext();
  mlir::DataLayoutSpecInterface dlSpec =
      mlir::translateDataLayout(dl, mlirContext);
  mod->setAttr(mlir::DLTIDialect::kDataLayoutAttrName, dlSpec);
}

void CIRGenerator::Initialize(ASTContext &astContext) {
  using namespace llvm;

  this->astContext = &astContext;

  mlirContext = std::make_unique<mlir::MLIRContext>();
  cir::registerAllDialects(*mlirContext);
  mlirContext->loadDialect<mlir::DLTIDialect, cir::CIRDialect>();
  mlirContext->getOrLoadDialect<mlir::acc::OpenACCDialect>();
  mlirContext->getOrLoadDialect<mlir::omp::OpenMPDialect>();

  cgm = std::make_unique<clang::CIRGen::CIRGenModule>(
      *mlirContext.get(), astContext, codeGenOpts, diags);
  mlir::ModuleOp mod = cgm->getModule();
  llvm::DataLayout layout =
      llvm::DataLayout(astContext.getTargetInfo().getDataLayoutString());
  setMLIRDataLayout(mod, layout);
}

void CIRGenerator::InitializeSema(Sema &sema) { this->sema = &sema; }

void CIRGenerator::ForgetSema() { sema = nullptr; }

void CIRGenerator::defineSelectedDefaultedMethod(CXXMethodDecl *method) {
  assert(sema && "selected defaulted method definition requires Sema");
  if (!method->isDefaulted() || method->isDeleted() ||
      method->doesThisDeclarationHaveABody() ||
      !cgm->shouldParseSelectedDeclBody(method))
    return;

  SourceLocation loc = method->getLocation();
  if (auto *ctor = dyn_cast<CXXConstructorDecl>(method)) {
    if (ctor->isDefaultConstructor())
      sema->DefineImplicitDefaultConstructor(loc, ctor);
    else if (ctor->isCopyConstructor())
      sema->DefineImplicitCopyConstructor(loc, ctor);
    else if (ctor->isMoveConstructor())
      sema->DefineImplicitMoveConstructor(loc, ctor);
  } else if (auto *dtor = dyn_cast<CXXDestructorDecl>(method)) {
    sema->DefineImplicitDestructor(loc, dtor);
  } else if (method->isCopyAssignmentOperator()) {
    sema->DefineImplicitCopyAssignment(loc, method);
  } else if (method->isMoveAssignmentOperator()) {
    sema->DefineImplicitMoveAssignment(loc, method);
  } else {
    auto comparisonKind = sema->getDefaultedComparisonKind(method);
    if (comparisonKind != Sema::DefaultedComparisonKind::None)
      sema->DefineDefaultedComparison(loc, method, comparisonKind);
  }
}

void CIRGenerator::defineSelectedDependencyMethods() {
  llvm::SmallVector<CXXMethodDecl *, 16> methods;
  for (GlobalDecl gd : cgm->getSelectedDeclDependencies())
    if (auto *method = dyn_cast<CXXMethodDecl>(gd.getDecl()))
      methods.push_back(const_cast<CXXMethodDecl *>(method));
  for (CXXMethodDecl *method : methods)
    defineSelectedDefaultedMethod(method);
}

void CIRGenerator::defineSelectedDefaultedMethods(DeclContext *context) {
  for (Decl *decl : context->decls()) {
    if (auto *method = dyn_cast<CXXMethodDecl>(decl))
      defineSelectedDefaultedMethod(method);
    if (auto *nested = dyn_cast<DeclContext>(decl))
      defineSelectedDefaultedMethods(nested);
  }
}

bool CIRGenerator::verifyModule() const { return cgm->verifyModule(); }

mlir::ModuleOp CIRGenerator::getModule() const { return cgm->getModule(); }

bool CIRGenerator::HandleTopLevelDecl(DeclGroupRef group) {
  if (diags.hasUnrecoverableErrorOccurred())
    return true;

  HandlingTopLevelDeclRAII handlingDecl(*this);

  for (Decl *decl : group)
    cgm->emitTopLevelDecl(decl);

  return true;
}

void CIRGenerator::HandleTranslationUnit(ASTContext &astContext) {
  // Selected defaulted methods can introduce further implicit destructor and
  // constructor dependencies. Define and emit until the exact dependency set
  // reaches a fixed point.
  if (!diags.hasErrorOccurred() && cgm) {
    if (sema) {
      size_t dependencyCount = 0;
      do {
        dependencyCount = cgm->getSelectedDeclDependencyCount();
        defineSelectedDefaultedMethods(astContext.getTranslationUnitDecl());
        defineSelectedDependencyMethods();
        cgm->emitSelectedMethods(astContext.getTranslationUnitDecl());
        cgm->emitSelectedDependencies();
      } while (cgm->getSelectedDeclDependencyCount() != dependencyCount);
    } else {
      cgm->emitSelectedMethods(astContext.getTranslationUnitDecl());
      cgm->emitSelectedDependencies();
    }
    cgm->release();
  }

  // If there are errors before or when releasing the cgm, reset the module to
  // stop here before invoking the backend.
  assert(!cir::MissingFeatures::cleanupAfterErrorDiags());
}

void CIRGenerator::HandleInlineFunctionDefinition(FunctionDecl *d) {
  if (diags.hasErrorOccurred())
    return;

  if (!cgm->shouldParseSelectedDeclBody(d))
    return;

  assert(d->doesThisDeclarationHaveABody());

  // We may want to emit this definition. However, that decision might be
  // based on computing the linkage, and we have to defer that in case we are
  // inside of something that will chagne the method's final linkage, e.g.
  //   typedef struct {
  //     void bar();
  //     void foo() { bar(); }
  //   } A;
  deferredInlineMemberFuncDefs.push_back(d);

  // Provide some coverage mapping even for methods that aren't emitted.
  // Don't do this for templated classes though, as they may not be
  // instantiable.
  assert(!cir::MissingFeatures::coverageMapping());
}

void CIRGenerator::emitDeferredDecls() {
  if (deferredInlineMemberFuncDefs.empty())
    return;

  // Emit any deferred inline method definitions. Note that more deferred
  // methods may be added during this loop, since ASTConsumer callbacks can be
  // invoked if AST inspection results in declarations being added. Therefore,
  // we use an index to loop over the deferredInlineMemberFuncDefs rather than
  // a range.
  HandlingTopLevelDeclRAII handlingDecls(*this);
  for (unsigned i = 0; i != deferredInlineMemberFuncDefs.size(); ++i)
    cgm->emitTopLevelDecl(deferredInlineMemberFuncDefs[i]);
  deferredInlineMemberFuncDefs.clear();
}

/// HandleTagDeclDefinition - This callback is invoked each time a TagDecl to
/// (e.g. struct, union, enum, class) is completed. This allows the client to
/// hack on the type, which can occur at any point in the file (because these
/// can be defined in declspecs).
void CIRGenerator::HandleTagDeclDefinition(TagDecl *d) {
  if (diags.hasErrorOccurred())
    return;

  if (!codeGenOpts.ClangIRSelectedDeclsFile.empty())
    return;

  // Don't allow re-entrant calls to CIRGen triggered by PCH deserialization to
  // emit deferred decls.
  HandlingTopLevelDeclRAII handlingDecl(*this, /*EmitDeferred=*/false);

  cgm->updateCompletedType(d);

  // For MSVC compatibility, treat declarations of static data members with
  // inline initializers as definitions.
  if (astContext->getTargetInfo().getCXXABI().isMicrosoft())
    cgm->errorNYI(d->getSourceRange(), "HandleTagDeclDefinition: MSABI");

  // For OpenMP emit declare reduction functions or declare mapper, if
  // required.
  if (astContext->getLangOpts().OpenMP) {
    for (Decl *member : d->decls()) {
      if (auto *drd = dyn_cast<OMPDeclareReductionDecl>(member)) {
        if (astContext->DeclMustBeEmitted(drd))
          cgm->errorNYI(d->getSourceRange(),
                        "HandleTagDeclDefinition: OMPDeclareReductionDecl");
      } else if (auto *dmd = dyn_cast<OMPDeclareMapperDecl>(member)) {
        if (astContext->DeclMustBeEmitted(dmd))
          cgm->errorNYI(d->getSourceRange(),
                        "HandleTagDeclDefinition: OMPDeclareMapperDecl");
      }
    }
  }
}

void CIRGenerator::HandleTagDeclRequiredDefinition(const TagDecl *D) {
  if (diags.hasErrorOccurred())
    return;

  assert(!cir::MissingFeatures::generateDebugInfo());
}

void CIRGenerator::HandleCXXStaticMemberVarInstantiation(VarDecl *D) {
  if (diags.hasErrorOccurred())
    return;

  if (!cgm->shouldEmitSelectedDeclRoot(GlobalDecl(D)))
    return;

  cgm->handleCXXStaticMemberVarInstantiation(D);
}

void CIRGenerator::HandleOpenACCRoutineReference(const FunctionDecl *FD,
                                                 const OpenACCRoutineDecl *RD) {
  llvm::StringRef mangledName = cgm->getMangledName(FD);
  cir::FuncOp entry =
      mlir::dyn_cast_if_present<cir::FuncOp>(cgm->getGlobalValue(mangledName));

  // if this wasn't generated, don't force it to be.
  if (!entry)
    return;
  cgm->emitOpenACCRoutineDecl(FD, entry, RD->getBeginLoc(), RD->clauses());
}

void CIRGenerator::CompleteTentativeDefinition(VarDecl *d) {
  if (diags.hasErrorOccurred())
    return;

  if (!cgm->shouldEmitSelectedDeclRoot(GlobalDecl(d)))
    return;

  cgm->emitTentativeDefinition(d);
}

void CIRGenerator::HandleVTable(CXXRecordDecl *rd) {
  if (diags.hasErrorOccurred())
    return;

  if (!codeGenOpts.ClangIRSelectedDeclsFile.empty())
    return;

  cgm->emitVTable(rd);
}

bool CIRGenerator::shouldSkipFunctionBody(Decl *d) {
  if (codeGenOpts.ClangIRSelectedDeclsFile.empty())
    return false;
  const auto *fd = dyn_cast<FunctionDecl>(d);
  if (fd && (fd->getDeclContext()->isDependentContext() ||
             fd->getTemplatedKind() != FunctionDecl::TK_NonTemplate))
    return false;
  return !fd || !cgm->shouldParseSelectedDeclBody(fd);
}
