//===- CIRGenModule.cpp - Per-Module state for CIR generation -------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is the internal per-translation-unit state used for CIR translation.
//
//===----------------------------------------------------------------------===//

#include "CIRGenModule.h"
#include "CIRGenCUDARuntime.h"
#include "CIRGenCXXABI.h"
#include "CIRGenConstantEmitter.h"
#include "CIRGenFunction.h"

#include "mlir/Dialect/OpenMP/OpenMPOffloadUtils.h"
#include "mlir/IR/AttrTypeSubElements.h"
#include "mlir/IR/SymbolTable.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/ASTLambda.h"
#include "clang/AST/EvaluatedExprVisitor.h"
#include "clang/AST/Attrs.inc"
#include "clang/AST/DeclBase.h"
#include "clang/AST/DeclObjC.h"
#include "clang/AST/DeclOpenACC.h"
#include "clang/AST/GlobalDecl.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/AST/StmtOpenMP.h"
#include "clang/Basic/DiagnosticFrontend.h"
#include "clang/Basic/SourceManager.h"
#include "clang/CIR/Dialect/IR/CIRAttrs.h"
#include "clang/CIR/Dialect/IR/CIRDialect.h"
#include "clang/CIR/Dialect/IR/CIROpsEnums.h"
#include "clang/CIR/Dialect/IR/CIRTypes.h"
#include "clang/CIR/Interfaces/CIROpInterfaces.h"
#include "clang/CIR/MissingFeatures.h"
#include "clang/Lex/Lexer.h"
#include "clang/UnifiedSymbolResolution/USRGeneration.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SaveAndRestore.h"
#include "llvm/Support/TimeProfiler.h"
#include "llvm/Support/raw_ostream.h"

#include "CIRGenFunctionInfo.h"
#include "TargetInfo.h"
#include "mlir/Dialect/Ptr/IR/MemorySpaceInterfaces.h"
#include "mlir/IR/Attributes.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/Location.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Verifier.h"

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <functional>
#include <limits>

using namespace clang;
using namespace clang::CIRGen;

static std::optional<std::string>
sourceLocationIdentity(const ASTContext &astContext, SourceLocation location) {
  if (location.isInvalid())
    return std::nullopt;

  const SourceManager &sourceManager = astContext.getSourceManager();
  SourceLocation spellingLocation =
      location.isMacroID() ? sourceManager.getExpansionLoc(location)
                           : sourceManager.getSpellingLoc(location);
  if (spellingLocation.isInvalid())
    return std::nullopt;
  FileIDAndOffset decomposed = sourceManager.getDecomposedLoc(spellingLocation);
  if (decomposed.first.isInvalid())
    return std::nullopt;
  PresumedLoc presumed = sourceManager.getPresumedLoc(spellingLocation);
  if (!presumed.isValid())
    return std::nullopt;

  // Header search can spell one file several ways (gcc toolchain include
  // paths traverse ".." chains that remove_dots can even clamp at the
  // filesystem root). The FileEntry's resolved real path is the one spelling
  // every compiler instance agrees on; fall back to the working-directory
  // normalization only when no real path is recorded.
  llvm::SmallString<256> normalizedFile;
  if (auto fileRef = sourceManager.getFileEntryRefForID(decomposed.first)) {
    llvm::StringRef realPath = fileRef->getFileEntry().tryGetRealPathName();
    if (!realPath.empty() && llvm::sys::path::is_absolute(realPath))
      normalizedFile = realPath;
  }
  if (normalizedFile.empty())
    normalizedFile = sourceManager.getFilename(spellingLocation);
  if (normalizedFile.empty())
    return std::nullopt;
  if (!llvm::sys::path::is_absolute(normalizedFile)) {
    llvm::StringRef workingDirectory =
        sourceManager.getFileManager().getFileSystemOpts().WorkingDir;
    if (workingDirectory.empty())
      return std::nullopt;
    llvm::SmallString<256> resolvedFile(workingDirectory);
    llvm::sys::path::append(resolvedFile, normalizedFile);
    normalizedFile = resolvedFile;
  }
  if (auto fileRef = sourceManager.getFileEntryRefForID(decomposed.first)) {
    llvm::StringRef canonical =
        sourceManager.getFileManager().getCanonicalName(*fileRef);
    if (!canonical.empty() && llvm::sys::path::is_absolute(canonical))
      normalizedFile = canonical;
  }
  llvm::sys::path::remove_dots(normalizedFile, /*remove_dot_dot=*/true);
  if (normalizedFile.empty() || !llvm::sys::path::is_absolute(normalizedFile))
    return std::nullopt;

  std::string identity;
  llvm::raw_string_ostream stream(identity);
  stream << "v1:" << normalizedFile.size() << ':' << normalizedFile << ':'
         << decomposed.second << ':' << presumed.getLine() << ':'
         << presumed.getColumn();
  stream.flush();
  return identity;
}

static std::optional<std::string>
specializationPointOfInstantiationIdentity(const ASTContext &astContext,
                                           const FunctionDecl *functionDecl) {
  return sourceLocationIdentity(astContext,
                                functionDecl->getPointOfInstantiation());
}
static std::atomic<uint64_t> diagnosticCensusSequence = 0;

static std::unique_ptr<llvm::raw_fd_ostream> openDiagnosticCensus() {
  const char *path = std::getenv("HELIOS_CIR_DIAGNOSTIC_CENSUS_PATH");
  if (!path)
    return nullptr;

  std::error_code error;
  auto stream = std::make_unique<llvm::raw_fd_ostream>(
      path, error, llvm::sys::fs::OF_Text | llvm::sys::fs::OF_Append);
  if (error)
    llvm::report_fatal_error(
        llvm::Twine("cannot open requested CIR diagnostic census '") + path +
        "': " + error.message());
  return stream;
}

static CIRGenCXXABI *createCXXABI(CIRGenModule &cgm) {
  switch (cgm.getASTContext().getCXXABIKind()) {
  case TargetCXXABI::GenericItanium:
  case TargetCXXABI::GenericAArch64:
  case TargetCXXABI::AppleARM64:
  case TargetCXXABI::GenericARM:
    return CreateCIRGenItaniumCXXABI(cgm);

  case TargetCXXABI::Fuchsia:
  case TargetCXXABI::iOS:
  case TargetCXXABI::WatchOS:
  case TargetCXXABI::GenericMIPS:
  case TargetCXXABI::WebAssembly:
  case TargetCXXABI::XL:
  case TargetCXXABI::Microsoft:
    cgm.errorNYI("createCXXABI: C++ ABI kind");
    return nullptr;
  }

  llvm_unreachable("invalid C++ ABI kind");
}

CIRGenModule::CIRGenModule(mlir::MLIRContext &mlirContext,
                           clang::ASTContext &astContext,
                           const clang::CodeGenOptions &cgo,
                           DiagnosticsEngine &diags)
    : builder(mlirContext, *this), astContext(astContext),
      langOpts(astContext.getLangOpts()), codeGenOpts(cgo),
      theModule{mlir::ModuleOp::create(mlir::UnknownLoc::get(&mlirContext))},
      diags(diags), target(astContext.getTargetInfo()),
      diagnosticCensus(openDiagnosticCensus()), abi(createCXXABI(*this)),
      genTypes(*this), vtables(*this) {
  if (abi && !codeGenOpts.ClangIRSelectedDeclsFile.empty())
    abi->getMangleContext().enableDeterministicAnonymousStructIds();
  loadSelectedDeclRoots();

  // Initialize cached types
  voidTy = cir::VoidType::get(&getMLIRContext());
  voidPtrTy = cir::PointerType::get(voidTy);
  sInt8Ty = cir::IntType::get(&getMLIRContext(), 8, /*isSigned=*/true);
  sInt16Ty = cir::IntType::get(&getMLIRContext(), 16, /*isSigned=*/true);
  sInt32Ty = cir::IntType::get(&getMLIRContext(), 32, /*isSigned=*/true);
  sInt64Ty = cir::IntType::get(&getMLIRContext(), 64, /*isSigned=*/true);
  sInt128Ty = cir::IntType::get(&getMLIRContext(), 128, /*isSigned=*/true);
  uInt8Ty = cir::IntType::get(&getMLIRContext(), 8, /*isSigned=*/false);
  uInt8PtrTy = cir::PointerType::get(uInt8Ty);
  cirAllocaAddressSpace = getTargetCIRGenInfo().getCIRAllocaAddressSpace();
  uInt16Ty = cir::IntType::get(&getMLIRContext(), 16, /*isSigned=*/false);
  uInt32Ty = cir::IntType::get(&getMLIRContext(), 32, /*isSigned=*/false);
  uInt64Ty = cir::IntType::get(&getMLIRContext(), 64, /*isSigned=*/false);
  uInt128Ty = cir::IntType::get(&getMLIRContext(), 128, /*isSigned=*/false);
  fP16Ty = cir::FP16Type::get(&getMLIRContext());
  bFloat16Ty = cir::BF16Type::get(&getMLIRContext());
  floatTy = cir::SingleType::get(&getMLIRContext());
  doubleTy = cir::DoubleType::get(&getMLIRContext());
  fP80Ty = cir::FP80Type::get(&getMLIRContext());
  fP128Ty = cir::FP128Type::get(&getMLIRContext());

  allocaInt8PtrTy = cir::PointerType::get(uInt8Ty, cirAllocaAddressSpace);

  PointerAlignInBytes =
      astContext
          .toCharUnitsFromBits(
              astContext.getTargetInfo().getPointerAlign(LangAS::Default))
          .getQuantity();

  const unsigned charSize = astContext.getTargetInfo().getCharWidth();
  uCharTy = cir::IntType::get(&getMLIRContext(), charSize, /*isSigned=*/false);

  // TODO(CIR): Should be updated once TypeSizeInfoAttr is upstreamed
  const unsigned sizeTypeSize =
      astContext.getTypeSize(astContext.getSignedSizeType());
  SizeSizeInBytes = astContext.toCharUnitsFromBits(sizeTypeSize).getQuantity();
  // In CIRGenTypeCache, UIntPtrTy and SizeType are fields of the same union
  uIntPtrTy =
      cir::IntType::get(&getMLIRContext(), sizeTypeSize, /*isSigned=*/false);
  ptrDiffTy =
      cir::IntType::get(&getMLIRContext(), sizeTypeSize, /*isSigned=*/true);

  std::optional<cir::SourceLanguage> sourceLanguage = getCIRSourceLanguage();
  if (sourceLanguage)
    theModule->setAttr(
        cir::CIRDialect::getSourceLanguageAttrName(),
        cir::SourceLanguageAttr::get(&mlirContext, *sourceLanguage));
  theModule->setAttr(cir::CIRDialect::getTripleAttrName(),
                     builder.getStringAttr(getTriple().str()));

  if (cgo.OptimizationLevel > 0 || cgo.OptimizeSize > 0)
    theModule->setAttr(cir::CIRDialect::getOptInfoAttrName(),
                       cir::OptInfoAttr::get(&mlirContext,
                                             cgo.OptimizationLevel,
                                             cgo.OptimizeSize));

  if (langOpts.OpenMP) {
    mlir::omp::OffloadModuleOpts ompOpts(
        langOpts.OpenMPTargetDebug, langOpts.OpenMPTeamSubscription,
        langOpts.OpenMPThreadSubscription, langOpts.OpenMPNoThreadState,
        langOpts.OpenMPNoNestedParallelism, langOpts.OpenMPIsTargetDevice,
        getTriple().isGPU(), langOpts.OpenMPForceUSM, langOpts.OpenMP,
        langOpts.OMPHostIRFile, langOpts.OMPTargetTriples, langOpts.NoGPULib);
    mlir::omp::setOffloadModuleInterfaceAttributes(theModule, ompOpts);
  }

  if (langOpts.CUDA)
    createCUDARuntime();
  if (langOpts.OpenMP)
    createOpenMPRuntime();

  // Set the module name to be the name of the main file. TranslationUnitDecl
  // often contains invalid source locations and isn't a reliable source for the
  // module location.
  FileID mainFileId = astContext.getSourceManager().getMainFileID();
  const FileEntry &mainFile =
      *astContext.getSourceManager().getFileEntryForID(mainFileId);
  StringRef path = mainFile.tryGetRealPathName();
  if (!path.empty()) {
    theModule.setSymName(path);
    theModule->setLoc(mlir::FileLineColLoc::get(&mlirContext, path,
                                                /*line=*/0,
                                                /*column=*/0));
  }

  // Set CUDA GPU binary handle.
  if (langOpts.CUDA) {
    llvm::StringRef cudaBinaryName = codeGenOpts.CudaGpuBinaryFileName;
    if (!cudaBinaryName.empty()) {
      theModule->setAttr(cir::CIRDialect::getCUDABinaryHandleAttrName(),
                         cir::CUDABinaryHandleAttr::get(
                             &mlirContext, mlir::StringAttr::get(
                                               &mlirContext, cudaBinaryName)));
    }
  }
}

CIRGenModule::~CIRGenModule() = default;

void CIRGenModule::createCUDARuntime() {
  cudaRuntime.reset(createNVCUDARuntime(*this));
}

void CIRGenModule::createOpenMPRuntime() {
  openMPRuntime = std::make_unique<CIRGenOpenMPRuntime>(*this);
}

/// FIXME: this could likely be a common helper and not necessarily related
/// with codegen.
/// Return the best known alignment for an unknown pointer to a
/// particular class.
CharUnits CIRGenModule::getClassPointerAlignment(const CXXRecordDecl *rd) {
  if (!rd->hasDefinition())
    return CharUnits::One(); // Hopefully won't be used anywhere.

  auto &layout = astContext.getASTRecordLayout(rd);

  // If the class is final, then we know that the pointer points to an
  // object of that type and can use the full alignment.
  if (rd->isEffectivelyFinal())
    return layout.getAlignment();

  // Otherwise, we have to assume it could be a subclass.
  return layout.getNonVirtualAlignment();
}

CharUnits CIRGenModule::getNaturalTypeAlignment(QualType t,
                                                LValueBaseInfo *baseInfo,
                                                bool forPointeeType) {
  assert(!cir::MissingFeatures::opTBAA());

  // FIXME: This duplicates logic in ASTContext::getTypeAlignIfKnown, but
  // that doesn't return the information we need to compute baseInfo.

  // Honor alignment typedef attributes even on incomplete types.
  // We also honor them straight for C++ class types, even as pointees;
  // there's an expressivity gap here.
  if (const auto *tt = t->getAs<TypedefType>()) {
    if (unsigned align = tt->getDecl()->getMaxAlignment()) {
      if (baseInfo)
        *baseInfo = LValueBaseInfo(AlignmentSource::AttributedType);
      return astContext.toCharUnitsFromBits(align);
    }
  }

  bool alignForArray = t->isArrayType();

  // Analyze the base element type, so we don't get confused by incomplete
  // array types.
  t = astContext.getBaseElementType(t);

  if (t->isIncompleteType()) {
    // We could try to replicate the logic from
    // ASTContext::getTypeAlignIfKnown, but nothing uses the alignment if the
    // type is incomplete, so it's impossible to test. We could try to reuse
    // getTypeAlignIfKnown, but that doesn't return the information we need
    // to set baseInfo.  So just ignore the possibility that the alignment is
    // greater than one.
    if (baseInfo)
      *baseInfo = LValueBaseInfo(AlignmentSource::Type);
    return CharUnits::One();
  }

  if (baseInfo)
    *baseInfo = LValueBaseInfo(AlignmentSource::Type);

  CharUnits alignment;
  const CXXRecordDecl *rd = nullptr;
  if (t.getQualifiers().hasUnaligned()) {
    alignment = CharUnits::One();
  } else if (forPointeeType && !alignForArray &&
             (rd = t->getAsCXXRecordDecl())) {
    alignment = getClassPointerAlignment(rd);
  } else {
    alignment = astContext.getTypeAlignInChars(t);
  }

  // Cap to the global maximum type alignment unless the alignment
  // was somehow explicit on the type.
  if (unsigned maxAlign = astContext.getLangOpts().MaxTypeAlign) {
    if (alignment.getQuantity() > maxAlign &&
        !astContext.isAlignmentRequired(t))
      alignment = CharUnits::fromQuantity(maxAlign);
  }
  return alignment;
}

CharUnits
CIRGenModule::getNaturalPointeeTypeAlignment(QualType t,
                                             LValueBaseInfo *baseInfo) {
  return getNaturalTypeAlignment(t->getPointeeType(), baseInfo,
                                 /*forPointeeType=*/true);
}

const TargetCIRGenInfo &CIRGenModule::getTargetCIRGenInfo() {
  if (theTargetCIRGenInfo)
    return *theTargetCIRGenInfo;

  const llvm::Triple &triple = getTarget().getTriple();
  switch (triple.getArch()) {
  default:
    assert(!cir::MissingFeatures::targetCIRGenInfoArch());

    // Currently we just fall through to x86_64.
    [[fallthrough]];

  case llvm::Triple::x86_64: {
    switch (triple.getOS()) {
    default:
      assert(!cir::MissingFeatures::targetCIRGenInfoOS());

      // Currently we just fall through to x86_64.
      [[fallthrough]];

    case llvm::Triple::Linux:
      theTargetCIRGenInfo = createX8664TargetCIRGenInfo(genTypes);
      return *theTargetCIRGenInfo;
    }
  }
  case llvm::Triple::nvptx:
  case llvm::Triple::nvptx64:
    theTargetCIRGenInfo = createNVPTXTargetCIRGenInfo(genTypes);
    return *theTargetCIRGenInfo;
  case llvm::Triple::amdgcn: {
    theTargetCIRGenInfo = createAMDGPUTargetCIRGenInfo(genTypes);
    return *theTargetCIRGenInfo;
  }
  case llvm::Triple::spirv:
  case llvm::Triple::spirv32:
  case llvm::Triple::spirv64:
    theTargetCIRGenInfo = createSPIRVTargetCIRGenInfo(genTypes);
    return *theTargetCIRGenInfo;
  }
}

static mlir::Location getPresumedFileLineColLoc(mlir::Builder &builder,
                                                const SourceManager &sm,
                                                SourceLocation cLoc) {
  PresumedLoc pLoc = sm.getPresumedLoc(cLoc);
  StringRef filename = pLoc.getFilename();
  return mlir::FileLineColLoc::get(builder.getStringAttr(filename),
                                   pLoc.getLine(), pLoc.getColumn());
}

static mlir::Location getMacroAwareLoc(mlir::Builder &builder,
                                       const SourceManager &sm,
                                       const LangOptions &langOpts,
                                       SourceLocation cLoc) {
  mlir::Location loc = getPresumedFileLineColLoc(builder, sm, cLoc);
  if (!cLoc.isMacroID())
    return loc;

  SourceLocation current = cLoc;
  for (unsigned depth = 0; depth < 32 && current.isMacroID(); ++depth) {
    SourceLocation spellingLoc = sm.getImmediateSpellingLoc(current);
    if (spellingLoc.isInvalid())
      spellingLoc = sm.getSpellingLoc(current);
    mlir::Location calleeLoc =
        spellingLoc.isValid()
            ? getPresumedFileLineColLoc(builder, sm, spellingLoc)
            : loc;

    StringRef macroName = Lexer::getImmediateMacroName(current, sm, langOpts);
    if (!macroName.empty())
      calleeLoc =
          mlir::NameLoc::get(builder.getStringAttr(macroName), calleeLoc);
    loc = mlir::CallSiteLoc::get(calleeLoc, loc);

    CharSourceRange expansion = sm.getImmediateExpansionRange(current);
    SourceLocation next = expansion.getBegin();
    if (next.isInvalid() || next == current)
      break;
    current = next;
  }
  return loc;
}

mlir::Location CIRGenModule::getLoc(SourceLocation cLoc) {
  assert(cLoc.isValid() && "expected valid source location");
  const SourceManager &sm = astContext.getSourceManager();
  return getMacroAwareLoc(builder, sm, getLangOpts(), cLoc);
}

mlir::Location CIRGenModule::getLoc(SourceRange cRange) {
  assert(cRange.isValid() && "expected a valid source range");
  mlir::Location begin = getLoc(cRange.getBegin());
  mlir::Location end = getLoc(cRange.getEnd());
  mlir::Attribute metadata;
  return mlir::FusedLoc::get({begin, end}, metadata, builder.getContext());
}

mlir::Operation *
CIRGenModule::getAddrOfGlobal(GlobalDecl gd, ForDefinition_t isForDefinition) {
  const Decl *d = gd.getDecl();

  if (isa<CXXConstructorDecl>(d) || isa<CXXDestructorDecl>(d))
    return getAddrOfCXXStructor(gd, /*FnInfo=*/nullptr, /*FnType=*/nullptr,
                                /*DontDefer=*/false, isForDefinition);

  if (isa<CXXMethodDecl>(d)) {
    const CIRGenFunctionInfo &fi =
        getTypes().arrangeCXXMethodDeclaration(cast<CXXMethodDecl>(d));
    cir::FuncType ty = getTypes().getFunctionType(fi);
    return getAddrOfFunction(gd, ty, /*ForVTable=*/false, /*DontDefer=*/false,
                             isForDefinition);
  }

  if (isa<FunctionDecl>(d)) {
    const CIRGenFunctionInfo &fi = getTypes().arrangeGlobalDeclaration(gd);
    cir::FuncType ty = getTypes().getFunctionType(fi);
    return getAddrOfFunction(gd, ty, /*ForVTable=*/false, /*DontDefer=*/false,
                             isForDefinition);
  }

  return getAddrOfGlobalVar(cast<VarDecl>(d), /*ty=*/nullptr, isForDefinition)
      .getDefiningOp();
}

void CIRGenModule::emitGlobalDecl(const clang::GlobalDecl &d) {
  // We call getAddrOfGlobal with isForDefinition set to ForDefinition in
  // order to get a Value with exactly the type we need, not something that
  // might have been created for another decl with the same mangled name but
  // different type.
  mlir::Operation *op = getAddrOfGlobal(d, ForDefinition);

  // In case of different address spaces, we may still get a cast, even with
  // IsForDefinition equal to ForDefinition. Query mangled names table to get
  // GlobalValue.
  if (!op)
    op = getGlobalValue(getMangledName(d));

  assert(op && "expected a valid global op");

  // Check to see if we've already emitted this. This is necessary for a
  // couple of reasons: first, decls can end up in deferred-decls queue
  // multiple times, and second, decls can end up with definitions in unusual
  // ways (e.g. by an extern inline function acquiring a strong function
  // redefinition). Just ignore those cases.
  // TODO: Not sure what to map this to for MLIR
  mlir::Operation *globalValueOp = op;
  if (auto gv = dyn_cast<cir::GetGlobalOp>(op)) {
    globalValueOp = getGlobalValue(gv.getName());
    assert(globalValueOp && "expected a valid global op");
  }

  if (auto cirGlobalValue =
          dyn_cast<cir::CIRGlobalValueInterface>(globalValueOp)) {
    if (!cirGlobalValue.isDeclaration()) {
      return;
    }
  }

  // If this is OpenMP, check if it is legal to emit this global normally.
  assert(!cir::MissingFeatures::openMP());

  // Otherwise, emit the definition and move on to the next one.
  emitGlobalDefinition(d, op);
}

void CIRGenModule::addDeferredDeclToEmit(GlobalDecl gd) {
  if (selectedDeclRootMode) {
    const bool isRoot = isSelectedDeclRoot(gd);
    if (!isRoot) {
      addSelectedDeclDependency(gd);
      if (!emittingSelectedDeclDependency)
        return;
    }
    // A selected root can also be rediscovered while another selected body is
    // being generated. It must still enter the exact dependency worklist:
    // template specializations and nested lambdas can be materialized only by
    // that reference, after the stable root frontier was discovered.
    if (isRoot && curCGF) {
      addSelectedDeclDependency(gd);
      return;
    }
  }
  deferredDeclsToEmit.emplace_back(gd);
}

void CIRGenModule::emitDeferred() {
  // Emit code for any potentially referenced deferred decls. Since a previously
  // unused static decl may become used during the generation of code for a
  // static function, iterate until no changes are made.

  assert(!cir::MissingFeatures::openMP());

  emitDeferredVTables();
  // Emitting a vtable doesn't directly cause more vtables to
  // become deferred, although it can cause functions to be
  // emitted that then need those vtables.
  assert(deferredVTables.empty());

  assert(!cir::MissingFeatures::cudaSupport());

  // Stop if we're out of both deferred vtables and deferred declarations.
  if (deferredDeclsToEmit.empty())
    return;

  // Grab the list of decls to emit. If emitGlobalDefinition schedules more
  // work, it will not interfere with this.
  std::vector<GlobalDecl> curDeclsToEmit;
  curDeclsToEmit.swap(deferredDeclsToEmit);
  for (const GlobalDecl &d : curDeclsToEmit) {
    bool wasEmittingSelectedDeclDependency = emittingSelectedDeclDependency;
    emittingSelectedDeclDependency = true;
    emitGlobalDecl(d);
    emittingSelectedDeclDependency = wasEmittingSelectedDeclDependency;

    // If we found out that we need to emit more decls, do that recursively.
    // This has the advantage that the decls are emitted in the DFS and related
    // ones are close together, which is convenient for testing.
    if (!deferredVTables.empty() || !deferredDeclsToEmit.empty()) {
      emitDeferred();
      assert(deferredVTables.empty() && deferredDeclsToEmit.empty());
    }
  }
}

template <typename AttrT> static bool hasImplicitAttr(const ValueDecl *decl) {
  if (!decl)
    return false;
  if (auto *attr = decl->getAttr<AttrT>())
    return attr->isImplicit();
  return decl->isImplicit();
}

// TODO(cir): This should be shared with OG Codegen.
bool CIRGenModule::shouldEmitCUDAGlobalVar(const VarDecl *global) const {
  assert(langOpts.CUDA && "Should not be called by non-CUDA languages");
  // We need to emit host-side 'shadows' for all global
  // device-side variables because the CUDA runtime needs their
  // size and host-side address in order to provide access to
  // their device-side incarnations.
  return !langOpts.CUDAIsDevice || global->hasAttr<CUDADeviceAttr>() ||
         global->hasAttr<CUDAConstantAttr>() ||
         global->hasAttr<CUDASharedAttr>() ||
         global->getType()->isCUDADeviceBuiltinSurfaceType() ||
         global->getType()->isCUDADeviceBuiltinTextureType();
}

void CIRGenModule::printPostfixForExternalizedDecl(llvm::raw_ostream &os,
                                                   const Decl *d) {
  // ptxas does not allow '.' in symbol names. On the other hand, HIP prefers
  // postfix beginning with '.' since the symbol name can be demangled.
  if (langOpts.HIP)
    os << (isa<VarDecl>(d) ? ".static." : ".intern.");
  else
    os << (isa<VarDecl>(d) ? "__static__" : "__intern__");

  // If the CUID is not specified we try to generate a unique postfix.
  if (getLangOpts().CUID.empty()) {
    // TODO: Once we add 'PreprocessorOpts' into CIRGenModule this part can be
    // brought in from OG.
    errorNYI(d->getSourceRange(),
             "printPostfixForExternalizedDecl: CUID is not specified");
  } else {
    os << getASTContext().getCUIDHash();
  }
}

void CIRGenModule::emitGlobal(clang::GlobalDecl gd) {
  llvm::SaveAndRestore<const Decl *> diagnosticOwner(currentDiagnosticDecl,
                                                     gd.getDecl());
  if (const auto *cd = dyn_cast<clang::OpenACCConstructDecl>(gd.getDecl())) {
    emitGlobalOpenACCDecl(cd);
    return;
  }

  if (selectedDeclRootMode && !emittingSelectedDeclDependency &&
      !isSelectedDeclRoot(gd) && !isSelectedDeclDependency(gd))
    return;

  const auto *global = cast<ValueDecl>(gd.getDecl());

  // Weak references don't produce any output by themselves.
  if (global->hasAttr<WeakRefAttr>())
    return;

  // If this is an alias definition (which otherwise looks like a declaration)
  // emit it now.
  if (global->hasAttr<AliasAttr>()) {
    // Classic codegen calls shouldSkipAliasEmission here to skip alias
    // emission for OpenMP target device and CUDA configurations.
    assert(!cir::MissingFeatures::shouldSkipAliasEmission());
    auto existing = mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(
        getGlobalValue(getMangledName(gd)));
    const bool hadDefinition = existing && existing.isDefinition();
    emitAliasDefinition(gd);
    if (!hadDefinition)
      noteSelectedDeclRootDefinition(gd);
    return;
  }

  // If this is CUDA, be selective about which declarations we emit.
  // Non-constexpr non-lambda implicit host device functions are not emitted
  // unless they are used on device side.
  if (langOpts.CUDA) {
    assert((isa<FunctionDecl>(global) || isa<VarDecl>(global)) &&
           "Expected Variable or Function");
    if (const auto *varDecl = dyn_cast<VarDecl>(global)) {
      if (!shouldEmitCUDAGlobalVar(varDecl))
        return;
      // TODO(cir): This should be shared with OG Codegen.
    } else if (langOpts.CUDAIsDevice) {
      const auto *functionDecl = dyn_cast<FunctionDecl>(global);
      if ((!global->hasAttr<CUDADeviceAttr>() ||
           (langOpts.OffloadImplicitHostDeviceTemplates &&
            hasImplicitAttr<CUDAHostAttr>(functionDecl) &&
            hasImplicitAttr<CUDADeviceAttr>(functionDecl) &&
            !functionDecl->isConstexpr() &&
            !isLambdaCallOperator(functionDecl) &&
            !getASTContext().CUDAImplicitHostDeviceFunUsedByDevice.count(
                functionDecl))) &&
          !global->hasAttr<CUDAGlobalAttr>() &&
          !(langOpts.HIPStdPar && isa<FunctionDecl>(global) &&
            !global->hasAttr<CUDAHostAttr>()))
        return;
      // Device-only functions are the only things we skip.
    } else if (!global->hasAttr<CUDAHostAttr>() &&
               global->hasAttr<CUDADeviceAttr>())
      return;
  }

  if (langOpts.OpenMP) {
    // If this is OpenMP, check if it is legal to emit this global normally.
    if (openMPRuntime && openMPRuntime->emitTargetGlobal(gd))
      return;
    if (auto *drd = dyn_cast<OMPDeclareReductionDecl>(global)) {
      if (mustBeEmitted(global))
        emitOMPDeclareReduction(drd);
      return;
    }
    if (auto *dmd = dyn_cast<OMPDeclareMapperDecl>(global)) {
      if (mustBeEmitted(global))
        emitOMPDeclareMapper(dmd);
      return;
    }
  }

  if (const auto *fd = dyn_cast<FunctionDecl>(global)) {
    // Update deferred annotations with the latest declaration if the function
    // was already used or defined.
    if (fd->hasAttr<AnnotateAttr>()) {
      StringRef mangledName = getMangledName(gd);
      if (getGlobalValue(mangledName))
        deferredAnnotations[mangledName] = fd;
    }
    if (!fd->doesThisDeclarationHaveABody()) {
      if (!fd->doesDeclarationForceExternallyVisibleDefinition() &&
          (!fd->isMultiVersion() || !getTarget().getTriple().isAArch64()))
        return;

      const CIRGenFunctionInfo &fi = getTypes().arrangeGlobalDeclaration(gd);
      cir::FuncType ty = getTypes().getFunctionType(fi);
      getAddrOfFunction(gd, ty, /*ForVTable=*/false, /*DontDefer=*/false);
      return;
    }
  } else {
    const auto *vd = cast<VarDecl>(global);
    if (isSelectedVariableTemplatePattern(vd)) {
      // A selected dependent variable-template pattern is not linker-owned
      // storage, but a non-dependent constant initializer is still an exact,
      // typed AST definition fact. Preserve that fact as an
      // available_externally CIR definition.
      if (isSelectedConstantVariableTemplatePattern(vd))
        emitGlobalDefinition(gd);
      return;
    }
    assert(vd->isFileVarDecl() && "Cannot emit local var decl as global.");
    // A selected pre-C++17-style in-class static const member owns an exact
    // initializer fact but not a strong storage definition. Materialize that
    // fact directly as an available_externally CIR definition; the ordinary
    // declaration-only path below must remain unchanged.
    if (isSelectedStaticDataMemberDeclaration(vd)) {
      emitGlobalDefinition(gd);
      return;
    }
    if (vd->isThisDeclarationADefinition() != VarDecl::Definition &&
        !astContext.isMSStaticDataMemberInlineDefinition(vd)) {
      assert(!cir::MissingFeatures::openMP());
      // If this declaration may have caused an inline variable definition to
      // change linkage, make sure that it's emitted.
      if (astContext.getInlineVariableDefinitionKind(vd) ==
          ASTContext::InlineVariableDefinitionKind::Strong)
        getAddrOfGlobalVar(vd);
      // Otherwise, we can ignore this declaration. The variable will be emitted
      // on its first use.
      return;
    }
  }

  // Defer code generation to first use when possible, e.g. if this is an inline
  // function. If the global must always be emitted, do it eagerly if possible
  // to benefit from cache locality. Deferring code generation is necessary to
  // avoid adding initializers to external declarations.
  if (mustBeEmitted(global) && mayBeEmittedEagerly(global)) {
    // Emit the definition if it can't be deferred.
    emitGlobalDefinition(gd);
    return;
  }

  // If we're deferring emission of a C++ variable with an initializer, remember
  // the order in which it appeared on the file.
  assert(!cir::MissingFeatures::deferredCXXGlobalInit());

  llvm::StringRef mangledName = getMangledName(gd);
  if (getGlobalValue(mangledName) != nullptr) {
    // The value has already been used and should therefore be emitted.
    addDeferredDeclToEmit(gd);
  } else if (mustBeEmitted(global)) {
    // The value must be emitted, but cannot be emitted eagerly.
    assert(!mayBeEmittedEagerly(global));
    addDeferredDeclToEmit(gd);
  } else {
    // Otherwise, remember that we saw a deferred decl with this name. The first
    // use of the mangled name will cause it to move into deferredDeclsToEmit.
    deferredDecls[mangledName] = gd;
  }
}

cir::FuncOp CIRGenModule::emitGlobalFunctionDefinition(clang::GlobalDecl gd,
                                                       mlir::Operation *op) {
  auto const *funcDecl = cast<FunctionDecl>(gd.getDecl());
  const CIRGenFunctionInfo &fi = getTypes().arrangeGlobalDeclaration(gd);
  cir::FuncType funcType = getTypes().getFunctionType(fi);
  cir::FuncOp funcOp = dyn_cast_if_present<cir::FuncOp>(op);
  if (!funcOp || funcOp.getFunctionType() != funcType) {
    funcOp = getAddrOfFunction(gd, funcType, /*ForVTable=*/false,
                               /*DontDefer=*/true, ForDefinition);
  }

  // Already emitted.
  if (!funcOp.isDeclaration())
    return funcOp;
  // Inline builtin emission leaves the public spelling as a declaration and
  // owns the body under a distinct .inline symbol. Check the requested symbol
  // before mutating its definition linkage on a repeated emission request.
  if (!emittedFunctionBodySymbols.insert(funcOp.getSymName()).second)
    return funcOp;
  llvm::TimeTraceScope timeScope("CIRGen Function", [&]() {
    std::string name;
    llvm::raw_string_ostream os(name);
    funcDecl->getNameForDiagnostic(os, astContext.getPrintingPolicy(),
                                   /*Qualified=*/true);
    return name;
  });

  setFunctionLinkage(gd, funcOp);
  setGVProperties(funcOp, funcDecl);
  assert(!cir::MissingFeatures::opFuncMaybeHandleStaticInExternC());
  maybeSetTrivialComdat(*funcDecl, funcOp);
  assert(!cir::MissingFeatures::setLLVMFunctionFEnvAttributes());

  CIRGenFunction cgf(*this, builder);
  CIRGenFunction *previousCGF = curCGF;
  curCGF = &cgf;
  {
    mlir::OpBuilder::InsertionGuard guard(builder);
    cir::FuncOp generatedFuncOp = cgf.generateCode(gd, funcOp, funcType);
    if (generatedFuncOp) {
      // Inline builtin definitions are emitted under a distinct internal
      // symbol. Keep their external fallback declaration private, and apply
      // definition attributes to the function that actually owns the body.
      if (generatedFuncOp == funcOp)
        setFunctionLinkage(gd, generatedFuncOp);
      funcOp = generatedFuncOp;
    }
  }
  curCGF = previousCGF;
  // A definition can replace a declaration created from an earlier
  // redeclaration, and inline builtins move the body to a distinct operation.
  // Attach declaration and specialization identity to the operation that
  // actually owns the body.
  setCIRFunctionAttributes(gd, fi, funcOp, /*isThunk=*/false);

  setNonAliasAttributes(gd, funcOp);
  setCIRFunctionAttributesForDefinition(funcDecl, funcOp);

  auto getPriority = [this](const auto *attr) -> int {
    Expr *e = attr->getPriority();
    if (e)
      return e->EvaluateKnownConstInt(this->getASTContext()).getExtValue();
    return attr->DefaultPriority;
  };

  if (const ConstructorAttr *ca = funcDecl->getAttr<ConstructorAttr>())
    addGlobalCtor(funcOp, getPriority(ca));
  if (const DestructorAttr *da = funcDecl->getAttr<DestructorAttr>())
    addGlobalDtor(funcOp, getPriority(da));

  if (funcDecl->getAttr<AnnotateAttr>())
    deferredAnnotations[getMangledName(gd)] = funcDecl;

  if (getLangOpts().OpenMP && funcDecl->hasAttr<OMPDeclareTargetDeclAttr>())
    getOpenMPRuntime().emitDeclareTargetFunction(funcDecl, funcOp);

  return funcOp;
}

/// Track functions to be called before main() runs.
void CIRGenModule::addGlobalCtor(cir::FuncOp ctor,
                                 std::optional<int> priority) {
  assert(!cir::MissingFeatures::globalCtorLexOrder());
  assert(!cir::MissingFeatures::globalCtorAssociatedData());

  // Traditional LLVM codegen directly adds the function to the list of global
  // ctors. In CIR we just add a global_ctor attribute to the function. The
  // global list is created in LoweringPrepare.
  //
  // FIXME(from traditional LLVM): Type coercion of void()* types.
  ctor.setGlobalCtorPriority(priority);
}

/// Add a function to the list that will be called when the module is unloaded.
void CIRGenModule::addGlobalDtor(cir::FuncOp dtor,
                                 std::optional<int> priority) {
  if (codeGenOpts.RegisterGlobalDtorsWithAtExit &&
      (!getASTContext().getTargetInfo().getTriple().isOSAIX()))
    errorNYI(dtor.getLoc(), "registerGlobalDtorsWithAtExit");

  // FIXME(from traditional LLVM): Type coercion of void()* types.
  dtor.setGlobalDtorPriority(priority);
}

void CIRGenModule::handleCXXStaticMemberVarInstantiation(VarDecl *vd) {
  VarDecl::DefinitionKind dk = vd->isThisDeclarationADefinition();
  if (dk == VarDecl::Definition && vd->hasAttr<DLLImportAttr>())
    return;

  TemplateSpecializationKind tsk = vd->getTemplateSpecializationKind();
  // If we have a definition, this might be a deferred decl. If the
  // instantiation is explicit, make sure we emit it at the end.
  if (vd->getDefinition() && tsk == TSK_ExplicitInstantiationDefinition)
    getAddrOfGlobalVar(vd);

  emitTopLevelDecl(vd);
}

mlir::Operation *CIRGenModule::getGlobalValue(StringRef name) {
  auto it = symbolLookupCache.find(name);
  return it != symbolLookupCache.end() ? it->second : nullptr;
}

cir::GlobalOp
CIRGenModule::createGlobalOp(mlir::Location loc, StringRef name, mlir::Type t,
                             bool isConstant,
                             mlir::ptr::MemorySpaceAttrInterface addrSpace,
                             mlir::Operation *insertPoint) {
  cir::GlobalOp g;
  CIRGenBuilderTy &builder = getBuilder();

  {
    mlir::OpBuilder::InsertionGuard guard(builder);

    // If an insertion point is provided, we're replacing an existing global,
    // otherwise, create the new global immediately after the last gloabl we
    // emitted.
    if (insertPoint) {
      builder.setInsertionPoint(insertPoint);
    } else {
      // Group global operations together at the top of the module.
      if (lastGlobalOp)
        builder.setInsertionPointAfter(lastGlobalOp);
      else
        builder.setInsertionPointToStart(getModule().getBody());
    }

    g = cir::GlobalOp::create(builder, loc, name, t, isConstant, addrSpace);
    if (!insertPoint)
      lastGlobalOp = g;

    // Default to private until we can judge based on the initializer,
    // since MLIR doesn't allow public declarations.
    mlir::SymbolTable::setSymbolVisibility(
        g, mlir::SymbolTable::Visibility::Private);
  }
  symbolLookupCache[g.getSymNameAttr()] = g;
  return g;
}

void CIRGenModule::setCommonAttributes(GlobalDecl gd, mlir::Operation *gv) {
  const Decl *d = gd.getDecl();
  if (isa_and_nonnull<NamedDecl>(d))
    setGVProperties(gv, dyn_cast<NamedDecl>(d));
  assert(!cir::MissingFeatures::defaultVisibility());

  if (auto gvi = mlir::dyn_cast<cir::CIRGlobalValueInterface>(gv)) {
    if (d && d->hasAttr<UsedAttr>())
      addUsedOrCompilerUsedGlobal(gvi);

    if (const auto *vd = dyn_cast_if_present<VarDecl>(d);
        vd && ((codeGenOpts.KeepPersistentStorageVariables &&
                (vd->getStorageDuration() == SD_Static ||
                 vd->getStorageDuration() == SD_Thread)) ||
               (codeGenOpts.KeepStaticConsts &&
                vd->getStorageDuration() == SD_Static &&
                vd->getType().isConstQualified())))
      addUsedOrCompilerUsedGlobal(gvi);
  }
}

/// Get the feature delta from the default feature map for the given target CPU.
static std::vector<std::string>
getFeatureDeltaFromDefault(const CIRGenModule &cgm, llvm::StringRef targetCPU,
                           llvm::StringMap<bool> &featureMap) {
  llvm::StringMap<bool> defaultFeatureMap;
  cgm.getTarget().initFeatureMap(
      defaultFeatureMap, cgm.getASTContext().getDiagnostics(), targetCPU, {});

  std::vector<std::string> delta;
  for (const auto &[k, v] : featureMap) {
    auto defaultIt = defaultFeatureMap.find(k);
    if (defaultIt == defaultFeatureMap.end() || defaultIt->getValue() != v)
      delta.push_back((v ? "+" : "-") + k.str());
  }

  return delta;
}

bool CIRGenModule::getCPUAndFeaturesAttributes(
    GlobalDecl gd, llvm::StringMap<std::string> &attrs,
    bool setTargetFeatures) {
  // Add target-cpu and target-features attributes to functions. If
  // we have a decl for the function and it has a target attribute then
  // parse that and add it to the feature set.
  llvm::StringRef targetCPU = getTarget().getTargetOpts().CPU;
  llvm::StringRef tuneCPU = getTarget().getTargetOpts().TuneCPU;
  std::vector<std::string> features;
  // `fd` may be null when emitting attributes for globals that don't have a
  // FunctionDecl. The AMDGPU branch below handles
  // the null case via initFeatureMap.
  const auto *fd = dyn_cast_or_null<FunctionDecl>(gd.getDecl());
  fd = fd ? fd->getMostRecentDecl() : fd;
  const auto *td = fd ? fd->getAttr<TargetAttr>() : nullptr;
  const auto *tv = fd ? fd->getAttr<TargetVersionAttr>() : nullptr;
  assert((!td || !tv) && "both target_version and target specified");
  const auto *sd = fd ? fd->getAttr<CPUSpecificAttr>() : nullptr;
  const auto *tc = fd ? fd->getAttr<TargetClonesAttr>() : nullptr;
  bool addedAttr = false;
  if (td || tv || sd || tc) {
    llvm::StringMap<bool> featureMap;
    astContext.getFunctionFeatureMap(featureMap, gd);

    // Now add the target-cpu and target-features to the function.
    // While we populated the feature map above, we still need to
    // get and parse the target/target_clones attribute so we can
    // get the cpu for the function.
    llvm::StringRef featureStr = td ? td->getFeaturesStr() : llvm::StringRef();
    if (tc && (getTriple().isOSAIX() || getTriple().isX86()))
      featureStr = tc->getFeatureStr(gd.getMultiVersionIndex());
    if (!featureStr.empty()) {
      clang::ParsedTargetAttr parsedAttr =
          getTarget().parseTargetAttr(featureStr);
      if (!parsedAttr.CPU.empty() &&
          getTarget().isValidCPUName(parsedAttr.CPU)) {
        targetCPU = parsedAttr.CPU;
        tuneCPU = ""; // Clear the tune CPU.
      }
      if (!parsedAttr.Tune.empty() &&
          getTarget().isValidCPUName(parsedAttr.Tune))
        tuneCPU = parsedAttr.Tune;
    }

    if (sd) {
      // Apply the given CPU name as the 'tune-cpu' so that the optimizer can
      // favor this processor.
      tuneCPU = sd->getCPUName(gd.getMultiVersionIndex())->getName();
    }

    // For AMDGPU, only emit delta features (features that differ from the
    // target CPU's defaults). Other targets might want to follow a similar
    // pattern.
    if (getTarget().getTriple().isAMDGPU()) {
      features = getFeatureDeltaFromDefault(*this, targetCPU, featureMap);
    } else {
      // Produce the canonical string for this set of features.
      features.reserve(features.size() + featureMap.size());
      for (const auto &entry : featureMap)
        features.push_back((entry.getValue() ? "+" : "-") +
                           entry.getKey().str());
    }
  } else {
    // Just add the existing target cpu and target features to the function.
    if (setTargetFeatures && getTarget().getTriple().isAMDGPU()) {
      llvm::StringMap<bool> featureMap;
      if (fd)
        astContext.getFunctionFeatureMap(featureMap, gd);
      else
        getTarget().initFeatureMap(featureMap, astContext.getDiagnostics(),
                                   targetCPU,
                                   getTarget().getTargetOpts().Features);
      features = getFeatureDeltaFromDefault(*this, targetCPU, featureMap);
    } else {
      features = getTarget().getTargetOpts().Features;
    }
  }

  if (!targetCPU.empty()) {
    attrs["cir.target-cpu"] = targetCPU.str();
    addedAttr = true;
  }
  if (!tuneCPU.empty()) {
    attrs["cir.tune-cpu"] = tuneCPU.str();
    addedAttr = true;
  }
  if (!features.empty() && setTargetFeatures) {
    llvm::erase_if(features, [&](const std::string &f) {
      assert(!f.empty() && (f[0] == '+' || f[0] == '-') &&
             "feature string must start with '+' or '-'");
      return getTarget().isReadOnlyFeature(f.substr(1));
    });
    llvm::sort(features);
    attrs["cir.target-features"] = llvm::join(features, ",");
    addedAttr = true;
  }
  // TODO(cir): add metadata for AArch64 Function Multi Versioning.
  assert(!cir::MissingFeatures::opFuncMultiVersioning());
  return addedAttr;
}

void CIRGenModule::setNonAliasAttributes(GlobalDecl gd, mlir::Operation *op) {
  setCommonAttributes(gd, op);

  const Decl *d = gd.getDecl();
  if (d) {
    if (auto gvi = mlir::dyn_cast<cir::CIRGlobalValueInterface>(op)) {
      if (const auto *sa = d->getAttr<SectionAttr>())
        gvi.setSection(builder.getStringAttr(sa->getName()));
      if (d->hasAttr<RetainAttr>())
        addUsedGlobal(gvi);

      if (auto func = dyn_cast<cir::FuncOp>(op)) {
        llvm::StringMap<std::string> attrs;
        if (getCPUAndFeaturesAttributes(gd, attrs)) {
          // TODO(cir): Classic codegen removes the existing target-cpu,
          // target-features, tune-cpu and fmv-features attributes here
          // before adding the new ones.
          for (const auto &[key, val] : attrs)
            func->setAttr(key, builder.getStringAttr(val));
        }
      }
    }
  }

  assert(!cir::MissingFeatures::opGlobalPragmaClangSection());
  getTargetCIRGenInfo().setTargetAttributes(gd.getDecl(), op, *this);
}

std::optional<cir::SourceLanguage> CIRGenModule::getCIRSourceLanguage() const {
  using ClangStd = clang::LangStandard;
  using CIRLang = cir::SourceLanguage;
  auto opts = getLangOpts();

  if (opts.CPlusPlus)
    return CIRLang::CXX;
  if (opts.C99 || opts.C11 || opts.C17 || opts.C23 || opts.C2y ||
      opts.LangStd == ClangStd::lang_c89 ||
      opts.LangStd == ClangStd::lang_gnu89)
    return CIRLang::C;

  // TODO(cir): support remaining source languages.
  assert(!cir::MissingFeatures::sourceLanguageCases());
  errorNYI("CIR does not yet support the given source language");
  return std::nullopt;
}

LangAS CIRGenModule::getGlobalVarAddressSpace(const VarDecl *d) {
  if (langOpts.OpenCL) {
    LangAS as = d ? d->getType().getAddressSpace() : LangAS::opencl_global;
    assert(as == LangAS::opencl_global || as == LangAS::opencl_global_device ||
           as == LangAS::opencl_global_host || as == LangAS::opencl_constant ||
           as == LangAS::opencl_local || as >= LangAS::FirstTargetAddressSpace);
    return as;
  }

  if (langOpts.SYCLIsDevice &&
      (!d || d->getType().getAddressSpace() == LangAS::Default))
    errorNYI("SYCL global address space");

  if (langOpts.CUDA && langOpts.CUDAIsDevice) {
    if (d) {
      if (d->hasAttr<CUDAConstantAttr>())
        return LangAS::cuda_constant;
      if (d->hasAttr<CUDASharedAttr>())
        return LangAS::cuda_shared;
      if (d->hasAttr<CUDADeviceAttr>())
        return LangAS::cuda_device;
      if (d->getType().isConstQualified())
        return LangAS::cuda_constant;
    }
    return LangAS::cuda_device;
  }

  if (langOpts.OpenMP)
    errorNYI("OpenMP global address space");

  return getTargetCIRGenInfo().getGlobalVarAddressSpace(*this, d);
}

static void setLinkageForGV(cir::GlobalOp &gv, const NamedDecl *nd) {
  // Set linkage and visibility in case we never see a definition.
  LinkageInfo lv = nd->getLinkageAndVisibility();
  // Don't set internal linkage on declarations.
  // "extern_weak" is overloaded in LLVM; we probably should have
  // separate linkage types for this.
  if (isExternallyVisible(lv.getLinkage()) &&
      (nd->hasAttr<WeakAttr>() || nd->isWeakImported()))
    gv.setLinkage(cir::GlobalLinkageKind::ExternalWeakLinkage);
}

static void setLinkageForFunction(CIRGenModule &cgm, cir::FuncOp &func,
                                  const NamedDecl *nd) {
  // Mirrors CodeGenModule::setLinkageForGV for function declarations.
  LinkageInfo lv = nd->getLinkageAndVisibility();
  if (isExternallyVisible(lv.getLinkage()) &&
      (nd->hasAttr<WeakAttr>() || nd->isWeakImported())) {
    auto linkage = cir::GlobalLinkageKind::ExternalWeakLinkage;
    func.setLinkage(linkage);
    func.setLinkageAttr(
        cir::GlobalLinkageKindAttr::get(&cgm.getMLIRContext(), linkage));
    // Declarations must keep 'private' MLIR visibility; only update for defs.
    if (!func.isDeclaration())
      mlir::SymbolTable::setSymbolVisibility(
          func, cgm.getMLIRVisibilityFromCIRLinkage(linkage));
  }
}

static llvm::SmallVector<int64_t> indexesOfArrayAttr(mlir::ArrayAttr indexes) {
  llvm::SmallVector<int64_t> inds;
  for (mlir::Attribute i : indexes) {
    auto ind = mlir::cast<mlir::IntegerAttr>(i);
    inds.push_back(ind.getValue().getSExtValue());
  }
  return inds;
}

static bool isViewOnGlobal(cir::GlobalOp glob, cir::GlobalViewAttr view) {
  return view.getSymbol().getValue() == glob.getSymName();
}

static cir::GlobalViewAttr createNewGlobalView(CIRGenModule &cgm,
                                               cir::GlobalOp newGlob,
                                               cir::GlobalViewAttr attr,
                                               mlir::Type oldTy) {
  // If the attribute does not require indexes or it is not a global view on
  // the global we're replacing, keep the original attribute.
  if (!attr.getIndices() || !isViewOnGlobal(newGlob, attr))
    return attr;

  llvm::SmallVector<int64_t> oldInds = indexesOfArrayAttr(attr.getIndices());
  llvm::SmallVector<int64_t> newInds;
  CIRGenBuilderTy &bld = cgm.getBuilder();
  const cir::CIRDataLayout &layout = cgm.getDataLayout();
  mlir::Type newTy = newGlob.getSymType();

  uint64_t offset =
      bld.computeOffsetFromGlobalViewIndices(layout, oldTy, oldInds);
  bld.computeGlobalViewIndicesFromFlatOffset(offset, newTy, layout, newInds);
  // A replacement changes the physical storage type of the global, not the
  // type of the object (or subobject) denoted by an existing view.  Keep the
  // view's result type while remapping its indices to the new storage layout.
  auto symbol = mlir::FlatSymbolRefAttr::get(newGlob.getSymNameAttr());
  return cir::GlobalViewAttr::get(attr.getType(), symbol,
                                  bld.getI64ArrayAttr(newInds));
}

static mlir::Attribute getNewInitValue(CIRGenModule &cgm, cir::GlobalOp newGlob,
                                       mlir::Type oldTy,
                                       mlir::Attribute oldInit) {
  if (auto oldView = mlir::dyn_cast<cir::GlobalViewAttr>(oldInit))
    return createNewGlobalView(cgm, newGlob, oldView, oldTy);

  auto getNewInitElements =
      [&](mlir::ArrayAttr oldElements) -> mlir::ArrayAttr {
    llvm::SmallVector<mlir::Attribute> newElements;
    for (mlir::Attribute elt : oldElements) {
      if (auto view = mlir::dyn_cast<cir::GlobalViewAttr>(elt))
        newElements.push_back(createNewGlobalView(cgm, newGlob, view, oldTy));
      else if (mlir::isa<cir::ConstArrayAttr, cir::ConstRecordAttr>(elt))
        newElements.push_back(getNewInitValue(cgm, newGlob, oldTy, elt));
      else
        newElements.push_back(elt);
    }
    return mlir::ArrayAttr::get(cgm.getBuilder().getContext(), newElements);
  };

  if (auto oldArray = mlir::dyn_cast<cir::ConstArrayAttr>(oldInit)) {
    // String-backed arrays cannot contain a GlobalViewAttr and therefore need
    // no recursive rewrite.
    auto oldElements = mlir::dyn_cast<mlir::ArrayAttr>(oldArray.getElts());
    if (!oldElements)
      return oldArray;
    mlir::ArrayAttr newElements = getNewInitElements(oldElements);
    return cgm.getBuilder().getConstArray(
        newElements, mlir::cast<cir::ArrayType>(oldArray.getType()));
  }
  if (auto oldRecord = mlir::dyn_cast<cir::ConstRecordAttr>(oldInit)) {
    mlir::ArrayAttr newMembers = getNewInitElements(oldRecord.getMembers());
    auto recordTy = mlir::cast<cir::RecordType>(oldRecord.getType());
    return cgm.getBuilder().getConstRecordOrZeroAttr(
        newMembers, recordTy.getPacked(), recordTy.getPadded(), recordTy);
  }

  // This may be unreachable in practice, but keep it as errorNYI while CIR
  // is still under development.
  cgm.errorNYI("Unhandled type in getNewInitValue");
  return {};
}

// We want to replace a global value, but because of CIR's typed pointers,
// we need to update the existing uses to reflect the new type, not just replace
// them directly.
void CIRGenModule::replaceGlobal(cir::GlobalOp oldGV, cir::GlobalOp newGV) {
  assert(oldGV.getSymName() == newGV.getSymName() && "symbol names must match");

  mlir::Type oldTy = oldGV.getSymType();
  mlir::Type newTy = newGV.getSymType();

  assert(!cir::MissingFeatures::addressSpace());

  // If the type didn't change, why are we here?
  assert(oldTy != newTy && "expected type change in replaceGlobal");

  // Visit all uses and add handling to fix up the types.
  std::optional<mlir::SymbolTable::UseRange> oldSymUses =
      oldGV.getSymbolUses(theModule);
  for (mlir::SymbolTable::SymbolUse use : *oldSymUses) {
    mlir::Operation *userOp = use.getUser();
    assert(
        (mlir::isa<cir::GetGlobalOp, cir::GlobalOp, cir::ConstantOp>(userOp)) &&
        "Unexpected user for global op");

    if (auto getGlobalOp = dyn_cast<cir::GetGlobalOp>(use.getUser())) {
      mlir::Value useOpResultValue = getGlobalOp.getAddr();
      useOpResultValue.setType(cir::PointerType::get(newTy));

      mlir::OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPointAfter(getGlobalOp);
      mlir::Type ptrTy = builder.getPointerTo(oldTy);
      mlir::Value cast =
          builder.createBitcast(getGlobalOp->getLoc(), useOpResultValue, ptrTy);
      useOpResultValue.replaceAllUsesExcept(cast, cast.getDefiningOp());
    } else if (auto glob = dyn_cast<cir::GlobalOp>(userOp)) {
      if (auto init = glob.getInitialValue()) {
        mlir::Attribute nw = getNewInitValue(*this, newGV, oldTy, init.value());
        glob.setInitialValueAttr(nw);
      }
    } else if (auto c = dyn_cast<cir::ConstantOp>(userOp)) {
      mlir::Attribute init = getNewInitValue(*this, newGV, oldTy, c.getValue());
      auto typedAttr = mlir::cast<mlir::TypedAttr>(init);
      mlir::OpBuilder::InsertionGuard guard(builder);
      builder.setInsertionPointAfter(c);
      auto newUser = cir::ConstantOp::create(builder, c.getLoc(), typedAttr);
      c.replaceAllUsesWith(newUser.getOperation());
      c.erase();
    }
  }

  // If the old global is being tracked as the most-recently-created global,
  // update it so that subsequent globals are not inserted after a (now
  // erased) operation, which would leave them detached from the module.
  if (lastGlobalOp == oldGV)
    lastGlobalOp = newGV;
  if (getLangOpts().CUDA)
    getCUDARuntime().handleGlobalReplace(oldGV, newGV);
  eraseGlobalSymbol(oldGV);
  oldGV.erase();
}

/// If the specified mangled name is not in the module,
/// create and return an mlir GlobalOp with the specified type (TODO(cir):
/// address space).
///
/// TODO(cir):
/// 1. If there is something in the module with the specified name, return
/// it potentially bitcasted to the right type.
///
/// 2. If \p d is non-null, it specifies a decl that correspond to this.  This
/// is used to set the attributes on the global when it is first created.
///
/// 3. If \p isForDefinition is true, it is guaranteed that an actual global
/// with type \p ty will be returned, not conversion of a variable with the same
/// mangled name but some other type.
cir::GlobalOp
CIRGenModule::getOrCreateCIRGlobal(StringRef mangledName, mlir::Type ty,
                                   LangAS langAS, const VarDecl *d,
                                   ForDefinition_t isForDefinition) {

  // Lookup the entry, lazily creating it if necessary.
  cir::GlobalOp entry;
  if (mlir::Operation *v = getGlobalValue(mangledName)) {
    if (!isa<cir::GlobalOp>(v))
      errorNYI(d->getSourceRange(),
               "getOrCreateCIRGlobal: global with non-GlobalOp type");
    entry = cast<cir::GlobalOp>(v);
  }

  if (entry) {
    mlir::ptr::MemorySpaceAttrInterface entryCIRAS = entry.getAddrSpaceAttr();
    assert(!cir::MissingFeatures::opGlobalWeakRef());

    assert(!cir::MissingFeatures::setDLLStorageClass());
    assert(!cir::MissingFeatures::openMP());

    if (entry.getSymType() == ty &&
        cir::isMatchingAddressSpace(entryCIRAS, langAS)) {
      setObjectArrayAllocationMetadata(entry.getOperation(), d->getType());
      return entry;
    }

    // If there are two attempts to define the same mangled name, issue an
    // error.
    //
    // TODO(cir): look at mlir::GlobalValue::isDeclaration for all aspects of
    // recognizing the global as a declaration, for now only check if
    // initializer is present.
    if (isForDefinition && !entry.isDeclaration()) {
      errorNYI(d->getSourceRange(),
               "getOrCreateCIRGlobal: global with conflicting type");
    }

    // Address space check removed because it is unnecessary because CIR records
    // address space info in types.

    // (If global is requested for a definition, we always need to create a new
    // global, not just return a bitcast.)
    if (!isForDefinition)
      return entry;
  }

  mlir::Location loc = getLoc(d->getSourceRange());

  // Calculate constant storage flag before creating the global. This was moved
  // from after the global creation to ensure the constant flag is set correctly
  // at creation time, matching the logic used in emitCXXGlobalVarDeclInit.
  bool isConstant = false;
  // An incomplete array type is safe here when its base element type is
  // complete. It is the definition-only C++ record queries performed by
  // isConstantStorage that must be avoided.
  if (d && !astContext.getBaseElementType(d->getType())->isIncompleteType()) {
    bool needsDtor =
        d->needsDestruction(astContext) == QualType::DK_cxx_destructor;
    isConstant = d->getType().isConstantStorage(
        astContext, /*ExcludeCtor=*/true, /*ExcludeDtor=*/!needsDtor);
  }

  mlir::ptr::MemorySpaceAttrInterface declCIRAS =
      cir::toCIRAddressSpaceAttr(getMLIRContext(), getGlobalVarAddressSpace(d));

  // mlir::SymbolTable::Visibility::Public is the default, no need to explicitly
  // mark it as such.
  cir::GlobalOp gv = createGlobalOp(loc, mangledName, ty, isConstant, declCIRAS,
                                    /*insertPoint=*/entry.getOperation());
  setObjectArrayAllocationMetadata(gv.getOperation(), d->getType());

  // If we already created a global with the same mangled name (but different
  // type) before, remove it from its parent.
  if (entry)
    replaceGlobal(entry, gv);

  // This is the first use or definition of a mangled name.  If there is a
  // deferred decl with this name, remember that we need to emit it at the end
  // of the file.
  auto ddi = deferredDecls.find(mangledName);
  if (ddi != deferredDecls.end()) {
    // Move the potentially referenced deferred decl to the DeferredDeclsToEmit
    // list, and remove it from DeferredDecls (since we don't need it anymore).
    addDeferredDeclToEmit(ddi->second);
    deferredDecls.erase(ddi);
  }

  // Handle things which are present even on external declarations.
  if (d) {
    if (langOpts.OpenMP && !langOpts.OpenMPSimd)
      errorNYI(d->getSourceRange(),
               "getOrCreateCIRGlobal: OpenMP target global variable");

    gv.setAlignmentAttr(getSize(astContext.getDeclAlign(d)));

    setLinkageForGV(gv, d);

    if (d->getTLSKind())
      setTLSMode(gv, *d);

    setGVProperties(gv, d);

    // If required by the ABI, treat declarations of static data members with
    // inline initializers as definitions.
    if (astContext.isMSStaticDataMemberInlineDefinition(d))
      errorNYI(d->getSourceRange(),
               "getOrCreateCIRGlobal: MS static data member inline definition");

    // Emit section information for extern variables.
    if (d->hasExternalStorage()) {
      if (const SectionAttr *sa = d->getAttr<SectionAttr>())
        gv.setSectionAttr(builder.getStringAttr(sa->getName()));
    }

    // Handle XCore specific ABI requirements.
    if (getTriple().getArch() == llvm::Triple::xcore)
      errorNYI(d->getSourceRange(),
               "getOrCreateCIRGlobal: XCore specific ABI requirements");

    // Check if we a have a const declaration with an initializer, we may be
    // able to emit it as available_externally to expose it's value to the
    // optimizer.
    if (getLangOpts().CPlusPlus && gv.isPublic() &&
        d->getType().isConstQualified() && gv.isDeclaration() &&
        !d->hasDefinition() && d->hasInit() && !d->hasAttr<DLLImportAttr>())
      errorNYI(
          d->getSourceRange(),
          "getOrCreateCIRGlobal: external const declaration with initializer");
  }

  if (d &&
      d->isThisDeclarationADefinition(astContext) == VarDecl::DeclarationOnly) {
    getTargetCIRGenInfo().setTargetAttributes(d, gv, *this);
    // TODO(cir): set target attributes
    // External HIP managed variables needed to be recorded for transformation
    // in both device and host compilations.
    if (getLangOpts().CUDA && d && d->hasAttr<HIPManagedAttr>() &&
        d->hasExternalStorage())
      errorNYI(d->getSourceRange(),
               "getOrCreateCIRGlobal: HIP managed attribute");
  }

  assert(!cir::MissingFeatures::addressSpace());
  return gv;
}

cir::GlobalOp
CIRGenModule::getOrCreateCIRGlobal(const VarDecl *d, mlir::Type ty,
                                   ForDefinition_t isForDefinition) {
  assert(d->hasGlobalStorage() && "Not a global variable");
  QualType astTy = d->getType();
  if (!ty)
    ty = getTypes().convertTypeForMem(astTy);
  if (selectedDeclRootMode && !isSelectedDeclRoot(GlobalDecl(d)))
    addSelectedDeclDependency(GlobalDecl(d));

  StringRef mangledName = getMangledName(d);
  return getOrCreateCIRGlobal(mangledName, ty, getGlobalVarAddressSpace(d), d,
                              isForDefinition);
}

/// Return the mlir::Value for the address of the given global variable. If
/// \p ty is non-null and if the global doesn't exist, then it will be created
/// with the specified type instead of whatever the normal requested type would
/// be. If \p isForDefinition is true, it is guaranteed that an actual global
/// with type \p ty will be returned, not conversion of a variable with the same
/// mangled name but some other type.
mlir::Value CIRGenModule::getAddrOfGlobalVar(const VarDecl *d, mlir::Type ty,
                                             ForDefinition_t isForDefinition) {
  assert(d->hasGlobalStorage() && "Not a global variable");
  QualType astTy = d->getType();
  if (!ty)
    ty = getTypes().convertTypeForMem(astTy);

  bool tlsAccess = d->getTLSKind() != VarDecl::TLS_None;
  cir::GlobalOp g = getOrCreateCIRGlobal(d, ty, isForDefinition);
  mlir::Type ptrTy = builder.getPointerTo(g.getSymType(), g.getAddrSpaceAttr());
  return cir::GetGlobalOp::create(
      builder, getLoc(d->getSourceRange()), ptrTy, g.getSymNameAttr(),
      tlsAccess,
      /*static_local=*/g.getStaticLocalGuard().has_value());
}

cir::GlobalViewAttr CIRGenModule::getAddrOfGlobalVarAttr(const VarDecl *d) {
  assert(d->hasGlobalStorage() && "Not a global variable");
  mlir::Type ty = getTypes().convertTypeForMem(d->getType());
  cir::GlobalOp globalOp = getOrCreateCIRGlobal(d, ty, NotForDefinition);
  cir::PointerType ptrTy =
      builder.getPointerTo(ty, globalOp.getAddrSpaceAttr());
  return builder.getGlobalViewAttr(ptrTy, globalOp);
}

void CIRGenModule::addUsedGlobal(cir::CIRGlobalValueInterface gv) {
  assert((mlir::isa<cir::FuncOp>(gv.getOperation()) ||
          !gv.isDeclarationForLinker()) &&
         "Only globals with definition can force usage.");
  llvmUsed.emplace_back(gv);
}

void CIRGenModule::addCompilerUsedGlobal(cir::CIRGlobalValueInterface gv) {
  assert(!gv.isDeclarationForLinker() &&
         "Only globals with definition can force usage.");
  llvmCompilerUsed.emplace_back(gv);
}

void CIRGenModule::addUsedOrCompilerUsedGlobal(
    cir::CIRGlobalValueInterface gv) {
  assert((mlir::isa<cir::FuncOp>(gv.getOperation()) ||
          !gv.isDeclarationForLinker()) &&
         "Only globals with definition can force usage.");
  if (getTriple().isOSBinFormatELF())
    llvmCompilerUsed.emplace_back(gv);
  else
    llvmUsed.emplace_back(gv);
}

static void emitUsed(CIRGenModule &cgm, StringRef name,
                     std::vector<cir::CIRGlobalValueInterface> &list) {
  if (list.empty())
    return;

  CIRGenBuilderTy &builder = cgm.getBuilder();
  mlir::Location loc = builder.getUnknownLoc();
  llvm::SmallVector<mlir::Attribute> usedArray;
  usedArray.resize(list.size());
  for (auto [i, op] : llvm::enumerate(list)) {
    usedArray[i] = cir::GlobalViewAttr::get(
        cgm.voidPtrTy, mlir::FlatSymbolRefAttr::get(op.getNameAttr()));
  }

  cir::ArrayType arrayTy = cir::ArrayType::get(cgm.voidPtrTy, usedArray.size());

  cir::ConstArrayAttr initAttr = cir::ConstArrayAttr::get(
      arrayTy, mlir::ArrayAttr::get(&cgm.getMLIRContext(), usedArray));

  cir::GlobalOp gv = cgm.createGlobalOp(loc, name, arrayTy,
                                        /*isConstant=*/false);
  gv.setLinkage(cir::GlobalLinkageKind::AppendingLinkage);
  gv.setInitialValueAttr(initAttr);
  gv.setSectionAttr(builder.getStringAttr("llvm.metadata"));
}

void CIRGenModule::emitLLVMUsed() {
  emitUsed(*this, "llvm.used", llvmUsed);
  emitUsed(*this, "llvm.compiler.used", llvmCompilerUsed);
}

void CIRGenModule::emitGlobalVarDefinition(const clang::VarDecl *vd,
                                           bool isTentative) {
  llvm::TimeTraceScope timeScope("CIRGen Global Variable", [&]() {
    std::string name;
    llvm::raw_string_ostream os(name);
    vd->getNameForDiagnostic(os, astContext.getPrintingPolicy(),
                             /*Qualified=*/true);
    return name;
  });
  if (getLangOpts().OpenCL || getLangOpts().OpenMPIsTargetDevice) {
    errorNYI(vd->getSourceRange(),
             "emitGlobalVarDefinition: emit OpenCL/OpenMP global variable");
    return;
  }

  // Whether the definition of the variable is available externally.
  // If yes, we shouldn't emit the GloablCtor and GlobalDtor for the variable
  // since this is the job for its original source.
  const bool isSelectedDeclarationOnlyMember =
      isSelectedStaticDataMemberDeclaration(vd);
  const bool isSelectedConstantVariableTemplatePattern =
      this->isSelectedConstantVariableTemplatePattern(vd);
  const bool isDefinitionAvailableExternally =
      isSelectedDeclarationOnlyMember ||
      isSelectedConstantVariableTemplatePattern ||
      astContext.GetGVALinkageForVariable(vd) == GVA_AvailableExternally;

  // It is useless to emit the definition for an available_externally variable
  // which can't be marked as const.
  // A class-template static data member covered by an explicit instantiation
  // declaration records constant initialization only on its initializing
  // in-class declaration; the instantiated out-of-line definition that CIR
  // emits never carries an EvaluatedStmt, so hasConstantInitialization() is
  // false on `vd` even though the exact evaluated constant exists (libc++
  // basic_string<CharT>::npos). Selected-decl emission owns that typed
  // initializer fact: materialize the available_externally definition from
  // the initializing declaration's evaluated constant instead of leaving the
  // selected symbol declaration-only. The explicit-instantiation-definition
  // translation unit still owns the strong symbol.
  auto hasSelectedEmittableConstantInit = [&] {
    if (!selectedDeclRootMode)
      return false;
    if (vd->needsDestruction(astContext) ||
        !vd->getType().isConstantStorage(astContext, /*ExcludeCtor=*/true,
                                         /*ExcludeDtor=*/true))
      return false;
    const VarDecl *initializingDecl = nullptr;
    if (!vd->getAnyInitializer(initializingDecl))
      return false;
    return initializingDecl->evaluateValue() != nullptr;
  };
  if (isDefinitionAvailableExternally &&
      (!vd->hasConstantInitialization() ||
       // TODO: Update this when we have interface to check constexpr
       // destructor.
       vd->needsDestruction(astContext) ||
       !vd->getType().isConstantStorage(astContext, true, true)) &&
      !hasSelectedEmittableConstantInit())
    return;

  mlir::Attribute init;
  bool needsGlobalCtor = false;
  bool needsGlobalDtor =
      !isDefinitionAvailableExternally &&
      vd->needsDestruction(astContext) == QualType::DK_cxx_destructor;
  const VarDecl *initDecl;
  const Expr *initExpr = vd->getAnyInitializer(initDecl);

  std::optional<ConstantEmitter> emitter;

  // CUDA E.2.4.1 "__shared__ variables cannot have an initialization
  // as part of their declaration."  Sema has already checked for
  // error cases, so we just need to set Init to PoisonValue.
  bool isCUDASharedVar =
      getLangOpts().CUDAIsDevice && vd->hasAttr<CUDASharedAttr>();
  // Shadows of initialized device-side global variables are also left
  // undefined.
  // Managed Variables should be initialized on both host side and device side.
  bool isCUDAShadowVar =
      !getLangOpts().CUDAIsDevice && !vd->hasAttr<HIPManagedAttr>() &&
      (vd->hasAttr<CUDAConstantAttr>() || vd->hasAttr<CUDADeviceAttr>() ||
       vd->hasAttr<CUDASharedAttr>());
  bool isCUDADeviceShadowVar =
      getLangOpts().CUDAIsDevice && !vd->hasAttr<HIPManagedAttr>() &&
      (vd->getType()->isCUDADeviceBuiltinSurfaceType() ||
       vd->getType()->isCUDADeviceBuiltinTextureType());

  if (getLangOpts().CUDA &&
      (isCUDASharedVar || isCUDAShadowVar || isCUDADeviceShadowVar)) {
    init = cir::PoisonAttr::get(convertType(vd->getType()));
  } else if (vd->hasAttr<LoaderUninitializedAttr>()) {
    errorNYI(vd->getSourceRange(),
             "emitGlobalVarDefinition: loader uninitialized attribute");
  } else if (!initExpr) {
    // This is a tentative definition; tentative definitions are
    // implicitly initialized with { 0 }.
    //
    // Note that tentative definitions are only emitted at the end of
    // a translation unit, so they should never have incomplete
    // type. In addition, EmitTentativeDefinition makes sure that we
    // never attempt to emit a tentative definition if a real one
    // exists. A use may still exists, however, so we still may need
    // to do a RAUW.
    assert(!vd->getType()->isIncompleteType() && "Unexpected incomplete type");
    init = builder.getZeroInitAttr(convertType(vd->getType()));
  } else {
    emitter.emplace(*this);
    mlir::Attribute initializer = emitter->tryEmitForInitializer(*initDecl);
    if (!initializer) {
      QualType qt = initExpr->getType();
      if (vd->getType()->isReferenceType())
        qt = vd->getType();

      if (getLangOpts().CPlusPlus) {
        if (initDecl->hasFlexibleArrayInit(astContext))
          errorNYI(vd->getSourceRange(),
                   "emitGlobalVarDefinition: flexible array initializer");
        init = builder.getZeroInitAttr(convertType(qt));
        if (!isDefinitionAvailableExternally)
          needsGlobalCtor = true;
      } else {
        errorNYI(vd->getSourceRange(),
                 "emitGlobalVarDefinition: static initializer");
      }
    } else {
      init = initializer;
      // We don't need an initializer, so remove the entry for the delayed
      // initializer position (just in case this entry was delayed) if we
      // also don't need to register a destructor.
      assert(!cir::MissingFeatures::deferredCXXGlobalInit());
    }
  }

  mlir::Type initType;
  if (mlir::isa<mlir::SymbolRefAttr>(init)) {
    errorNYI(
        vd->getSourceRange(),
        "emitGlobalVarDefinition: global initializer is a symbol reference");
    return;
  } else {
    assert(mlir::isa<mlir::TypedAttr>(init) && "This should have a type");
    auto typedInitAttr = mlir::cast<mlir::TypedAttr>(init);
    initType = typedInitAttr.getType();
  }
  assert(!mlir::isa<mlir::NoneType>(initType) && "Should have a type by now");

  cir::GlobalOp gv =
      getOrCreateCIRGlobal(vd, initType, ForDefinition_t(!isTentative));
  // TODO(cir): Strip off pointer casts from Entry if we get them?

  if (!gv || gv.getSymType() != initType) {
    errorNYI(vd->getSourceRange(),
             "emitGlobalVarDefinition: global initializer with type mismatch");
    return;
  }

  assert(!cir::MissingFeatures::maybeHandleStaticInExternC());

  if (vd->hasAttr<AnnotateAttr>())
    addGlobalAnnotations(vd, gv);

  // Set CIR's linkage type as appropriate.
  cir::GlobalLinkageKind linkage =
      (isSelectedDeclarationOnlyMember ||
       isSelectedConstantVariableTemplatePattern)
          ? cir::GlobalLinkageKind::AvailableExternallyLinkage
          : getCIRLinkageVarDefinition(vd);

  // CUDA B.2.1 "The __device__ qualifier declares a variable that resides on
  // the device. [...]"
  // CUDA B.2.2 "The __constant__ qualifier, optionally used together with
  // __device__, declares a variable that: [...]
  // Is accessible from all the threads within the grid and from the host
  // through the runtime library (cudaGetSymbolAddress() / cudaGetSymbolSize()
  // / cudaMemcpyToSymbol() / cudaMemcpyFromSymbol())."
  if (langOpts.CUDA) {
    if (langOpts.CUDAIsDevice) {
      // __shared__ variables is not marked as externally initialized,
      // because they must not be initialized.
      if (linkage != cir::GlobalLinkageKind::InternalLinkage &&
          !vd->isConstexpr() && !vd->getType().isConstQualified() &&
          (vd->hasAttr<CUDADeviceAttr>() || vd->hasAttr<CUDAConstantAttr>() ||
           vd->getType()->isCUDADeviceBuiltinSurfaceType() ||
           vd->getType()->isCUDADeviceBuiltinTextureType())) {
        gv->setAttr(cir::CUDAExternallyInitializedAttr::getMnemonic(),
                    cir::CUDAExternallyInitializedAttr::get(&getMLIRContext()));
      }
    } else {
      // Adjust linkage of shadow variables in host compilation
      getCUDARuntime().internalizeDeviceSideVar(vd, linkage);
    }
    getCUDARuntime().handleVarRegistration(vd, gv);
  }

  // Set initializer and finalize emission
  CIRGenModule::setInitializer(gv, init);
  if (const auto *memberPointer =
          vd->getType().getCanonicalType()->getAs<MemberPointerType>();
      memberPointer && memberPointer->isMemberFunctionPointer()) {
    setMemberPointerTargetMetadata(gv.getOperation(), vd->getType());
    mlir::NamedAttrList identity;
    const Expr *constantExpr =
        initExpr ? initExpr->IgnoreParenImpCasts() : nullptr;
    const CXXMethodDecl *methodDecl = nullptr;
    if (const auto *address = dyn_cast_or_null<UnaryOperator>(constantExpr);
        address && address->getOpcode() == UO_AddrOf) {
      const Expr *referenced = address->getSubExpr()->IgnoreParenImpCasts();
      if (const auto *declRef = dyn_cast<DeclRefExpr>(referenced))
        methodDecl = dyn_cast<CXXMethodDecl>(declRef->getDecl());
      else if (const auto *member = dyn_cast<MemberExpr>(referenced))
        methodDecl = dyn_cast<CXXMethodDecl>(member->getMemberDecl());
    }
    if (methodDecl) {
      auto exact = buildCIRGenVirtualMethodIdentityAttrs(
          *this, getMLIRContext(), GlobalDecl(methodDecl));
      identity.set("kind", builder.getStringAttr("method"));
      identity.set("method_symbol", exact.method);
      identity.set("is_virtual", builder.getBoolAttr(methodDecl->isVirtual()));
      if (exact.methodUSR)
        identity.set("method_usr", exact.methodUSR);
      if (auto declaringClass = getRecordUSRAttr(methodDecl->getParent()))
        identity.set("method_declaring_class_usr", declaringClass);
      if (exact.rootMethodUSR)
        identity.set("virtual_root_method_usr", exact.rootMethodUSR);
      if (exact.declaringClassUSR)
        identity.set("virtual_root_declaring_class_usr",
                     exact.declaringClassUSR);
      if (exact.rootAlternatives)
        identity.set("virtual_root_alternatives", exact.rootAlternatives);
    } else if (initExpr && initExpr->isNullPointerConstant(
                               astContext, Expr::NPC_ValueDependentIsNull)) {
      identity.set("kind", builder.getStringAttr("null"));
      identity.set("null_function_value", builder.getStringAttr("0"));
      identity.set("null_adjustment_value", builder.getStringAttr("0"));
    }
    if (!identity.empty()) {
      gv->setAttr("ast_member_function_pointer_constant",
                  identity.getDictionary(&getMLIRContext()));
    }
  }
  if (emitter)
    emitter->finalize(gv);

  // If it is safe to mark the global 'constant', do so now.
  // Use the same logic as classic codegen EmitGlobalVarDefinition.
  gv.setConstant((vd->hasAttr<CUDAConstantAttr>() && langOpts.CUDAIsDevice) ||
                 (!needsGlobalCtor && !needsGlobalDtor &&
                  vd->getType().isConstantStorage(astContext,
                                                  /*ExcludeCtor=*/true,
                                                  /*ExcludeDtor=*/true)));
  // If it is in a read-only section, mark it 'constant'.
  if (const SectionAttr *sa = vd->getAttr<SectionAttr>()) {
    const ASTContext::SectionInfo &si = astContext.SectionInfos[sa->getName()];
    if ((si.SectionFlags & ASTContext::PSF_Write) == 0)
      gv.setConstant(true);
  }

  // Set CIR linkage and DLL storage class.
  gv.setLinkage(linkage);
  // FIXME(cir): setLinkage should likely set MLIR's visibility automatically.
  gv.setVisibility(getMLIRVisibilityFromCIRLinkage(linkage));
  assert(!cir::MissingFeatures::opGlobalDLLImportExport());
  if (linkage == cir::GlobalLinkageKind::CommonLinkage) {
    // common vars aren't constant even if declared const.
    gv.setConstant(false);
    // Tentative definition of global variables may be initialized with
    // non-zero null pointers. In this case they should have weak linkage
    // since common linkage must have zero initializer and must not have
    // explicit section therefore cannot have non-zero initial value.
    std::optional<mlir::Attribute> initializer = gv.getInitialValue();
    if (initializer && !getBuilder().isNullValue(*initializer))
      gv.setLinkage(cir::GlobalLinkageKind::WeakAnyLinkage);
  }

  setNonAliasAttributes(vd, gv);

  if (vd->getTLSKind() && !vd->isStaticLocal())
    setTLSMode(gv, *vd);

  maybeSetTrivialComdat(*vd, gv);

  // Emit the initializer function if necessary.
  if (needsGlobalCtor || needsGlobalDtor)
    emitCXXGlobalVarDeclInitFunc(vd, gv, needsGlobalCtor);
}

void CIRGenModule::emitGlobalDefinition(clang::GlobalDecl gd,
                                        mlir::Operation *op) {
  llvm::SaveAndRestore<const Decl *> diagnosticOwner(currentDiagnosticDecl,
                                                     gd.getDecl());
  const auto *decl = cast<ValueDecl>(gd.getDecl());
  if (const auto *fd = dyn_cast<FunctionDecl>(decl)) {
    // TODO(CIR): Skip generation of CIR for functions with available_externally
    // linkage at -O0.

    if (const auto *method = dyn_cast<CXXMethodDecl>(decl)) {
      // Make sure to emit the definition(s) before we emit the thunks. This is
      // necessary for the generation of certain thunks.
      cir::FuncOp definition;
      if (isa<CXXConstructorDecl>(method) || isa<CXXDestructorDecl>(method))
        abi->emitCXXStructor(gd);
      else if (fd->isMultiVersion())
        errorNYI(method->getSourceRange(), "multiversion functions");
      else
        definition = emitGlobalFunctionDefinition(gd, op);

      if (method->isVirtual()) {
        getVTables().emitThunks(gd);
      }

      noteSelectedDeclRootDefinition(gd, definition);
      return;
    }

    if (fd->isMultiVersion())
      errorNYI(fd->getSourceRange(), "multiversion functions");
    cir::FuncOp definition = emitGlobalFunctionDefinition(gd, op);
    noteSelectedDeclRootDefinition(gd, definition);
    return;
  }

  if (const auto *vd = dyn_cast<VarDecl>(decl)) {
    emitGlobalVarDefinition(vd, !vd->hasDefinition());
    noteSelectedDeclRootDefinition(gd);
    return;
  }

  llvm_unreachable("Invalid argument to CIRGenModule::emitGlobalDefinition");
}

mlir::Attribute
CIRGenModule::getConstantArrayFromStringLiteral(const StringLiteral *e) {
  assert(!e->getType()->isPointerType() && "Strings are always arrays");

  // Don't emit it as the address of the string, emit the string data itself
  // as an inline array.
  if (e->getCharByteWidth() == 1) {
    SmallString<64> str(e->getString());

    // Resize the string to the right size, which is indicated by its type.
    const ConstantArrayType *cat =
        astContext.getAsConstantArrayType(e->getType());
    uint64_t finalSize = cat->getZExtSize();
    str.resize(finalSize);

    mlir::Type eltTy = convertType(cat->getElementType());
    return builder.getString(str, eltTy, finalSize, /*ensureNullTerm=*/false);
  }

  auto arrayTy = mlir::cast<cir::ArrayType>(convertType(e->getType()));

  auto arrayEltTy = mlir::cast<cir::IntType>(arrayTy.getElementType());

  uint64_t arraySize = arrayTy.getSize();
  unsigned literalSize = e->getLength();
  assert(arraySize > literalSize &&
         "wide string literal array size must have room for null terminator?");

  // Check if the string is all null bytes before building the vector.
  // In most non-zero cases, this will break out on the first element.
  bool isAllZero = true;
  for (unsigned i = 0; i < literalSize; ++i) {
    if (e->getCodeUnit(i) != 0) {
      isAllZero = false;
      break;
    }
  }

  if (isAllZero)
    return cir::ZeroAttr::get(arrayTy);

  // Otherwise emit a constant array holding the characters.
  SmallVector<mlir::Attribute> elements;
  elements.reserve(arraySize);
  for (unsigned i = 0; i < literalSize; ++i)
    elements.push_back(cir::IntAttr::get(arrayEltTy, e->getCodeUnit(i)));

  auto elementsAttr = mlir::ArrayAttr::get(&getMLIRContext(), elements);
  return builder.getConstArray(elementsAttr, arrayTy);
}

bool CIRGenModule::supportsCOMDAT() const {
  return getTriple().supportsCOMDAT();
}

static bool shouldBeInCOMDAT(CIRGenModule &cgm, const Decl &d) {
  if (!cgm.supportsCOMDAT())
    return false;

  if (d.hasAttr<SelectAnyAttr>())
    return true;

  GVALinkage linkage;
  if (auto *vd = dyn_cast<VarDecl>(&d))
    linkage = cgm.getASTContext().GetGVALinkageForVariable(vd);
  else
    linkage =
        cgm.getASTContext().GetGVALinkageForFunction(cast<FunctionDecl>(&d));

  switch (linkage) {
  case clang::GVA_Internal:
  case clang::GVA_AvailableExternally:
  case clang::GVA_StrongExternal:
    return false;
  case clang::GVA_DiscardableODR:
  case clang::GVA_StrongODR:
    return true;
  }
  llvm_unreachable("No such linkage");
}

void CIRGenModule::maybeSetTrivialComdat(const Decl &d, mlir::Operation *op) {
  if (!shouldBeInCOMDAT(*this, d))
    return;
  if (auto globalOp = dyn_cast_or_null<cir::GlobalOp>(op)) {
    globalOp.setComdat(true);
  } else {
    auto funcOp = cast<cir::FuncOp>(op);
    funcOp.setComdat(true);
  }
}

void CIRGenModule::updateCompletedType(const TagDecl *td) {
  // Make sure that this type is translated.
  genTypes.updateCompletedType(td);
}

void CIRGenModule::addReplacement(StringRef name, mlir::Operation *op) {
  auto symbol = mlir::cast<mlir::SymbolOpInterface>(op);
  replacements[name] = symbol.getNameAttr();
}

#ifndef NDEBUG
static bool verifyPointerTypeArgs(cir::CallOp call, cir::FuncOp newF) {
  for (auto [argOp, fnArgType] :
       llvm::zip(call.getArgs(), newF.getFunctionType().getInputs())) {
    if (argOp.getType() != fnArgType)
      return false;
  }

  return true;
}
#endif // NDEBUG

void CIRGenModule::applyReplacements() {
  if (replacements.empty())
    return;

  struct Replacement {
    cir::FuncOp oldFunction;
    cir::FuncOp newFunction;
  };

  // Resolve the replacement graph without recursion before looking at the IR.
  // The graph is functional (each source has one target), so memoizing each
  // terminal target makes chain resolution linear in the number of entries.
  llvm::DenseMap<mlir::StringAttr, mlir::StringAttr> directReplacements;
  for (const auto &replacement : replacements) {
    directReplacements.try_emplace(
        mlir::StringAttr::get(&getMLIRContext(), replacement.getKey()),
        replacement.getValue());
  }

  llvm::DenseMap<mlir::StringAttr, mlir::StringAttr> resolvedReplacements;
  llvm::DenseSet<mlir::StringAttr> resolving;
  for (const auto &replacement : directReplacements) {
    mlir::StringAttr sourceName = replacement.first;
    if (resolvedReplacements.count(sourceName))
      continue;

    llvm::SmallVector<mlir::StringAttr, 4> path;
    mlir::StringAttr currentName = sourceName;
    mlir::StringAttr finalName;
    while (true) {
      auto resolved = resolvedReplacements.find(currentName);
      if (resolved != resolvedReplacements.end()) {
        finalName = resolved->second;
        break;
      }

      auto direct = directReplacements.find(currentName);
      if (direct == directReplacements.end()) {
        finalName = currentName;
        break;
      }

      if (!resolving.insert(currentName).second) {
        mlir::Operation *cycleOp = getGlobalValue(currentName.getValue());
        errorNYI(cycleOp ? cycleOp->getLoc() : theModule.getLoc(),
                 "cyclic function replacement");
        replacements.clear();
        return;
      }
      path.push_back(currentName);
      currentName = direct->second;
    }

    for (mlir::StringAttr name : path) {
      resolving.erase(name);
      resolvedReplacements.try_emplace(name, finalName);
    }
  }

  llvm::SmallVector<Replacement> pendingReplacements;
  bool invalidReplacement = false;
  for (const auto &replacement : replacements) {
    llvm::StringRef mangledName = replacement.getKey();
    mlir::Operation *entry = getGlobalValue(mangledName);
    if (!entry)
      continue;

    auto oldF = mlir::dyn_cast<cir::FuncOp>(entry);
    if (!oldF) {
      errorNYI(entry->getLoc(), "replacement source is not a function");
      invalidReplacement = true;
      continue;
    }

    mlir::StringAttr sourceName =
        mlir::StringAttr::get(&getMLIRContext(), mangledName);
    auto resolved = resolvedReplacements.find(sourceName);
    assert(resolved != resolvedReplacements.end() &&
           "replacement source was not resolved");
    auto newF = mlir::dyn_cast_or_null<cir::FuncOp>(
        getGlobalValue(resolved->second.getValue()));
    if (!newF) {
      errorNYI(entry->getLoc(), "replacement target is not a function");
      invalidReplacement = true;
      continue;
    }
    if (newF == oldF)
      continue;

    pendingReplacements.push_back({oldF, newF});
  }
  replacements.clear();

  // Do not mutate any references when one entry in the batch is invalid.
  if (invalidReplacement || pendingReplacements.empty())
    return;

  llvm::DenseMap<mlir::StringAttr, mlir::StringAttr> terminalBySource;
  for (const Replacement &replacement : pendingReplacements) {
    cir::FuncOp oldFunction = replacement.oldFunction;
    cir::FuncOp newFunction = replacement.newFunction;
    terminalBySource.try_emplace(oldFunction.getSymNameAttr(),
                                 newFunction.getSymNameAttr());
  }
  auto normalizeLifecycleIdentitySymbols =
      [&](auto &&self, mlir::Attribute attribute) -> mlir::Attribute {
    if (auto dictionary = mlir::dyn_cast<mlir::DictionaryAttr>(attribute)) {
      mlir::NamedAttrList normalized;
      bool changed = false;
      for (mlir::NamedAttribute named : dictionary) {
        mlir::Attribute value = named.getValue();
        llvm::StringRef name = named.getName().strref();
        // Cleanup constructor/destructor symbols and call callee_symbol name
        // the exact ABI body actually used, so they follow replacement.
        // ast_constructor_call.canonical_symbol is instead the stable
        // complete-object C1 declaration identity and must not be rewritten.
        if (name == "constructor_symbol" || name == "destructor_symbol" ||
            name == "callee_symbol") {
          if (auto symbol = mlir::dyn_cast<mlir::StringAttr>(value)) {
            auto replacement = terminalBySource.find(symbol);
            if (replacement != terminalBySource.end()) {
              value = replacement->second;
              changed = true;
            }
          }
        } else {
          mlir::Attribute nested = self(self, value);
          changed |= nested != value;
          value = nested;
        }
        normalized.append(named.getName(), value);
      }
      return changed ? normalized.getDictionary(&getMLIRContext()) : attribute;
    }
    if (auto array = mlir::dyn_cast<mlir::ArrayAttr>(attribute)) {
      llvm::SmallVector<mlir::Attribute> normalized;
      normalized.reserve(array.size());
      bool changed = false;
      for (mlir::Attribute element : array) {
        mlir::Attribute nested = self(self, element);
        changed |= nested != element;
        normalized.push_back(nested);
      }
      return changed ? mlir::ArrayAttr::get(&getMLIRContext(), normalized)
                     : attribute;
    }
    return attribute;
  };

  // Build one batch replacer; scope discovery below collects every relevant
  // module-level use before any operation is mutated or erased.
  mlir::AttrTypeReplacer replacer;
  replacer.addReplacement([&](mlir::SymbolRefAttr symbolRef)
                              -> std::pair<mlir::Attribute, mlir::WalkResult> {
    auto replacement = terminalBySource.find(symbolRef.getRootReference());
    if (replacement == terminalBySource.end())
      return {symbolRef, mlir::WalkResult::skip()};

    mlir::StringAttr newName = replacement->second;
    if (mlir::isa<mlir::FlatSymbolRefAttr>(symbolRef))
      return {mlir::FlatSymbolRefAttr::get(newName), mlir::WalkResult::skip()};
    return {mlir::SymbolRefAttr::get(newName, symbolRef.getNestedReferences()),
            mlir::WalkResult::skip()};
  });

  using AttributeUpdate = std::pair<mlir::Operation *, mlir::DictionaryAttr>;
  llvm::SmallVector<AttributeUpdate> attributeUpdates;
  {
    auto symbolUses =
        mlir::SymbolTable::getSymbolUses(&theModule.getBodyRegion());
    if (!symbolUses) {
      errorNYI(theModule.getLoc(),
               "failed to replace all function symbol uses");
      return;
    }

    llvm::DenseSet<mlir::Operation *> relevantUsers;
    theModule.walk([&](mlir::Operation *op) {
      if (op->hasAttr("ast_automatic_object_identity") ||
          op->hasAttr("ast_temporary_object_identities") ||
          op->hasAttr("ast_conditional_cleanup_identities") ||
          op->hasAttr("ast_constructor_call"))
        relevantUsers.insert(op);
    });
    for (const mlir::SymbolTable::SymbolUse &use : *symbolUses)
      if (terminalBySource.count(use.getSymbolRef().getRootReference()))
        relevantUsers.insert(use.getUser());

    for (mlir::Operation *op : relevantUsers) {
      mlir::StringAttr normalizedDestructorCallee;
      mlir::StringAttr normalizedDestructorVariant;
      mlir::StringAttr normalizedConstructorCallee;
      mlir::StringAttr normalizedConstructorVariant;
      if (auto call = mlir::dyn_cast<cir::CallOp>(op)) {
        if (mlir::FlatSymbolRefAttr callee = call.getCalleeAttr()) {
          auto replacement = terminalBySource.find(callee.getRootReference());
          if (replacement != terminalBySource.end()) {
            cir::FuncOp newFunction =
                lookupFuncOp(replacement->second.getValue());
            assert(newFunction && "replacement target disappeared");
            assert(verifyPointerTypeArgs(call, newFunction) &&
                   "call argument types do not match replacement function");
            if (op->hasAttr("ast_destructor_call")) {
              normalizedDestructorCallee = replacement->second;
              normalizedDestructorVariant =
                  newFunction->getAttrOfType<mlir::StringAttr>(
                      "abi_dtor_variant");
              if (!normalizedDestructorVariant) {
                errorNYI(op->getLoc(),
                         "destructor replacement target has no ABI variant");
                return;
              }
            }
            if (op->hasAttr("ast_constructor_call")) {
              normalizedConstructorCallee = replacement->second;
              normalizedConstructorVariant =
                  newFunction->getAttrOfType<mlir::StringAttr>(
                      "abi_ctor_variant");
              if (!normalizedConstructorVariant) {
                errorNYI(op->getLoc(),
                         "constructor replacement target has no ABI variant");
                return;
              }
            }
          }
        }
      }

      mlir::DictionaryAttr oldAttrs = op->getAttrDictionary();
      mlir::Attribute rewritten = replacer.replace(oldAttrs);
      if (!rewritten) {
        errorNYI(op->getLoc(), "failed to replace all function symbol uses");
        return;
      }
      auto newAttrs = mlir::cast<mlir::DictionaryAttr>(rewritten);
      {
        mlir::NamedAttrList normalized(newAttrs);
        for (llvm::StringRef name :
             {"ast_automatic_object_identity",
              "ast_temporary_object_identities",
              "ast_conditional_cleanup_identities", "ast_constructor_call"}) {
          if (mlir::Attribute identity = newAttrs.get(name))
            normalized.set(name,
                           normalizeLifecycleIdentitySymbols(
                               normalizeLifecycleIdentitySymbols, identity));
        }
        newAttrs = normalized.getDictionary(&getMLIRContext());
      }
      if (normalizedDestructorCallee) {
        auto destructorIdentity =
            newAttrs.getAs<mlir::DictionaryAttr>("ast_destructor_call");
        assert(destructorIdentity &&
               "destructor call identity disappeared during replacement");
        mlir::NamedAttrList normalizedIdentity(destructorIdentity);
        normalizedIdentity.set("callee_symbol", normalizedDestructorCallee);
        normalizedIdentity.set("variant", normalizedDestructorVariant);
        mlir::NamedAttrList normalizedAttrs(newAttrs);
        normalizedAttrs.set(
            "ast_destructor_call",
            normalizedIdentity.getDictionary(&getMLIRContext()));
        newAttrs = normalizedAttrs.getDictionary(&getMLIRContext());
      }
      if (normalizedConstructorCallee) {
        auto constructorIdentity =
            newAttrs.getAs<mlir::DictionaryAttr>("ast_constructor_call");
        assert(constructorIdentity &&
               "constructor call identity disappeared during replacement");
        mlir::NamedAttrList normalizedIdentity(constructorIdentity);
        normalizedIdentity.set("callee_symbol", normalizedConstructorCallee);
        normalizedIdentity.set("variant", normalizedConstructorVariant);
        mlir::NamedAttrList normalizedAttrs(newAttrs);
        normalizedAttrs.set(
            "ast_constructor_call",
            normalizedIdentity.getDictionary(&getMLIRContext()));
        newAttrs = normalizedAttrs.getDictionary(&getMLIRContext());
      }
      if (newAttrs != oldAttrs)
        attributeUpdates.emplace_back(op, newAttrs);
    }
  }

  // Attribute replacement cannot fail after the complete set has been staged.
  // Apply every reference update before invalidating any source operation.
  for (const auto &[op, attrs] : attributeUpdates)
    op->setAttrs(attrs);

  for (Replacement replacement : pendingReplacements) {
    cir::FuncOp oldF = replacement.oldFunction;
    cir::FuncOp newF = replacement.newFunction;

    // Replace old with new, but keep the old order.
    newF->moveBefore(oldF);
    eraseGlobalSymbol(oldF);
    oldF->erase();
  }
}

cir::GlobalOp CIRGenModule::createOrReplaceCXXRuntimeVariable(
    mlir::Location loc, StringRef name, mlir::Type ty,
    cir::GlobalLinkageKind linkage, clang::CharUnits alignment) {
  auto gv = mlir::dyn_cast_or_null<cir::GlobalOp>(getGlobalValue(name));

  if (gv) {
    // Check if the variable has the right type.
    if (gv.getSymType() == ty)
      return gv;

    // Because of C++ name mangling, the only way we can end up with an already
    // existing global with the same name is if it has been declared extern
    // "C".
    assert(gv.isDeclaration() && "Declaration has wrong type!");

    errorNYI(loc, "createOrReplaceCXXRuntimeVariable: declaration exists with "
                  "wrong type");
    return gv;
  }

  // Create a new variable.
  gv = createGlobalOp(loc, name, ty);

  // Set up extra information and add to the module
  gv.setLinkageAttr(
      cir::GlobalLinkageKindAttr::get(&getMLIRContext(), linkage));
  mlir::SymbolTable::setSymbolVisibility(gv,
                                         CIRGenModule::getMLIRVisibility(gv));

  if (supportsCOMDAT() && cir::isWeakForLinker(linkage) &&
      !gv.hasAvailableExternallyLinkage()) {
    gv.setComdat(true);
  }

  gv.setAlignmentAttr(getSize(alignment));
  setDSOLocal(static_cast<mlir::Operation *>(gv));
  return gv;
}

// TODO(CIR): this could be a common method between LLVM codegen.
static bool isVarDeclStrongDefinition(const ASTContext &astContext,
                                      CIRGenModule &cgm, const VarDecl *vd,
                                      bool noCommon) {
  // Don't give variables common linkage if -fno-common was specified unless it
  // was overridden by a NoCommon attribute.
  if ((noCommon || vd->hasAttr<NoCommonAttr>()) && !vd->hasAttr<CommonAttr>())
    return true;

  // C11 6.9.2/2:
  //   A declaration of an identifier for an object that has file scope without
  //   an initializer, and without a storage-class specifier or with the
  //   storage-class specifier static, constitutes a tentative definition.
  if (vd->getInit() || vd->hasExternalStorage())
    return true;

  // A variable cannot be both common and exist in a section.
  if (vd->hasAttr<SectionAttr>())
    return true;

  // A variable cannot be both common and exist in a section.
  // We don't try to determine which is the right section in the front-end.
  // If no specialized section name is applicable, it will resort to default.
  if (vd->hasAttr<PragmaClangBSSSectionAttr>() ||
      vd->hasAttr<PragmaClangDataSectionAttr>() ||
      vd->hasAttr<PragmaClangRelroSectionAttr>() ||
      vd->hasAttr<PragmaClangRodataSectionAttr>())
    return true;

  // Thread local vars aren't considered common linkage.
  if (vd->getTLSKind())
    return true;

  // Tentative definitions marked with WeakImportAttr are true definitions.
  if (vd->hasAttr<WeakImportAttr>())
    return true;

  // A variable cannot be both common and exist in a comdat.
  if (shouldBeInCOMDAT(cgm, *vd))
    return true;

  // Declarations with a required alignment do not have common linkage in MSVC
  // mode.
  if (astContext.getTargetInfo().getCXXABI().isMicrosoft()) {
    if (vd->hasAttr<AlignedAttr>())
      return true;
    QualType varType = vd->getType();
    if (astContext.isAlignmentRequired(varType))
      return true;

    if (const auto *rd = varType->getAsRecordDecl()) {
      for (const FieldDecl *fd : rd->fields()) {
        if (fd->isBitField())
          continue;
        if (fd->hasAttr<AlignedAttr>())
          return true;
        if (astContext.isAlignmentRequired(fd->getType()))
          return true;
      }
    }
  }

  // Microsoft's link.exe doesn't support alignments greater than 32 bytes for
  // common symbols, so symbols with greater alignment requirements cannot be
  // common.
  // Other COFF linkers (ld.bfd and LLD) support arbitrary power-of-two
  // alignments for common symbols via the aligncomm directive, so this
  // restriction only applies to MSVC environments.
  if (astContext.getTargetInfo().getTriple().isKnownWindowsMSVCEnvironment() &&
      astContext.getTypeAlignIfKnown(vd->getType()) >
          astContext.toBits(CharUnits::fromQuantity(32)))
    return true;

  return false;
}

cir::GlobalLinkageKind
CIRGenModule::getCIRLinkageForDeclarator(const DeclaratorDecl *dd,
                                         GVALinkage linkage) {
  if (linkage == GVA_Internal)
    return cir::GlobalLinkageKind::InternalLinkage;

  if (dd->hasAttr<WeakAttr>())
    return cir::GlobalLinkageKind::WeakAnyLinkage;

  if (const auto *fd = dd->getAsFunction())
    if (fd->isMultiVersion() && linkage == GVA_AvailableExternally)
      return cir::GlobalLinkageKind::LinkOnceAnyLinkage;

  // We are guaranteed to have a strong definition somewhere else,
  // so we can use available_externally linkage.
  if (linkage == GVA_AvailableExternally)
    return cir::GlobalLinkageKind::AvailableExternallyLinkage;

  // Note that Apple's kernel linker doesn't support symbol
  // coalescing, so we need to avoid linkonce and weak linkages there.
  // Normally, this means we just map to internal, but for explicit
  // instantiations we'll map to external.

  // In C++, the compiler has to emit a definition in every translation unit
  // that references the function.  We should use linkonce_odr because
  // a) if all references in this translation unit are optimized away, we
  // don't need to codegen it.  b) if the function persists, it needs to be
  // merged with other definitions. c) C++ has the ODR, so we know the
  // definition is dependable.
  if (linkage == GVA_DiscardableODR)
    return !astContext.getLangOpts().AppleKext
               ? cir::GlobalLinkageKind::LinkOnceODRLinkage
               : cir::GlobalLinkageKind::InternalLinkage;

  // An explicit instantiation of a template has weak linkage, since
  // explicit instantiations can occur in multiple translation units
  // and must all be equivalent. However, we are not allowed to
  // throw away these explicit instantiations.
  //
  // CUDA/HIP: For -fno-gpu-rdc case, device code is limited to one TU,
  // so say that CUDA templates are either external (for kernels) or internal.
  // This lets llvm perform aggressive inter-procedural optimizations. For
  // -fgpu-rdc case, device function calls across multiple TU's are allowed,
  // therefore we need to follow the normal linkage paradigm.
  if (linkage == GVA_StrongODR) {
    if (getLangOpts().AppleKext)
      return cir::GlobalLinkageKind::ExternalLinkage;
    if (getLangOpts().CUDA && getLangOpts().CUDAIsDevice &&
        !getLangOpts().GPURelocatableDeviceCode)
      return dd->hasAttr<CUDAGlobalAttr>()
                 ? cir::GlobalLinkageKind::ExternalLinkage
                 : cir::GlobalLinkageKind::InternalLinkage;
    return cir::GlobalLinkageKind::WeakODRLinkage;
  }

  // C++ doesn't have tentative definitions and thus cannot have common
  // linkage.
  if (!getLangOpts().CPlusPlus && isa<VarDecl>(dd) &&
      !isVarDeclStrongDefinition(astContext, *this, cast<VarDecl>(dd),
                                 getCodeGenOpts().NoCommon))
    return cir::GlobalLinkageKind::CommonLinkage;

  // selectany symbols are externally visible, so use weak instead of
  // linkonce.  MSVC optimizes away references to const selectany globals, so
  // all definitions should be the same and ODR linkage should be used.
  // http://msdn.microsoft.com/en-us/library/5tkz6s71.aspx
  if (dd->hasAttr<SelectAnyAttr>())
    return cir::GlobalLinkageKind::WeakODRLinkage;

  // Otherwise, we have strong external linkage.
  assert(linkage == GVA_StrongExternal);
  return cir::GlobalLinkageKind::ExternalLinkage;
}

/// This function is called when we implement a function with no prototype, e.g.
/// "int foo() {}". If there are existing call uses of the old function in the
/// module, this adjusts them to call the new function directly.
///
/// This is not just a cleanup: the always_inline pass requires direct calls to
/// functions to be able to inline them.  If there is a bitcast in the way, it
/// won't inline them. Instcombine normally deletes these calls, but it isn't
/// run at -O0.
void CIRGenModule::replaceUsesOfNonProtoTypeWithRealFunction(
    mlir::Operation *old, cir::FuncOp newFn) {
  // If we're redefining a global as a function, don't transform it.
  auto oldFn = mlir::dyn_cast<cir::FuncOp>(old);
  if (!oldFn)
    return;

  // TODO(cir): this RAUW ignores the features below.
  assert(!cir::MissingFeatures::opFuncExceptions());
  assert(!cir::MissingFeatures::opFuncParameterAttributes());
  assert(!cir::MissingFeatures::opFuncOperandBundles());
  if (oldFn->getAttrs().size() <= 1)
    errorNYI(old->getLoc(),
             "replaceUsesOfNonProtoTypeWithRealFunction: Attribute forwarding");

  // Mark new function as originated from a no-proto declaration.
  newFn.setNoProto(oldFn.getNoProto());

  // Iterate through all calls of the no-proto function.
  std::optional<mlir::SymbolTable::UseRange> symUses =
      oldFn.getSymbolUses(oldFn->getParentOp());
  for (const mlir::SymbolTable::SymbolUse &use : symUses.value()) {
    mlir::OpBuilder::InsertionGuard guard(builder);

    if (auto noProtoCallOp = mlir::dyn_cast<cir::CallOp>(use.getUser())) {
      builder.setInsertionPoint(noProtoCallOp);

      // Patch call type with the real function type.
      cir::FuncType newFnType = newFn.getFunctionType();
      mlir::OperandRange callOperands = noProtoCallOp.getOperands();
      bool returnTypeMatches =
          newFnType.hasVoidReturn()
              ? noProtoCallOp.getNumResults() == 0
              : noProtoCallOp.getNumResults() == 1 &&
                    noProtoCallOp.getResultTypes().front() ==
                        newFnType.getReturnType();
      bool typesMatch = !newFn.getNoProto() && returnTypeMatches &&
                        callOperands.size() == newFnType.getNumInputs();
      for (unsigned i = 0, e = newFnType.getNumInputs(); typesMatch && i != e;
           ++i) {
        if (callOperands[i].getType() != newFnType.getInput(i))
          typesMatch = false;
      }

      cir::CallOp realCallOp;
      if (typesMatch) {
        // Patch call type with the real function type.
        realCallOp =
            builder.createCallOp(noProtoCallOp.getLoc(), newFn, callOperands);
      } else {
        // Build an indirect call whose function-pointer signature matches
        // the existing call site.
        cir::FuncType origFnType = oldFn.getFunctionType();
        cir::FuncType callFnType =
            origFnType.isVarArg()
                ? cir::FuncType::get(origFnType.getInputs(),
                                     origFnType.getReturnType(),
                                     /*isVarArg=*/false)
                : origFnType;
        mlir::Value addr = cir::GetGlobalOp::create(
            builder, noProtoCallOp.getLoc(), cir::PointerType::get(newFnType),
            newFn.getSymName());
        mlir::Value casted =
            builder.createBitcast(addr, cir::PointerType::get(callFnType));
        realCallOp = builder.createIndirectCallOp(
            noProtoCallOp.getLoc(), casted, callFnType, callOperands);
      }

      // Replace old no proto call with fixed call.
      noProtoCallOp.replaceAllUsesWith(realCallOp);
      noProtoCallOp.erase();
    } else if (auto getGlobalOp =
                   mlir::dyn_cast<cir::GetGlobalOp>(use.getUser())) {
      // The GetGlobal was emitted with the no-proto FuncType. Uses of this
      // operation (cir.store, cir.cast) were built for that pointer type. When
      // we re-type the result to the real FuncType, we need to add a bit the
      // old pointer type so those uses are still valid. This can lead to
      // some redundant bitcast chains, but those will be cleaned up by the
      // canonicalizer.
      mlir::Value res = getGlobalOp.getAddr();
      const mlir::Type oldResTy = res.getType();
      const auto newPtrTy = cir::PointerType::get(newFn.getFunctionType());
      if (oldResTy != newPtrTy) {
        res.setType(newPtrTy);
        builder.setInsertionPointAfter(getGlobalOp.getOperation());
        mlir::Value castRes =
            cir::CastOp::create(builder, getGlobalOp.getLoc(), oldResTy,
                                cir::CastKind::bitcast, res);
        res.replaceAllUsesExcept(castRes, castRes.getDefiningOp());
      }
    } else if (mlir::isa<cir::GlobalOp>(use.getUser())) {
      // Function addresses in global initializers use GlobalViewAttrs typed to
      // the initializer context (e.g. struct field type), not the FuncOp type,
      // so no update is required when the no-proto FuncOp is replaced.
    } else {
      llvm_unreachable(
          "replaceUsesOfNonProtoTypeWithRealFunction: unexpected use type");
    }
  }
}

cir::GlobalLinkageKind
CIRGenModule::getCIRLinkageVarDefinition(const VarDecl *vd) {
  GVALinkage linkage = astContext.GetGVALinkageForVariable(vd);
  return getCIRLinkageForDeclarator(vd, linkage);
}

cir::GlobalLinkageKind CIRGenModule::getFunctionLinkage(GlobalDecl gd) {
  const auto *d = cast<FunctionDecl>(gd.getDecl());

  GVALinkage linkage = astContext.GetGVALinkageForFunction(d);

  if (const auto *dtor = dyn_cast<CXXDestructorDecl>(d))
    return getCXXABI().getCXXDestructorLinkage(linkage, dtor, gd.getDtorType());

  return getCIRLinkageForDeclarator(d, linkage);
}

static cir::GlobalOp
generateStringLiteral(mlir::Location loc, mlir::TypedAttr c,
                      cir::GlobalLinkageKind lt, CIRGenModule &cgm,
                      StringRef globalName, CharUnits alignment) {
  assert(!cir::MissingFeatures::addressSpace());

  // Create a global variable for this string
  // FIXME(cir): check for insertion point in module level.
  cir::GlobalOp gv = cgm.createGlobalOp(loc, globalName, c.getType(),
                                        !cgm.getLangOpts().WritableStrings);

  // Set up extra information and add to the module
  gv.setAlignmentAttr(cgm.getSize(alignment));
  gv.setLinkageAttr(
      cir::GlobalLinkageKindAttr::get(cgm.getBuilder().getContext(), lt));
  assert(!cir::MissingFeatures::opGlobalThreadLocal());
  assert(!cir::MissingFeatures::opGlobalUnnamedAddr());
  CIRGenModule::setInitializer(gv, c);
  if (gv.isWeakForLinker()) {
    assert(cgm.supportsCOMDAT() && "Only COFF uses weak string literals");
    gv.setComdat(true);
  }
  cgm.setDSOLocal(static_cast<mlir::Operation *>(gv));
  return gv;
}

// LLVM IR automatically uniques names when new llvm::GlobalVariables are
// created. This is handy, for example, when creating globals for string
// literals. Since we don't do that when creating cir::GlobalOp's, we need
// a mechanism to generate a unique name in advance.
//
// For now, this mechanism is only used in cases where we know that the
// name is compiler-generated, so we don't use the MLIR symbol table for
// the lookup.
std::string CIRGenModule::getUniqueGlobalName(const std::string &baseName) {
  // If this is the first time we've generated a name for this basename, use
  // it as is and start a counter for this base name.
  auto it = cgGlobalNames.find(baseName);
  if (it == cgGlobalNames.end()) {
    cgGlobalNames[baseName] = 1;
    return baseName;
  }

  std::string result =
      baseName + "." + std::to_string(cgGlobalNames[baseName]++);
  // There should not be any symbol with this name in the module.
  assert(!getGlobalValue(result));
  return result;
}

/// Return a pointer to a constant array for the given string literal.
cir::GlobalOp CIRGenModule::getGlobalForStringLiteral(const StringLiteral *s,
                                                      StringRef name) {
  CharUnits alignment =
      astContext.getAlignOfGlobalVarInChars(s->getType(), /*VD=*/nullptr);

  mlir::Attribute c = getConstantArrayFromStringLiteral(s);

  cir::GlobalOp gv;
  if (!getLangOpts().WritableStrings && constantStringMap.count(c)) {
    gv = constantStringMap[c];
    // The bigger alignment always wins.
    if (!gv.getAlignment() ||
        uint64_t(alignment.getQuantity()) > *gv.getAlignment())
      gv.setAlignmentAttr(getSize(alignment));
  } else {
    // Mangle the string literal if that's how the ABI merges duplicate strings.
    // Don't do it if they are writable, since we don't want writes in one TU to
    // affect strings in another.
    if (getCXXABI().getMangleContext().shouldMangleStringLiteral(s) &&
        !getLangOpts().WritableStrings) {
      errorNYI(s->getSourceRange(),
               "getGlobalForStringLiteral: mangle string literals");
    }

    // Unlike LLVM IR, CIR doesn't automatically unique names for globals, so
    // we need to do that explicitly.
    std::string uniqueName = getUniqueGlobalName(name.str());
    // Synthetic string literals (e.g., from SourceLocExpr) may not have valid
    // source locations. Use unknown location in those cases.
    mlir::Location loc = s->getBeginLoc().isValid()
                             ? getLoc(s->getSourceRange())
                             : builder.getUnknownLoc();
    auto typedC = llvm::cast<mlir::TypedAttr>(c);
    gv = generateStringLiteral(loc, typedC,
                               cir::GlobalLinkageKind::PrivateLinkage, *this,
                               uniqueName, alignment);
    setDSOLocal(static_cast<mlir::Operation *>(gv));
    constantStringMap[c] = gv;

    assert(!cir::MissingFeatures::sanitizers());
  }
  return gv;
}

/// Return a pointer to a constant array for the given string literal.
cir::GlobalViewAttr
CIRGenModule::getAddrOfConstantStringFromLiteral(const StringLiteral *s,
                                                 StringRef name) {
  cir::GlobalOp gv = getGlobalForStringLiteral(s, name);
  auto arrayTy = mlir::dyn_cast<cir::ArrayType>(gv.getSymType());
  assert(arrayTy && "String literal must be array");
  assert(!cir::MissingFeatures::addressSpace());
  cir::PointerType ptrTy = getBuilder().getPointerTo(arrayTy.getElementType());

  return builder.getGlobalViewAttr(ptrTy, gv);
}

// TODO(cir): this could be a common AST helper for both CIR and LLVM codegen.
LangAS CIRGenModule::getLangTempAllocaAddressSpace() const {
  if (getLangOpts().OpenCL)
    return LangAS::opencl_private;

  // For temporaries inside functions, CUDA treats them as normal variables.
  // LangAS::cuda_device, on the other hand, is reserved for those variables
  // explicitly marked with __device__.
  if (getLangOpts().CUDAIsDevice)
    return LangAS::Default;

  if (getLangOpts().OpenMP && getLangOpts().OpenMPIsTargetDevice)
    assert(!cir::MissingFeatures::openMP());
  if (getLangOpts().SYCLIsDevice)
    errorNYI("SYCL temp address space");

  return LangAS::Default;
}

void CIRGenModule::emitExplicitCastExprType(const ExplicitCastExpr *e,
                                            CIRGenFunction *cgf) {
  if (cgf && e->getType()->isVariablyModifiedType())
    cgf->emitVariablyModifiedType(e->getType());

  assert(!cir::MissingFeatures::generateDebugInfo() &&
         "emitExplicitCastExprType");
}
static const RecordDecl *getCastEndpointRecord(QualType type) {
  while (true) {
    type = type.getCanonicalType();
    if (type->isPointerType() || type->isReferenceType()) {
      type = type->getPointeeType();
      continue;
    }
    if (const auto *array = type->getAsArrayTypeUnsafe()) {
      type = array->getElementType();
      continue;
    }
    const RecordDecl *record = nullptr;
    if (const auto *tag = type->getAs<TagType>())
      record = dyn_cast<RecordDecl>(tag->getDecl());
    else
      record = type->getAsRecordDecl();
    if (!record)
      return nullptr;
    record = cast<RecordDecl>(record->getCanonicalDecl());
    return record->getDefinition() ? record->getDefinition() : record;
  }
}

std::optional<CIRGenModule::ExactRecordEndpoint>
CIRGenModule::getExactRecordEndpoint(QualType type) {
  const RecordDecl *record = getCastEndpointRecord(type);
  if (!record)
    return std::nullopt;

  // Conversion can instantiate a previously incomplete specialization.
  // Resolve the endpoint again afterwards so the schema and identity are both
  // taken from the same final RecordDecl.
  (void)getTypes().convertRecordDeclType(record);
  record = getCastEndpointRecord(type);
  auto schema =
      dyn_cast<cir::RecordType>(getTypes().convertRecordDeclType(record));
  mlir::StringAttr identity = getRecordUSRAttr(record);
  if (!schema || !schema.getName() || !identity)
    return std::nullopt;

  addRecordDeclIdentity(schema.getName(), identity);
  return ExactRecordEndpoint{schema, identity, record};
}


mlir::StringAttr CIRGenModule::getRecordUSRAttr(const RecordDecl *record) {
  if (!record)
    return {};
  record = cast<RecordDecl>(record->getCanonicalDecl());
  // Cast/conditional endpoint identity is authoritative even when CIR erases
  // the endpoint behind void storage or only retains it inside a nested
  // template argument. Materialize this exact canonical RecordDecl through the
  // ordinary CIR type owner so its name, identity, empty-schema fact, and ABI
  // layout travel together in the module metadata.
  mlir::Type schemaType = getTypes().convertRecordDeclType(record);
  exactRecordDeclByCIRType[schemaType] = record;
  // An implicit specialization can be named by a pointer while it is still
  // incomplete and acquire its ODR/argument-owner identity only when a later
  // use instantiates the definition. Never cache that provisional identity.
  const RecordDecl *definition = record->getDefinition();
  const bool identityIsFinal = definition && definition->isCompleteDefinition();
  if (identityIsFinal) {
    if (auto cached = recordUSRCache.find(record);
        cached != recordUSRCache.end())
      return cached->second;
  }
  std::optional<std::string> identity = recordDeclIdentity(*this, record);
  if (!identity.has_value() || identity->empty())
    return {};
  mlir::StringAttr result = builder.getStringAttr(*identity);
  if (identityIsFinal)
    recordUSRCache[record] = result;
  return result;
}

void CIRGenModule::refreshExactRecordOperationIdentities() {
  auto refreshDictionary =
      [&](mlir::DictionaryAttr dictionary,
          llvm::ArrayRef<std::pair<llvm::StringRef, llvm::StringRef>>
              endpointKeys) -> mlir::DictionaryAttr {
    mlir::NamedAttrList refreshed(dictionary);
    for (const auto &[schemaKey, usrKey] : endpointKeys) {
      auto schema = dictionary.getAs<mlir::TypeAttr>(schemaKey);
      if (!schema)
        continue;
      auto record = exactRecordDeclByCIRType.find(schema.getValue());
      if (record == exactRecordDeclByCIRType.end())
        continue;
      std::optional<std::string> identity =
          recordDeclIdentity(*this, record->second);
      if (identity.has_value() && !identity->empty())
        refreshed.set(usrKey, builder.getStringAttr(*identity));
    }
    return refreshed.getDictionary(&getMLIRContext());
  };

  theModule.walk([&](mlir::Operation *operation) {
    // Member operations can be cloned after their exact FieldDecl was
    // recorded. Reconcile the redundant source-location copy from the
    // operation's final direct identity so a clone cannot retain provisional
    // template-instantiation metadata.
    const auto directMember =
        operation->getAttrOfType<mlir::StringAttr>("ast_member_decl_usr");
    const auto directRecord =
        operation->getAttrOfType<mlir::StringAttr>("ast_declaring_record_usr");
    if (directMember && directRecord) {
      if (auto fused = dyn_cast<mlir::FusedLoc>(operation->getLoc())) {
        if (auto metadata =
                dyn_cast_or_null<mlir::DictionaryAttr>(fused.getMetadata())) {
          if (metadata.contains("ast_member_decl_usr") ||
              metadata.contains("ast_declaring_record_usr")) {
            mlir::NamedAttrList refreshedMetadata(metadata);
            refreshedMetadata.set("ast_member_decl_usr", directMember);
            refreshedMetadata.set("ast_declaring_record_usr", directRecord);
            operation->setLoc(mlir::FusedLoc::get(
                fused.getLocations(),
                refreshedMetadata.getDictionary(&getMLIRContext()),
                &getMLIRContext()));
          }
        }
      }
    }
    if (auto classAddr = classAddrIdentityDeclsByOperation.find(operation);
        classAddr != classAddrIdentityDeclsByOperation.end()) {
      const RecordDecl *derived = classAddr->second.first;
      const RecordDecl *base = classAddr->second.second;
      const mlir::StringAttr derivedUSR = getRecordUSRAttr(derived);
      const mlir::StringAttr baseUSR = getRecordUSRAttr(base);
      if (derivedUSR && baseUSR) {
        operation->setAttr("ast_derived_record_usr", derivedUSR);
        operation->setAttr("ast_base_record_usr", baseUSR);
        const ASTRecordLayout &derivedLayout =
            getASTContext().getASTRecordLayout(derived);
        const ASTRecordLayout &baseLayout =
            getASTContext().getASTRecordLayout(base);
        operation->setAttr(
            "ast_derived_record_size_bytes",
            builder.getI64IntegerAttr(derivedLayout.getSize().getQuantity()));
        operation->setAttr(
            "ast_derived_record_align_bytes",
            builder.getI64IntegerAttr(
                derivedLayout.getAlignment().getQuantity()));
        operation->setAttr(
            "ast_base_record_size_bytes",
            builder.getI64IntegerAttr(baseLayout.getSize().getQuantity()));
        operation->setAttr(
            "ast_base_record_align_bytes",
            builder.getI64IntegerAttr(baseLayout.getAlignment().getQuantity()));
      }
    }
    if (auto specialization =
            specializationIdentityDeclByOperation.find(operation);
        specialization != specializationIdentityDeclByOperation.end()) {
      if (auto identity = operation->getAttrOfType<mlir::DictionaryAttr>(
              "ast_decl_specialization_identity")) {
        const FunctionDecl *instantiation = specialization->second;
        const FunctionDecl *pattern =
            instantiation ? instantiation->getTemplateInstantiationPattern()
                          : nullptr;
        llvm::SmallString<256> patternUSR;
        if (pattern && pattern != instantiation &&
            !clang::index::generateUSRForDecl(pattern->getCanonicalDecl(),
                                              patternUSR)) {
          mlir::NamedAttrList refreshedIdentity(identity);
          refreshedIdentity.set("template_pattern_usr",
                                builder.getStringAttr(patternUSR));
          if (auto poi = sourceLocationIdentity(
                  getASTContext(), instantiation->getPointOfInstantiation()))
            refreshedIdentity.set("poi", builder.getStringAttr(*poi));
          operation->setAttr(
              "ast_decl_specialization_identity",
              refreshedIdentity.getDictionary(&getMLIRContext()));
        }
      }
    }
    if (auto field = exactFieldDeclByOperation.find(operation);
        field != exactFieldDeclByOperation.end()) {
      const FieldDecl *declaration = field->second;
      const std::optional<std::string> fieldID =
          fieldDeclIdentity(*this, declaration);
      const mlir::StringAttr ownerUSR =
          getRecordUSRAttr(declaration->getParent());
      if (fieldID.has_value() && !fieldID->empty() && ownerUSR) {
        operation->setAttr("ast_member_decl_usr",
                           builder.getStringAttr(*fieldID));
        operation->setAttr("ast_declaring_record_usr", ownerUSR);
        if (auto fused = dyn_cast<mlir::FusedLoc>(operation->getLoc())) {
          if (auto metadata =
                  dyn_cast_or_null<mlir::DictionaryAttr>(fused.getMetadata())) {
            mlir::NamedAttrList refreshedMetadata(metadata);
            refreshedMetadata.set("ast_member_decl_usr",
                                  builder.getStringAttr(*fieldID));
            refreshedMetadata.set("ast_declaring_record_usr", ownerUSR);
            operation->setLoc(mlir::FusedLoc::get(
                fused.getLocations(),
                refreshedMetadata.getDictionary(&getMLIRContext()),
                &getMLIRContext()));
          }
        }
      }
      if (auto endpoint = operation->getAttrOfType<mlir::DictionaryAttr>(
              "ast_member_record_endpoint")) {
        if (fieldID.has_value() && !fieldID->empty() && ownerUSR) {
          mlir::NamedAttrList refreshedEndpoint(endpoint);
          refreshedEndpoint.set("declaring_record_usr", ownerUSR);
          refreshedEndpoint.set("field_decl_usr",
                                builder.getStringAttr(*fieldID));
          operation->setAttr(
              "ast_member_record_endpoint",
              refreshedEndpoint.getDictionary(&getMLIRContext()));
        }
      }
    }
    if (auto cast =
            operation->getAttrOfType<mlir::DictionaryAttr>("ast_cast_expr")) {
      mlir::DictionaryAttr refreshedCast = refreshDictionary(
          cast, {{"source_record_schema", "source_record_usr"},
                 {"result_record_schema", "result_record_usr"}});
      mlir::NamedAttrList concreteCast(refreshedCast);
      auto refreshConcreteDependentEndpoint = [&](llvm::StringRef prefix,
                                                  mlir::Type type) {
        const std::string presenceKey = (prefix + "_record_presence").str();
        auto presence =
            dyn_cast_or_null<mlir::StringAttr>(concreteCast.get(presenceKey));
        if (!presence || presence.getValue() != "dependent")
          return;
        while (auto pointer = dyn_cast<cir::PointerType>(type))
          type = pointer.getPointee();
        auto schema = dyn_cast<cir::RecordType>(type);
        if (!schema)
          return;
        auto exact = exactRecordDeclByCIRType.find(schema);
        if (exact == exactRecordDeclByCIRType.end())
          return;
        std::optional<std::string> identity =
            recordDeclIdentity(*this, exact->second);
        if (!identity.has_value() || identity->empty())
          return;
        concreteCast.set(presenceKey, builder.getStringAttr("record"));
        concreteCast.set((prefix + "_record_schema").str(),
                         mlir::TypeAttr::get(schema));
        concreteCast.set((prefix + "_record_usr").str(),
                         builder.getStringAttr(*identity));
      };
      if (operation->getNumOperands() != 0)
        refreshConcreteDependentEndpoint("source",
                                         operation->getOperand(0).getType());
      if (operation->getNumResults() != 0)
        refreshConcreteDependentEndpoint("result",
                                         operation->getResult(0).getType());
      auto refreshEndpointLayout = [&](llvm::StringRef prefix) {
        auto schemaAttr = dyn_cast_or_null<mlir::TypeAttr>(
            concreteCast.get((prefix + "_record_schema").str()));
        auto schema = schemaAttr
                          ? dyn_cast<cir::RecordType>(schemaAttr.getValue())
                          : cir::RecordType{};
        if (!schema)
          return;
        auto exact = exactRecordDeclByCIRType.find(schema);
        if (exact == exactRecordDeclByCIRType.end())
          return;
        const RecordDecl *definition = exact->second->getDefinition();
        if (!definition)
          return;
        const ASTRecordLayout &layout =
            getASTContext().getASTRecordLayout(definition);
        concreteCast.set(
            (prefix + "_record_size_bytes").str(),
            builder.getI64IntegerAttr(layout.getSize().getQuantity()));
        concreteCast.set(
            (prefix + "_record_align_bytes").str(),
            builder.getI64IntegerAttr(layout.getAlignment().getQuantity()));
      };
      refreshEndpointLayout("source");
      refreshEndpointLayout("result");
      refreshedCast = concreteCast.getDictionary(&getMLIRContext());
      operation->setAttr("ast_cast_expr", refreshedCast);
      auto sourceUSR =
          refreshedCast.getAs<mlir::StringAttr>("source_record_usr");
      auto resultUSR =
          refreshedCast.getAs<mlir::StringAttr>("result_record_usr");
      if (sourceUSR && resultUSR) {
        if (llvm::isa<cir::BaseClassAddrOp>(operation)) {
          operation->setAttr("ast_derived_record_usr", sourceUSR);
          operation->setAttr("ast_base_record_usr", resultUSR);
        } else if (llvm::isa<cir::DerivedClassAddrOp>(operation)) {
          operation->setAttr("ast_base_record_usr", sourceUSR);
          operation->setAttr("ast_derived_record_usr", resultUSR);
        }
      }
    }
    // Object-storage, array-allocation, member-endpoint, conditional, and
    // copy schemas mint their record identity when the operation is created.
    // Selected-decl emission can complete a specialization afterwards, so
    // reconcile each dictionary from the final exact RecordDecl exactly as
    // addRecordDeclIdentity keeps the module map synchronized.
    if (auto storage = operation->getAttrOfType<mlir::DictionaryAttr>(
            "ast_object_storage")) {
      operation->setAttr(
          "ast_object_storage",
          refreshDictionary(storage, {{"record_schema", "record_usr"}}));
    }
    if (auto arrayAllocation = operation->getAttrOfType<mlir::DictionaryAttr>(
            "ast_object_array_allocation")) {
      operation->setAttr(
          "ast_object_array_allocation",
          refreshDictionary(arrayAllocation,
                            {{"element_record_schema", "element_record_usr"}}));
    }
    if (auto memberEndpoint = operation->getAttrOfType<mlir::DictionaryAttr>(
            "ast_member_record_endpoint")) {
      operation->setAttr(
          "ast_member_record_endpoint",
          refreshDictionary(memberEndpoint, {{"record_schema", "record_usr"}}));
    }
    if (auto conditional = operation->getAttrOfType<mlir::DictionaryAttr>(
            "ast_conditional_expr")) {
      operation->setAttr(
          "ast_conditional_expr",
          refreshDictionary(
              conditional,
              {{"result_record_schema", "result_record_usr"},
               {"destination_record_schema", "destination_record_usr"}}));
    }
    if (auto copySchema = operation->getAttrOfType<mlir::DictionaryAttr>(
            "ast_copy_schema")) {
      operation->setAttr(
          "ast_copy_schema",
          refreshDictionary(
              copySchema,
              {{"source_record_schema", "source_record_usr"},
               {"destination_record_schema", "destination_record_usr"},
               {"replacement_record_schema", "replacement_record_usr"}}));
    }
    auto temporaryIdentities = operation->getAttrOfType<mlir::ArrayAttr>(
        "ast_temporary_object_identities");
    if (!temporaryIdentities)
      return;
    llvm::SmallVector<mlir::Attribute> refreshedIdentities;
    refreshedIdentities.reserve(temporaryIdentities.size());
    for (mlir::Attribute attribute : temporaryIdentities) {
      auto identity = mlir::dyn_cast<mlir::DictionaryAttr>(attribute);
      refreshedIdentities.push_back(
          identity ? refreshDictionary(identity, {{"constructor_record_schema",
                                                   "constructor_record_usr"}})
                   : attribute);
    }
    operation->setAttr("ast_temporary_object_identities",
                       builder.getArrayAttr(refreshedIdentities));
  });
  // A module map entry minted while its specialization was provisional and
  // never re-derived would publish a stale identity for the CIR record name.
  // Recompute every entry from the exact RecordDecl that owns the schema.
  llvm::DenseMap<mlir::StringAttr, const RecordDecl *> recordsBySchemaName;
  for (const auto &[type, record] : exactRecordDeclByCIRType)
    if (auto recordType = dyn_cast<cir::RecordType>(type))
      if (recordType.getName())
        recordsBySchemaName[recordType.getName()] = record;
  // Recomputing an identity can materialize a nested specialization's record
  // type, and that path appends to recordDeclIdentityEntries. Snapshot the
  // names first, compute every identity, then write the results back: holding
  // a reference into the vector across recordDeclIdentity would dangle the
  // moment it grows.
  llvm::SmallVector<mlir::StringAttr> refreshNames;
  refreshNames.reserve(recordDeclIdentityEntries.size());
  for (const mlir::NamedAttribute &entry : recordDeclIdentityEntries)
    if (recordsBySchemaName.contains(entry.getName()))
      refreshNames.push_back(entry.getName());
  llvm::DenseMap<mlir::StringAttr, mlir::StringAttr> refreshedIdentityByName;
  for (mlir::StringAttr name : refreshNames) {
    auto found = recordsBySchemaName.find(name);
    if (found == recordsBySchemaName.end())
      continue;
    std::optional<std::string> identity =
        recordDeclIdentity(*this, found->second);
    if (identity.has_value() && !identity->empty())
      refreshedIdentityByName[name] = builder.getStringAttr(*identity);
  }
  for (mlir::NamedAttribute &entry : recordDeclIdentityEntries) {
    auto refreshed = refreshedIdentityByName.find(entry.getName());
    if (refreshed != refreshedIdentityByName.end())
      entry = mlir::NamedAttribute(entry.getName(), refreshed->second);
  }
}

mlir::ArrayAttr CIRGenModule::buildCastEndpointSourceType(QualType type) {
  if (auto cached = castEndpointSourceTypeCache.find(type);
      cached != castEndpointSourceTypeCache.end())
    return cached->second;
  const QualType cacheKey = type;
  CIRGenBuilderTy &builder = getBuilder();
  llvm::SmallVector<mlir::Attribute, 4> layers;
  while (true) {
    mlir::NamedAttrList layer;
    StringRef kind = "value";
    bool referenceStorage = false;
    if (type->isLValueReferenceType()) {
      kind = "lvalue_reference";
      referenceStorage = true;
    } else if (type->isRValueReferenceType()) {
      kind = "rvalue_reference";
      referenceStorage = true;
    } else if (type->isPointerType()) {
      kind = "pointer";
    }
    layer.set("kind", builder.getStringAttr(kind));
    const Qualifiers qualifiers = type.getQualifiers();
    layer.set("is_const", builder.getBoolAttr(qualifiers.hasConst()));
    layer.set("is_volatile", builder.getBoolAttr(qualifiers.hasVolatile()));
    layer.set("is_restrict", builder.getBoolAttr(qualifiers.hasRestrict()));
    layer.set("is_atomic", builder.getBoolAttr(type->isAtomicType()));
    layer.set("clang_address_space",
              builder.getI64IntegerAttr(
                  static_cast<uint64_t>(qualifiers.getAddressSpace())));
    layer.set("target_address_space",
              builder.getI64IntegerAttr(getASTContext().getTargetAddressSpace(
                  qualifiers.getAddressSpace())));
    QualType layoutType = referenceStorage ? getASTContext().VoidPtrTy : type;
    if (!layoutType->isIncompleteType() && !layoutType->isFunctionType() &&
        !layoutType->isVoidType()) {
      const TypeInfo info = getASTContext().getTypeInfo(layoutType);
      layer.set("bit_width", builder.getI64IntegerAttr(info.Width));
      layer.set("align_bits", builder.getI64IntegerAttr(info.Align));
    }
    if (type->isIntegerType() || type->isEnumeralType())
      layer.set("is_signed",
                builder.getBoolAttr(type->isSignedIntegerOrEnumerationType()));
    layers.push_back(layer.getDictionary(&getMLIRContext()));
    if (type->isPointerType() || type->isReferenceType()) {
      type = type->getPointeeType();
      continue;
    }
    break;
  }
  mlir::ArrayAttr result = builder.getArrayAttr(layers);
  castEndpointSourceTypeCache.try_emplace(cacheKey, result);
  return result;
}
mlir::StringAttr CIRGenModule::getSourceTypeSpelling(QualType type) {
  auto [cached, inserted] = sourceTypeSpellingCache.try_emplace(type);
  if (inserted)
    cached->second = builder.getStringAttr(type.getAsString());
  return cached->second;
}

void CIRGenModule::setFieldEndpointMetadata(mlir::Operation *op,
                                            const FieldDecl *field) {
  if (!op || !field)
    return;

  const bool recordValued = getCastEndpointRecord(field->getType()) != nullptr;
  const std::optional<std::string> fieldID = fieldDeclIdentity(*this, field);
  const RecordDecl *owner = field->getParent();
  const mlir::StringAttr ownerUSR = getRecordUSRAttr(owner);
  if (!fieldID.has_value() || fieldID->empty() || !ownerUSR) {
    if (recordValued) {
      const std::string detail =
          "record field has no exact FieldDecl identity: field=" +
          field->getQualifiedNameAsString() +
          ", type=" + field->getType().getAsString() +
          (fieldID.has_value() && !fieldID->empty() ? ", field_usr=exact"
                                                    : ", field_usr=missing") +
          (ownerUSR ? ", owner_usr=exact" : ", owner_usr=missing");
      errorNYI(op->getLoc(), detail);
    }
    return;
  }

  op->setAttr("ast_member_decl_usr", builder.getStringAttr(*fieldID));
  op->setAttr("ast_declaring_record_usr", ownerUSR);
  exactFieldDeclByOperation[op] = field->getCanonicalDecl();
  op->setAttr("ast_member_offset_bits",
              builder.getI64IntegerAttr(
                  static_cast<int64_t>(getASTContext().getFieldOffset(field))));

  // Scalar fields need only their exact FieldDecl owner above. For a field
  // whose terminal AST endpoint is a record, also carry the endpoint's exact
  // CIR schema. This pair remains authoritative when a return slot,
  // temporary, or ABI transform retains the field address but rewrites its
  // surrounding record type.
  if (!recordValued)
    return;
  std::optional<ExactRecordEndpoint> recordEndpoint =
      getExactRecordEndpoint(field->getType());
  mlir::Type storageType;
  if (op->getNumResults() == 1) {
    if (auto resultPointer =
            dyn_cast<cir::PointerType>(op->getResult(0).getType()))
      storageType = resultPointer.getPointee();
  }
  if (!recordEndpoint || !storageType) {
    const std::string detail =
        "record field has no exact endpoint schema: field=" +
        field->getQualifiedNameAsString() +
        ", type=" + field->getType().getAsString() +
        ", op=" + op->getName().getStringRef().str() +
        (recordEndpoint ? ", record_endpoint=exact"
                        : ", record_endpoint=missing") +
        (storageType ? ", storage_type=exact" : ", storage_type=missing");
    errorNYI(op->getLoc(), detail);
    return;
  }

  mlir::NamedAttrList endpoint;
  endpoint.set("declaring_record_usr", ownerUSR);
  endpoint.set("field_decl_usr", builder.getStringAttr(*fieldID));
  endpoint.set("field_offset_bits",
               builder.getI64IntegerAttr(static_cast<int64_t>(
                   getASTContext().getFieldOffset(field))));
  endpoint.set("field_storage_type", mlir::TypeAttr::get(storageType));
  endpoint.set("record_schema", mlir::TypeAttr::get(recordEndpoint->schema));
  endpoint.set("record_usr", recordEndpoint->identity);
  endpoint.set("source_type", buildCastEndpointSourceType(field->getType()));
  op->setAttr("ast_member_record_endpoint",
              endpoint.getDictionary(&getMLIRContext()));
}

void CIRGenModule::setObjectStorageMetadata(mlir::Operation *op,
                                            QualType type) {
  if (!op || type.isNull() || op->hasAttr("ast_object_storage"))
    return;
  // MaterializeTemporaryExpr also wraps reference and pointer expressions.
  // Their alloca stores the pointer value; the pointee RecordDecl is not the
  // storage object's schema.
  if (!type.getCanonicalType()->isRecordType())
    return;
  std::optional<ExactRecordEndpoint> exact = getExactRecordEndpoint(type);
  if (!exact)
    return;

  mlir::NamedAttrList identity;
  identity.set("record_usr", exact->identity);
  identity.set("record_schema", mlir::TypeAttr::get(exact->schema));
  identity.set("source_type", buildCastEndpointSourceType(type));
  op->setAttr("ast_object_storage", identity.getDictionary(&getMLIRContext()));
}

void CIRGenModule::setObjectArrayAllocationMetadata(mlir::Operation *op,
                                                    QualType type) {
  if (!op || type.isNull() || op->hasAttr("ast_object_array_allocation"))
    return;

  QualType elementType = type.getCanonicalType();
  llvm::SmallVector<mlir::Attribute, 2> extents;
  while (const ArrayType *array = getASTContext().getAsArrayType(elementType)) {
    const auto *constant = dyn_cast<ConstantArrayType>(array);
    if (!constant)
      return;
    const llvm::APInt &extent = constant->getSize();
    // A GNU zero-length array is still an exact fixed array. Preserve its
    // zero extent as provenance; it describes storage with no element
    // lifetimes.
    if (extent.getActiveBits() > 63) {
      errorNYI(op->getLoc(),
               "object-array alloca extent is not representable in metadata");
      return;
    }
    extents.push_back(
        builder.getI64IntegerAttr(static_cast<int64_t>(extent.getZExtValue())));
    elementType = array->getElementType().getCanonicalType();
  }
  if (extents.empty())
    return;

  const RecordDecl *record = elementType->getAsRecordDecl();
  if (!record)
    return;
  if (const RecordDecl *definition = record->getDefinition())
    record = definition;
  mlir::StringAttr recordUSR = getRecordUSRAttr(record);
  if (!recordUSR) {
    errorNYI(op->getLoc(),
             "object-array alloca element has no exact RecordDecl identity");
    return;
  }
  auto recordSchema =
      dyn_cast<cir::RecordType>(getTypes().convertRecordDeclType(record));
  if (!recordSchema) {
    errorNYI(op->getLoc(),
             "object-array alloca element has no exact CIR record schema");
    return;
  }

  mlir::NamedAttrList identity;
  identity.set("array_extents", builder.getArrayAttr(extents));
  identity.set("element_record_usr", recordUSR);
  identity.set("element_record_schema", mlir::TypeAttr::get(recordSchema));
  identity.set("source_type", buildCastEndpointSourceType(type));
  op->setAttr("ast_object_array_allocation",
              identity.getDictionary(&getMLIRContext()));
}

static StringRef getCastEndpointRecordPresence(QualType type) {
  if (type->isDependentType() || type->isInstantiationDependentType())
    return "dependent";
  return getCastEndpointRecord(type) ? "record" : "no_record";
}

void CIRGenModule::setCastExprMetadata(mlir::Operation *op, const CastExpr *e) {
  if (!op || !e || op->hasAttr("ast_cast_expr"))
    return;
  mlir::NamedAttrList identity;
  identity.set("cast_kind", builder.getStringAttr(e->getCastKindName()));
  identity.set("is_explicit", builder.getBoolAttr(isa<ExplicitCastExpr>(e)));
  identity.set(
      "is_part_of_explicit_cast",
      builder.getBoolAttr(isa<ExplicitCastExpr>(e) ||
                          (isa<ImplicitCastExpr>(e) &&
                           cast<ImplicitCastExpr>(e)->isPartOfExplicitCast())));
  identity.set("source_type",
               buildCastEndpointSourceType(e->getSubExpr()->getType()));
  identity.set("result_type", buildCastEndpointSourceType(e->getType()));
  const StringRef sourceRecordPresence =
      getCastEndpointRecordPresence(e->getSubExpr()->getType());
  const StringRef resultRecordPresence =
      getCastEndpointRecordPresence(e->getType());
  identity.set("source_record_presence",
               builder.getStringAttr(sourceRecordPresence));
  identity.set("result_record_presence",
               builder.getStringAttr(resultRecordPresence));
  identity.set("source_type_spelling",
               getSourceTypeSpelling(e->getSubExpr()->getType()));
  identity.set("result_type_spelling", getSourceTypeSpelling(e->getType()));
  // AST endpoints own record identity; the materialized CIR operation may
  // wrap the record in an array, erase it behind a void pointer, or retain
  // only an enclosing template aggregate. Carry the exact CIR RecordType
  // schema together with its canonical RecordDecl identity so consumers never
  // have to reconstruct an endpoint schema from the destination spelling.
  auto setRecordEndpoint = [&](StringRef endpoint, StringRef usrKey,
                               StringRef schemaKey, QualType type) -> bool {
    std::optional<ExactRecordEndpoint> exact = getExactRecordEndpoint(type);
    if (!exact) {
      std::string message = "cast ";
      message.append(endpoint);
      message.append(
          " endpoint has no exact RecordDecl identity and CIR schema");
      errorNYI(op->getLoc(), message);
      return false;
    }
    identity.set(usrKey, exact->identity);
    identity.set(schemaKey, mlir::TypeAttr::get(exact->schema));
    if (exact->record->isCompleteDefinition()) {
      const ASTRecordLayout &layout =
          getASTContext().getASTRecordLayout(exact->record);
      identity.set((endpoint + "_record_size_bytes").str(),
                   builder.getI64IntegerAttr(layout.getSize().getQuantity()));
      identity.set(
          (endpoint + "_record_align_bytes").str(),
          builder.getI64IntegerAttr(layout.getAlignment().getQuantity()));
    }
    return true;
  };
  if (sourceRecordPresence == "record" &&
      !setRecordEndpoint("source", "source_record_usr", "source_record_schema",
                         e->getSubExpr()->getType()))
    return;
  if (resultRecordPresence == "record" &&
      !setRecordEndpoint("result", "result_record_usr", "result_record_schema",
                         e->getType()))
    return;
  op->setAttr("ast_cast_expr", identity.getDictionary(&getMLIRContext()));
}

void CIRGenModule::setConditionalExprMetadata(
    mlir::Operation *op, const AbstractConditionalOperator *e,
    mlir::Value aggregateDestination,
    llvm::StringRef aggregateDestinationInstanceToken) {
  if (!op || !e || op->hasAttr("ast_conditional_expr"))
    return;
  mlir::NamedAttrList identity;
  identity.set("result_type", buildCastEndpointSourceType(e->getType()));
  std::optional<ExactRecordEndpoint> exact = getExactRecordEndpoint(e->getType());
  if (exact) {
    identity.set("result_record_usr", exact->identity);
    identity.set("result_record_schema", mlir::TypeAttr::get(exact->schema));
  }

  if (aggregateDestination) {
    if (aggregateDestinationInstanceToken.empty()) {
      errorNYI(op->getLoc(),
               "aggregate conditional destination has no exact producer "
               "instance token");
      return;
    }
    if (!exact) {
      errorNYI(op->getLoc(),
               "aggregate conditional destination has no exact AST "
               "RecordDecl identity and CIR schema");
      return;
    }
    identity.set("destination_instance_token",
                 builder.getStringAttr(
                     aggregateDestinationInstanceToken));
    identity.set("destination_type",
                 buildCastEndpointSourceType(e->getType()));
    identity.set("destination_record_usr", exact->identity);
    identity.set("destination_record_schema",
                 mlir::TypeAttr::get(exact->schema));
    mlir::Operation *destinationProducer =
        aggregateDestination.getDefiningOp();
    if (destinationProducer) {
      auto result = mlir::cast<mlir::OpResult>(aggregateDestination);
      identity.set("destination_value_kind",
                   builder.getStringAttr("operation_result"));
      identity.set("destination_result_index",
                   builder.getI64IntegerAttr(result.getResultNumber()));
      llvm::SmallVector<mlir::Attribute> destinationIdentities;
      if (auto existing =
              destinationProducer->getAttrOfType<mlir::ArrayAttr>(
                  "ast_conditional_destination_identities")) {
        destinationIdentities.append(existing.begin(), existing.end());
      }
      destinationIdentities.push_back(
          builder.getStringAttr(aggregateDestinationInstanceToken));
      destinationProducer->setAttr(
          "ast_conditional_destination_identities",
          builder.getArrayAttr(destinationIdentities));
    } else {
      auto argument = mlir::dyn_cast<mlir::BlockArgument>(
          aggregateDestination);
      auto function =
          argument
              ? mlir::dyn_cast<cir::FuncOp>(
                    argument.getOwner()->getParentOp())
              : cir::FuncOp{};
      if (!argument || !function) {
        errorNYI(op->getLoc(),
                 "aggregate conditional destination has no exact CIR value "
                 "owner");
        return;
      }
      identity.set("destination_value_kind",
                   builder.getStringAttr("function_argument"));
      identity.set("destination_argument_index",
                   builder.getI64IntegerAttr(argument.getArgNumber()));
      identity.set("destination_function",
                   mlir::FlatSymbolRefAttr::get(function.getNameAttr()));
    }
    mlir::StringRef storageKind = "object_storage";
    bool requiresTypedSlot = false;
    auto alloca = destinationProducer
                      ? mlir::dyn_cast<cir::AllocaOp>(destinationProducer)
                      : cir::AllocaOp{};
    if (alloca) {
      requiresTypedSlot = true;
      storageKind = "direct_alloca";
    }
    identity.set("destination_storage_kind",
                 builder.getStringAttr(storageKind));
    identity.set("destination_requires_typed_slot",
                 builder.getBoolAttr(requiresTypedSlot));
    auto markArmDestination = [&](mlir::Value armDestination) {
      if (mlir::Operation *producer = armDestination.getDefiningOp()) {
        llvm::SmallVector<mlir::Attribute> identities;
        if (auto existing = producer->getAttrOfType<mlir::ArrayAttr>(
                "ast_conditional_arm_destination_identities")) {
          identities.append(existing.begin(), existing.end());
        }
        mlir::StringAttr token =
            builder.getStringAttr(aggregateDestinationInstanceToken);
        if (!llvm::is_contained(identities, token))
          identities.push_back(token);
        producer->setAttr("ast_conditional_arm_destination_identities",
                          builder.getArrayAttr(identities));
        return true;
      }
      return armDestination == aggregateDestination;
    };


    auto markArm = [&](mlir::Region &region) -> bool {
      if (region.empty() || std::next(region.begin()) != region.end() ||
          region.front().empty())
        return false;
      mlir::Operation *terminator = &region.front().back();
      mlir::Operation *final = terminator->getPrevNode();
      if (!mlir::isa<cir::YieldOp>(terminator) || !final)
        return false;
      mlir::Value armDestination;
      if (auto store = mlir::dyn_cast<cir::StoreOp>(final)) {
        armDestination = store.getAddr();
      } else if (auto call = mlir::dyn_cast<cir::CallOp>(final);
                 call && !call.getArgOperands().empty()) {
        armDestination = call.getArgOperands().front();
      }
      if (!armDestination || !markArmDestination(armDestination))
        return false;
      final->setAttr("ast_conditional_destination_identity",
                     builder.getStringAttr(
                         aggregateDestinationInstanceToken));
      return true;
    };
    auto conditional = mlir::dyn_cast<cir::IfOp>(op);
    if (!conditional || !markArm(conditional.getThenRegion()) ||
        !markArm(conditional.getElseRegion())) {
      errorNYI(op->getLoc(),
               "aggregate conditional destination does not own two exact "
               "linear CIR arms");
      return;
    }
  }
  op->setAttr("ast_conditional_expr",
              identity.getDictionary(&getMLIRContext()));
}

void CIRGenModule::setAggregateCopyMetadata(mlir::Operation *op,
                                            QualType destinationType,
                                            QualType sourceType,
                                            QualType replacementType) {
  if (!op || op->hasAttr("ast_copy_schema"))
    return;
  if (!getCastEndpointRecord(replacementType))
    return;

  mlir::NamedAttrList identity;
  auto setEndpoint = [&](StringRef prefix, QualType type) {
    std::optional<ExactRecordEndpoint> exact = getExactRecordEndpoint(type);
    if (!exact)
      return false;
    identity.set((prefix + "_record_usr").str(), exact->identity);
    identity.set((prefix + "_record_schema").str(),
                 mlir::TypeAttr::get(exact->schema));
    return true;
  };
  const bool sourceExact = setEndpoint("source", sourceType);
  const bool destinationExact = setEndpoint("destination", destinationType);
  const bool replacementExact = setEndpoint("replacement", replacementType);
  if (!sourceExact || !destinationExact || !replacementExact) {
    const std::string detail =
        "aggregate copy has no exact RecordDecl schema: source=" +
        sourceType.getAsString() + (sourceExact ? " [exact]" : " [missing]") +
        ", destination=" + destinationType.getAsString() +
        (destinationExact ? " [exact]" : " [missing]") +
        ", replacement=" + replacementType.getAsString() +
        (replacementExact ? " [exact]" : " [missing]");
    errorNYI(op->getLoc(), detail);
    return;
  }
  op->setAttr("ast_copy_schema", identity.getDictionary(&getMLIRContext()));
}

void CIRGenModule::setMemberPointerTargetMetadata(
    mlir::Operation *op, QualType type,
    const FieldDecl *constantDataMember) {
  if (!op)
    return;
  // Array storage of member pointers is member-pointer storage: every
  // element shares the declared target class and pointee kind, so the
  // annotation describes the element type.
  QualType elementType = getASTContext().getBaseElementType(type);
  const auto *memberPointer =
      elementType.getCanonicalType()->getAs<MemberPointerType>();
  if (!memberPointer)
    return;
  const CXXRecordDecl *record = memberPointer->getMostRecentCXXRecordDecl();
  mlir::StringAttr usr = getRecordUSRAttr(record);
  if (!usr)
    return;
  if (!op->hasAttr("ast_member_pointer_target")) {
    mlir::NamedAttrList identity;
    identity.set("record_usr", usr);
    identity.set("pointee_kind",
                 builder.getStringAttr(memberPointer->isMemberFunctionPointer()
                                           ? "function"
                                           : "data"));
    op->setAttr("ast_member_pointer_target",
                identity.getDictionary(&getMLIRContext()));
  }
  if (memberPointer->isMemberFunctionPointer() ||
      op->hasAttr("ast_data_member_pointer_constant"))
    return;
  auto constant = dyn_cast<cir::ConstantOp>(op);
  auto dataMember = constant
                        ? dyn_cast<cir::DataMemberAttr>(constant.getValue())
                        : cir::DataMemberAttr{};
  if (!dataMember || !record)
    return;
  const TypeInfo carrierInfo =
      getASTContext().getTypeInfo(getASTContext().getPointerDiffType());
  if (!carrierInfo.Width || !carrierInfo.Align ||
      getASTContext().getTargetInfo().getCXXABI().isMicrosoft())
    return;
  mlir::NamedAttrList exact;
  exact.set("kind", builder.getStringAttr("data"));
  exact.set("target_record_usr", usr);
  exact.set("class_type",
            mlir::TypeAttr::get(dataMember.getType().getClassTy()));
  exact.set("pointee_type",
            mlir::TypeAttr::get(dataMember.getType().getMemberTy()));
  exact.set("carrier_bits", builder.getI64IntegerAttr(carrierInfo.Width));
  exact.set("carrier_align_bits", builder.getI64IntegerAttr(carrierInfo.Align));
  exact.set("carrier_signed", builder.getBoolAttr(true));
  exact.set("null_value", builder.getStringAttr("-1"));
  if (dataMember.isNullPtr()) {
    exact.set("provenance_kind", builder.getStringAttr("null"));
  } else {
    if (!constantDataMember)
      return;
    const std::optional<uint64_t> memberIndex = dataMember.getMemberIndex();
    if (!memberIndex ||
        getTypes()
                .getCIRGenRecordLayout(constantDataMember->getParent())
                .getCIRFieldNo(constantDataMember) != *memberIndex)
      return;
    const std::optional<std::string> fieldID =
        fieldDeclIdentity(*this, constantDataMember);
    const mlir::StringAttr fieldOwner =
        getRecordUSRAttr(constantDataMember->getParent());
    if (!fieldID.has_value() || fieldID->empty() || !fieldOwner ||
        fieldOwner != usr)
      return;
    const uint64_t fieldOffset =
        getASTContext()
            .getASTRecordLayout(constantDataMember->getParent())
            .getFieldOffset(constantDataMember->getFieldIndex());
    exact.set("provenance_kind", builder.getStringAttr("field_decl"));
    exact.set("field_decl_id", builder.getStringAttr(*fieldID));
    exact.set("field_declaring_record_usr", fieldOwner);
    exact.set("field_offset_bits", builder.getI64IntegerAttr(fieldOffset));
    exact.set("field_declared_type",
              buildCastEndpointSourceType(constantDataMember->getType()));
    exact.set("field_declared_type_spelling",
              getSourceTypeSpelling(constantDataMember->getType()));
  }
  op->setAttr("ast_data_member_pointer_constant",
              exact.getDictionary(&getMLIRContext()));
}

void CIRGenModule::setBitfieldStoreMetadata(mlir::Operation *op,
                                            QualType sourceType,
                                            QualType fieldType,
                                            mlir::Type storageType) {
  if (!op || sourceType.isNull() || fieldType.isNull() || !storageType)
    return;
  mlir::NamedAttrList exact;
  exact.set("source_declared_type", buildCastEndpointSourceType(sourceType));
  exact.set("source_declared_type_spelling",
            getSourceTypeSpelling(sourceType));
  exact.set("field_declared_type", buildCastEndpointSourceType(fieldType));
  exact.set("field_declared_type_spelling", getSourceTypeSpelling(fieldType));
  exact.set("storage_type", mlir::TypeAttr::get(storageType));
  op->setAttr("ast_bitfield_assignment",
              exact.getDictionary(&getMLIRContext()));
}

mlir::TypedAttr CIRGenModule::emitNullMemberAttr(QualType destTy,
                                                 const MemberPointerType *mpt) {
  if (mpt->isMemberFunctionPointerType()) {
    auto ty = mlir::cast<cir::MethodType>(convertType(destTy));
    return builder.getNullMethodAttr(ty);
  }

  auto ty = mlir::cast<cir::DataMemberType>(convertType(destTy));
  return builder.getNullDataMemberAttr(ty);
}

mlir::Value CIRGenModule::emitMemberPointerConstant(const UnaryOperator *e) {
  assert(!cir::MissingFeatures::cxxABI());

  mlir::Location loc = getLoc(e->getSourceRange());
  const auto *decl = cast<DeclRefExpr>(e->getSubExpr())->getDecl();

  mlir::Value result;
  const auto *methodDecl = dyn_cast<CXXMethodDecl>(decl);
  const auto *dataFieldDecl = dyn_cast<FieldDecl>(decl);
  if (methodDecl) {
    auto ty = mlir::cast<cir::MethodType>(convertType(e->getType()));
    if (methodDecl->isVirtual()) {
      result = cir::ConstantOp::create(
          builder, loc, getCXXABI().buildVirtualMethodAttr(ty, methodDecl));
    } else {
      const CIRGenFunctionInfo &fi =
          getTypes().arrangeCXXMethodDeclaration(methodDecl);
      cir::FuncType funcTy = getTypes().getFunctionType(fi);
      cir::FuncOp methodFuncOp = getAddrOfFunction(methodDecl, funcTy);
      result = cir::ConstantOp::create(builder, loc,
                                       builder.getMethodAttr(ty, methodFuncOp));
    }
  } else {
    auto ty = mlir::cast<cir::DataMemberType>(convertType(e->getType()));
    assert(dataFieldDecl && "data-member pointer must name a FieldDecl");
    const RecordDecl *parent = dataFieldDecl->getParent();
    const unsigned memberIndex =
        getTypes().getCIRGenRecordLayout(parent).getCIRFieldNo(dataFieldDecl);
    result = cir::ConstantOp::create(
        builder, loc, builder.getDataMemberAttr(ty, memberIndex));
  }

  setMemberPointerTargetMetadata(result.getDefiningOp(), e->getType(),
                                 dataFieldDecl);
  if (methodDecl) {
    auto exact = buildCIRGenVirtualMethodIdentityAttrs(
        *this, getMLIRContext(), GlobalDecl(methodDecl));
    mlir::NamedAttrList identity;
    identity.set("kind", builder.getStringAttr("method"));
    identity.set("method_symbol", exact.method);
    identity.set("is_virtual", builder.getBoolAttr(methodDecl->isVirtual()));
    if (exact.methodUSR)
      identity.set("method_usr", exact.methodUSR);
    if (auto declaringClass = getRecordUSRAttr(methodDecl->getParent()))
      identity.set("method_declaring_class_usr", declaringClass);
    if (exact.rootMethodUSR)
      identity.set("virtual_root_method_usr", exact.rootMethodUSR);
    if (exact.declaringClassUSR)
      identity.set("virtual_root_declaring_class_usr", exact.declaringClassUSR);
    if (exact.rootAlternatives)
      identity.set("virtual_root_alternatives", exact.rootAlternatives);
    result.getDefiningOp()->setAttr("ast_member_function_pointer_constant",
                                    identity.getDictionary(&getMLIRContext()));
  }
  return result;
}

static std::optional<mlir::StringAttr> getObjCDeclUSRAttr(CIRGenModule &cgm,
                                                          const Decl *decl) {
  llvm::SmallString<256> usr;
  if (clang::index::generateUSRForDecl(decl, usr))
    return std::nullopt;
  return cgm.getBuilder().getStringAttr(usr);
}

static mlir::DictionaryAttr getObjCSourceTypeFacts(CIRGenModule &cgm,
                                                   QualType type,
                                                   bool &hasCompleteIdentity) {
  mlir::NamedAttrList facts;
  const ASTContext &astContext = cgm.getASTContext();
  const QualType canonicalType = astContext.getCanonicalType(type);
  const Qualifiers qualifiers = type.getQualifiers();

  facts.set("as_written", cgm.getBuilder().getStringAttr(type.getAsString()));
  facts.set("canonical",
            cgm.getBuilder().getStringAttr(canonicalType.getAsString()));
  facts.set("type_class",
            cgm.getBuilder().getStringAttr(type->getTypeClassName()));
  facts.set("is_const", cgm.getBuilder().getBoolAttr(qualifiers.hasConst()));
  facts.set("is_volatile",
            cgm.getBuilder().getBoolAttr(qualifiers.hasVolatile()));
  facts.set("is_restrict",
            cgm.getBuilder().getBoolAttr(qualifiers.hasRestrict()));
  facts.set("is_atomic", cgm.getBuilder().getBoolAttr(type->isAtomicType()));
  facts.set("clang_address_space",
            cgm.getBuilder().getI64IntegerAttr(
                static_cast<uint64_t>(qualifiers.getAddressSpace())));
  facts.set("target_address_space",
            cgm.getBuilder().getI64IntegerAttr(astContext.getTargetAddressSpace(
                qualifiers.getAddressSpace())));
  if (!type->isIncompleteType() && !type->isFunctionType() &&
      !type->isVoidType()) {
    const TypeInfo info = astContext.getTypeInfo(type);
    facts.set("bit_width", cgm.getBuilder().getI64IntegerAttr(info.Width));
    facts.set("align_bits", cgm.getBuilder().getI64IntegerAttr(info.Align));
  }
  if (type->isIntegerType() || type->isEnumeralType())
    facts.set("is_signed", cgm.getBuilder().getBoolAttr(
                               type->isSignedIntegerOrEnumerationType()));

  if (const auto *objectPointer =
          canonicalType->getAs<ObjCObjectPointerType>()) {
    const ObjCObjectType *objectType = objectPointer->getObjectType();
    facts.set("objc_is_id",
              cgm.getBuilder().getBoolAttr(objectPointer->isObjCIdType()));
    facts.set("objc_is_class",
              cgm.getBuilder().getBoolAttr(objectPointer->isObjCClassType()));
    facts.set("objc_is_kindof",
              cgm.getBuilder().getBoolAttr(objectPointer->isKindOfType()));
    if (NullabilityKindOrNone nullability = type->getNullability())
      facts.set("objc_nullability", cgm.getBuilder().getStringAttr(
                                        getNullabilitySpelling(*nullability)));

    if (const ObjCInterfaceDecl *interface =
            objectPointer->getInterfaceDecl()) {
      std::optional<mlir::StringAttr> interfaceUSR =
          getObjCDeclUSRAttr(cgm, interface);
      if (!interfaceUSR) {
        hasCompleteIdentity = false;
        return {};
      }
      facts.set("objc_interface_usr", *interfaceUSR);
    }

    llvm::SmallVector<mlir::Attribute, 4> protocolQualifiers;
    for (const ObjCProtocolDecl *qualifier : objectType->quals()) {
      std::optional<mlir::StringAttr> qualifierUSR =
          getObjCDeclUSRAttr(cgm, qualifier);
      if (!qualifierUSR) {
        hasCompleteIdentity = false;
        return {};
      }
      protocolQualifiers.push_back(*qualifierUSR);
    }
    facts.set("objc_protocol_qualifiers",
              cgm.getBuilder().getArrayAttr(protocolQualifiers));
  }

  return facts.getDictionary(&cgm.getMLIRContext());
}

static std::optional<mlir::DictionaryAttr>
getObjCMethodFacts(CIRGenModule &cgm, const ObjCMethodDecl *method) {
  std::optional<mlir::StringAttr> methodUSR = getObjCDeclUSRAttr(cgm, method);
  if (!methodUSR)
    return std::nullopt;

  bool hasCompleteIdentity = true;
  mlir::DictionaryAttr returnType =
      getObjCSourceTypeFacts(cgm, method->getReturnType(), hasCompleteIdentity);
  llvm::SmallVector<mlir::Attribute, 4> parameterTypes;
  for (const ParmVarDecl *parameter : method->parameters()) {
    mlir::DictionaryAttr parameterType =
        getObjCSourceTypeFacts(cgm, parameter->getType(), hasCompleteIdentity);
    if (!parameterType)
      break;
    parameterTypes.push_back(parameterType);
  }
  if (!hasCompleteIdentity || !returnType ||
      parameterTypes.size() != method->param_size())
    return std::nullopt;

  mlir::NamedAttrList methodFacts;
  methodFacts.set("usr", *methodUSR);
  methodFacts.set("selector", cgm.getBuilder().getStringAttr(
                                  method->getSelector().getAsString()));
  methodFacts.set("return_type", returnType);
  methodFacts.set("parameter_types",
                  cgm.getBuilder().getArrayAttr(parameterTypes));
  methodFacts.set("is_instance",
                  cgm.getBuilder().getBoolAttr(method->isInstanceMethod()));
  methodFacts.set("is_optional",
                  cgm.getBuilder().getBoolAttr(method->isOptional()));
  methodFacts.set("is_variadic",
                  cgm.getBuilder().getBoolAttr(method->isVariadic()));
  methodFacts.set("has_related_result_type",
                  cgm.getBuilder().getBoolAttr(method->hasRelatedResultType()));
  methodFacts.set("is_direct",
                  cgm.getBuilder().getBoolAttr(method->isDirectMethod()));
  methodFacts.set("method_family",
                  cgm.getBuilder().getI64IntegerAttr(
                      static_cast<uint64_t>(method->getMethodFamily())));
  methodFacts.set("objc_decl_qualifier",
                  cgm.getBuilder().getI64IntegerAttr(
                      static_cast<uint64_t>(method->getObjCDeclQualifier())));
  return methodFacts.getDictionary(&cgm.getMLIRContext());
}

void CIRGenModule::emitObjCProtocolDecl(const ObjCProtocolDecl *protocol) {
  std::optional<mlir::StringAttr> protocolUSR =
      getObjCDeclUSRAttr(*this, protocol);
  if (!protocolUSR) {
    errorNYI(protocol->getBeginLoc(),
             "ObjCProtocol definition without a Clang USR");
    return;
  }

  llvm::SmallVector<mlir::Attribute, 4> inheritedProtocols;
  for (const ObjCProtocolDecl *inheritedProtocol : protocol->protocols()) {
    std::optional<mlir::StringAttr> inheritedUSR =
        getObjCDeclUSRAttr(*this, inheritedProtocol);
    if (!inheritedUSR) {
      errorNYI(inheritedProtocol->getBeginLoc(),
               "ObjCProtocol conformance without a Clang USR");
      return;
    }
    inheritedProtocols.push_back(*inheritedUSR);
  }

  llvm::SmallVector<mlir::Attribute, 8> methods;
  for (const ObjCMethodDecl *method : protocol->methods()) {
    std::optional<mlir::DictionaryAttr> methodFacts =
        getObjCMethodFacts(*this, method);
    if (!methodFacts) {
      errorNYI(method->getBeginLoc(),
               "ObjCProtocol method without complete Clang identity");
      return;
    }
    methods.push_back(*methodFacts);
  }

  mlir::NamedAttrList protocolFacts;
  protocolFacts.set("usr", *protocolUSR);
  protocolFacts.set(
      "runtime_name",
      builder.getStringAttr(protocol->getObjCRuntimeNameAsString()));
  protocolFacts.set("is_non_runtime",
                    builder.getBoolAttr(protocol->isNonRuntimeProtocol()));
  protocolFacts.set("inherited_protocol_usrs",
                    builder.getArrayAttr(inheritedProtocols));
  protocolFacts.set("methods", builder.getArrayAttr(methods));
  addObjCProtocol(protocolFacts.getDictionary(&getMLIRContext()));
}

static std::optional<mlir::DictionaryAttr>
getObjCPropertyFacts(CIRGenModule &cgm, const ObjCPropertyDecl *property) {
  std::optional<mlir::StringAttr> propertyUSR =
      getObjCDeclUSRAttr(cgm, property);
  if (!propertyUSR)
    return std::nullopt;

  bool hasCompleteIdentity = true;
  mlir::DictionaryAttr propertyType =
      getObjCSourceTypeFacts(cgm, property->getType(), hasCompleteIdentity);
  if (!hasCompleteIdentity || !propertyType)
    return std::nullopt;

  mlir::NamedAttrList propertyFacts;
  propertyFacts.set("usr", *propertyUSR);
  propertyFacts.set("type", propertyType);
  propertyFacts.set("attributes",
                    cgm.getBuilder().getI64IntegerAttr(static_cast<uint64_t>(
                        property->getPropertyAttributes())));
  propertyFacts.set("attributes_as_written",
                    cgm.getBuilder().getI64IntegerAttr(static_cast<uint64_t>(
                        property->getPropertyAttributesAsWritten())));
  propertyFacts.set(
      "getter_selector",
      cgm.getBuilder().getStringAttr(property->getGetterName().getAsString()));
  propertyFacts.set(
      "setter_selector",
      cgm.getBuilder().getStringAttr(property->getSetterName().getAsString()));
  propertyFacts.set("is_class",
                    cgm.getBuilder().getBoolAttr(property->isClassProperty()));
  propertyFacts.set("is_optional",
                    cgm.getBuilder().getBoolAttr(property->isOptional()));
  propertyFacts.set("is_readonly",
                    cgm.getBuilder().getBoolAttr(property->isReadOnly()));
  propertyFacts.set("is_atomic",
                    cgm.getBuilder().getBoolAttr(property->isAtomic()));
  propertyFacts.set("is_direct",
                    cgm.getBuilder().getBoolAttr(property->isDirectProperty()));
  if (!property->isReadOnly())
    propertyFacts.set("setter_kind",
                      cgm.getBuilder().getI64IntegerAttr(
                          static_cast<uint64_t>(property->getSetterKind())));

  if (const ObjCIvarDecl *backingIvar = property->getPropertyIvarDecl()) {
    std::optional<mlir::StringAttr> backingIvarUSR =
        getObjCDeclUSRAttr(cgm, backingIvar);
    if (!backingIvarUSR)
      return std::nullopt;
    propertyFacts.set("backing_ivar_usr", *backingIvarUSR);
  }

  return propertyFacts.getDictionary(&cgm.getMLIRContext());
}

static std::optional<mlir::DictionaryAttr>
getObjCIvarFacts(CIRGenModule &cgm, const ObjCInterfaceDecl *interface,
                 const ObjCIvarDecl *ivar) {
  std::optional<mlir::StringAttr> ivarUSR = getObjCDeclUSRAttr(cgm, ivar);
  if (!ivarUSR)
    return std::nullopt;

  bool hasCompleteIdentity = true;
  mlir::DictionaryAttr ivarType =
      getObjCSourceTypeFacts(cgm, ivar->getType(), hasCompleteIdentity);
  if (!hasCompleteIdentity || !ivarType)
    return std::nullopt;

  mlir::NamedAttrList ivarFacts;
  ivarFacts.set("usr", *ivarUSR);
  ivarFacts.set("type", ivarType);
  ivarFacts.set("offset_bits",
                cgm.getBuilder().getI64IntegerAttr(
                    cgm.getASTContext().lookupFieldBitOffset(interface, ivar)));
  ivarFacts.set("access_control",
                cgm.getBuilder().getI64IntegerAttr(
                    static_cast<uint64_t>(ivar->getAccessControl())));
  ivarFacts.set("canonical_access_control",
                cgm.getBuilder().getI64IntegerAttr(
                    static_cast<uint64_t>(ivar->getCanonicalAccessControl())));
  ivarFacts.set("is_synthesized",
                cgm.getBuilder().getBoolAttr(ivar->getSynthesize()));
  ivarFacts.set("is_bitfield",
                cgm.getBuilder().getBoolAttr(ivar->isBitField()));
  if (ivar->isBitField())
    ivarFacts.set("bit_width",
                  cgm.getBuilder().getI64IntegerAttr(ivar->getBitWidthValue()));
  return ivarFacts.getDictionary(&cgm.getMLIRContext());
}

void CIRGenModule::emitObjCInterfaceDecl(const ObjCInterfaceDecl *interface) {
  std::optional<mlir::StringAttr> interfaceUSR =
      getObjCDeclUSRAttr(*this, interface);
  if (!interfaceUSR) {
    errorNYI(interface->getBeginLoc(),
             "ObjCInterface definition without a Clang USR");
    return;
  }

  std::optional<mlir::StringAttr> superclassUSR;
  if (const ObjCInterfaceDecl *superclass = interface->getSuperClass()) {
    superclassUSR = getObjCDeclUSRAttr(*this, superclass);
    if (!superclassUSR) {
      errorNYI(superclass->getBeginLoc(),
               "ObjCInterface superclass without a Clang USR");
      return;
    }
  }

  llvm::SmallVector<mlir::Attribute, 4> protocolUSRs;
  for (const ObjCProtocolDecl *protocol : interface->protocols()) {
    std::optional<mlir::StringAttr> protocolUSR =
        getObjCDeclUSRAttr(*this, protocol);
    if (!protocolUSR) {
      errorNYI(protocol->getBeginLoc(),
               "ObjCInterface conformance without a Clang USR");
      return;
    }
    protocolUSRs.push_back(*protocolUSR);
  }

  llvm::SmallVector<mlir::Attribute, 8> methods;
  for (const ObjCMethodDecl *method : interface->methods()) {
    std::optional<mlir::DictionaryAttr> methodFacts =
        getObjCMethodFacts(*this, method);
    if (!methodFacts) {
      errorNYI(method->getBeginLoc(),
               "ObjCInterface method without complete Clang identity");
      return;
    }
    methods.push_back(*methodFacts);
  }

  llvm::SmallVector<mlir::Attribute, 4> properties;
  for (const ObjCPropertyDecl *property : interface->properties()) {
    std::optional<mlir::DictionaryAttr> propertyFacts =
        getObjCPropertyFacts(*this, property);
    if (!propertyFacts) {
      errorNYI(property->getBeginLoc(),
               "ObjCInterface property without complete Clang identity");
      return;
    }
    properties.push_back(*propertyFacts);
  }

  llvm::SmallVector<mlir::Attribute, 4> ivars;
  for (const ObjCIvarDecl *ivar : interface->ivars()) {
    std::optional<mlir::DictionaryAttr> ivarFacts =
        getObjCIvarFacts(*this, interface, ivar);
    if (!ivarFacts) {
      errorNYI(ivar->getBeginLoc(),
               "ObjCInterface ivar without complete Clang identity");
      return;
    }
    ivars.push_back(*ivarFacts);
  }

  const ASTRecordLayout &layout =
      getASTContext().getASTObjCInterfaceLayout(interface);
  mlir::NamedAttrList interfaceFacts;
  interfaceFacts.set("usr", *interfaceUSR);
  interfaceFacts.set(
      "runtime_name",
      builder.getStringAttr(interface->getObjCRuntimeNameAsString()));
  if (superclassUSR)
    interfaceFacts.set("superclass_usr", *superclassUSR);
  interfaceFacts.set("protocol_usrs", builder.getArrayAttr(protocolUSRs));
  interfaceFacts.set("methods", builder.getArrayAttr(methods));
  interfaceFacts.set("properties", builder.getArrayAttr(properties));
  interfaceFacts.set("ivars", builder.getArrayAttr(ivars));
  interfaceFacts.set("layout_size_bytes",
                     builder.getI64IntegerAttr(layout.getSize().getQuantity()));
  interfaceFacts.set(
      "layout_data_size_bytes",
      builder.getI64IntegerAttr(layout.getDataSize().getQuantity()));
  interfaceFacts.set(
      "layout_align_bytes",
      builder.getI64IntegerAttr(layout.getAlignment().getQuantity()));
  interfaceFacts.set(
      "has_designated_initializers",
      builder.getBoolAttr(interface->hasDesignatedInitializers()));
  addObjCInterface(interfaceFacts.getDictionary(&getMLIRContext()));
}

void CIRGenModule::emitObjCCategoryDecl(const ObjCCategoryDecl *category) {
  std::optional<mlir::StringAttr> categoryUSR =
      getObjCDeclUSRAttr(*this, category);
  if (!categoryUSR) {
    errorNYI(category->getBeginLoc(),
             "ObjCCategory declaration without a Clang USR");
    return;
  }

  const ObjCInterfaceDecl *targetInterface = category->getClassInterface();
  if (!targetInterface) {
    errorNYI(category->getBeginLoc(),
             "ObjCCategory declaration without a target interface");
    return;
  }
  std::optional<mlir::StringAttr> targetInterfaceUSR =
      getObjCDeclUSRAttr(*this, targetInterface);
  if (!targetInterfaceUSR) {
    errorNYI(targetInterface->getBeginLoc(),
             "ObjCCategory target interface without a Clang USR");
    return;
  }

  llvm::SmallVector<mlir::Attribute, 4> protocolUSRs;
  for (const ObjCProtocolDecl *protocol : category->protocols()) {
    std::optional<mlir::StringAttr> protocolUSR =
        getObjCDeclUSRAttr(*this, protocol);
    if (!protocolUSR) {
      errorNYI(protocol->getBeginLoc(),
               "ObjCCategory conformance without a Clang USR");
      return;
    }
    protocolUSRs.push_back(*protocolUSR);
  }

  llvm::SmallVector<mlir::Attribute, 8> methods;
  for (const ObjCMethodDecl *method : category->methods()) {
    std::optional<mlir::DictionaryAttr> methodFacts =
        getObjCMethodFacts(*this, method);
    if (!methodFacts) {
      errorNYI(method->getBeginLoc(),
               "ObjCCategory method without complete Clang identity");
      return;
    }
    methods.push_back(*methodFacts);
  }

  llvm::SmallVector<mlir::Attribute, 4> properties;
  for (const ObjCPropertyDecl *property : category->properties()) {
    std::optional<mlir::DictionaryAttr> propertyFacts =
        getObjCPropertyFacts(*this, property);
    if (!propertyFacts) {
      errorNYI(property->getBeginLoc(),
               "ObjCCategory property without complete Clang identity");
      return;
    }
    properties.push_back(*propertyFacts);
  }

  mlir::NamedAttrList categoryFacts;
  categoryFacts.set("usr", *categoryUSR);
  categoryFacts.set("name", builder.getStringAttr(category->getName()));
  categoryFacts.set("target_interface_usr", *targetInterfaceUSR);
  categoryFacts.set("protocol_usrs", builder.getArrayAttr(protocolUSRs));
  categoryFacts.set("methods", builder.getArrayAttr(methods));
  categoryFacts.set("properties", builder.getArrayAttr(properties));
  categoryFacts.set("is_class_extension",
                    builder.getBoolAttr(category->IsClassExtension()));
  addObjCCategory(categoryFacts.getDictionary(&getMLIRContext()));
}

void CIRGenModule::emitDeclContext(const DeclContext *dc) {
  for (Decl *decl : dc->decls()) {
    // Unlike other DeclContexts, the contents of an ObjCImplDecl at TU scope
    // are themselves considered "top-level", so EmitTopLevelDecl on an
    // ObjCImplDecl does not recursively visit them. We need to do that in
    // case they're nested inside another construct (LinkageSpecDecl /
    // ExportDecl) that does stop them from being considered "top-level".
    if (auto *oid = dyn_cast<ObjCImplDecl>(decl))
      errorNYI(oid->getSourceRange(), "emitDeclConext: ObjCImplDecl");

    emitTopLevelDecl(decl);
  }
}

// Emit code for a single top level declaration.
void CIRGenModule::emitTopLevelDecl(Decl *decl) {
  llvm::SaveAndRestore<const Decl *> diagnosticOwner(currentDiagnosticDecl,
                                                     decl);

  // Ignore dependent declarations.
  if (decl->isTemplated())
    return;

  switch (decl->getKind()) {
  default:
    errorNYI(decl->getBeginLoc(), "declaration of kind",
             decl->getDeclKindName());
    break;
  case Decl::ObjCInterface: {
    const auto *interface = cast<ObjCInterfaceDecl>(decl);
    // Forward declarations have no executable representation. Definitions
    // carry runtime layout and dispatch facts, so retain their exact Clang
    // identities and signatures in module metadata without synthesizing
    // Objective-C runtime operations.
    if (interface->isThisDeclarationADefinition())
      emitObjCInterfaceDecl(interface);
    break;
  }
  case Decl::ObjCCategory: {
    const auto *category = cast<ObjCCategoryDecl>(decl);
    // Categories declare type and dispatch facts but contain no method bodies.
    // Preserve those facts without synthesizing Objective-C runtime operations.
    emitObjCCategoryDecl(category);
    break;
  }
  case Decl::ObjCProtocol: {
    const auto *protocol = cast<ObjCProtocolDecl>(decl);
    // Forward declarations have no executable representation. Definitions
    // carry runtime method and conformance facts, so retain their exact Clang
    // identities and signatures in module metadata without synthesizing
    // Objective-C runtime operations.
    if (protocol->isThisDeclarationADefinition())
      emitObjCProtocolDecl(protocol);
    break;
  }

  case Decl::CXXConversion:
  case Decl::CXXMethod:
  case Decl::Function: {
    auto *fd = cast<FunctionDecl>(decl);
    // Consteval functions shouldn't be emitted.
    if (!fd->isConsteval())
      emitGlobal(fd);
    break;
  }
  case Decl::Export:
    emitDeclContext(cast<ExportDecl>(decl));
    break;

  case Decl::Var:
  case Decl::Decomposition:
  case Decl::VarTemplateSpecialization: {
    emitGlobal(cast<VarDecl>(decl));
    if (auto *decomp = dyn_cast<DecompositionDecl>(decl))
      for (auto *binding : decomp->flat_bindings())
        if (auto *holdingVar = binding->getHoldingVar())
          emitGlobal(holdingVar);
    break;
  }
  case Decl::OpenACCRoutine:
    emitGlobalOpenACCRoutineDecl(cast<OpenACCRoutineDecl>(decl));
    break;
  case Decl::OpenACCDeclare:
    emitGlobalOpenACCDeclareDecl(cast<OpenACCDeclareDecl>(decl));
    break;
  case Decl::OMPThreadPrivate:
    emitOMPThreadPrivateDecl(cast<OMPThreadPrivateDecl>(decl));
    break;
  case Decl::OMPGroupPrivate:
    emitOMPGroupPrivateDecl(cast<OMPGroupPrivateDecl>(decl));
    break;
  case Decl::OMPAllocate:
    emitOMPAllocateDecl(cast<OMPAllocateDecl>(decl));
    break;
  case Decl::OMPCapturedExpr:
    emitOMPCapturedExpr(cast<OMPCapturedExprDecl>(decl));
    break;
  case Decl::OMPDeclareReduction:
    emitOMPDeclareReduction(cast<OMPDeclareReductionDecl>(decl));
    break;
  case Decl::OMPDeclareMapper:
    emitOMPDeclareMapper(cast<OMPDeclareMapperDecl>(decl));
    break;
  case Decl::OMPRequires:
    emitOMPRequiresDecl(cast<OMPRequiresDecl>(decl));
    break;
  case Decl::Enum:
  case Decl::Using:          // using X; [C++]
  case Decl::UsingDirective: // using namespace X; [C++]
  case Decl::UsingEnum:      // using enum X; [C++]
  case Decl::NamespaceAlias:
  case Decl::Typedef:
  case Decl::TypeAlias: // using foo = bar; [C++11]
  case Decl::Record:
    assert(!cir::MissingFeatures::generateDebugInfo());
    break;

  // No code generation needed.
  case Decl::ClassTemplate:
  case Decl::Concept:
  case Decl::CXXDeductionGuide:
  case Decl::Empty:
  case Decl::ExplicitInstantiation:
  case Decl::FunctionTemplate:
  case Decl::StaticAssert:
  case Decl::TypeAliasTemplate:
  case Decl::UsingShadow:
  case Decl::VarTemplate:
  case Decl::VarTemplatePartialSpecialization:
    break;

  case Decl::CXXConstructor:
    getCXXABI().emitCXXConstructors(cast<CXXConstructorDecl>(decl));
    break;
  case Decl::CXXDestructor:
    getCXXABI().emitCXXDestructors(cast<CXXDestructorDecl>(decl));
    break;

  // C++ Decls
  case Decl::LinkageSpec:
  case Decl::Namespace:
    emitDeclContext(Decl::castToDeclContext(decl));
    break;

  case Decl::ClassTemplateSpecialization:
  case Decl::CXXRecord: {
    CXXRecordDecl *crd = cast<CXXRecordDecl>(decl);
    assert(!cir::MissingFeatures::generateDebugInfo());
    for (auto *childDecl : crd->decls())
      if (isa<VarDecl, CXXRecordDecl, EnumDecl, OpenACCDeclareDecl>(childDecl))
        emitTopLevelDecl(childDecl);
    break;
  }

  case Decl::FileScopeAsm:
    // File-scope asm is ignored during device-side CUDA compilation.
    if (langOpts.CUDA && langOpts.CUDAIsDevice)
      break;
    // File-scope asm is ignored during device-side OpenMP compilation.
    if (langOpts.OpenMPIsTargetDevice)
      break;
    // File-scope asm is ignored during device-side SYCL compilation.
    if (langOpts.SYCLIsDevice)
      break;
    auto *file_asm = cast<FileScopeAsmDecl>(decl);
    std::string line = file_asm->getAsmString();
    globalScopeAsm.push_back(builder.getStringAttr(line));
    break;
  }
}

void CIRGenModule::setInitializer(cir::GlobalOp &op, mlir::Attribute value) {
  // Recompute visibility when updating initializer.
  op.setInitialValueAttr(value);
  assert(!cir::MissingFeatures::opGlobalVisibility());
}

std::pair<cir::FuncType, cir::FuncOp> CIRGenModule::getAddrAndTypeOfCXXStructor(
    GlobalDecl gd, const CIRGenFunctionInfo *fnInfo, cir::FuncType fnType,
    bool dontDefer, ForDefinition_t isForDefinition) {
  auto *md = cast<CXXMethodDecl>(gd.getDecl());

  if (isa<CXXDestructorDecl>(md)) {
    // Always alias equivalent complete destructors to base destructors in the
    // MS ABI.
    if (getTarget().getCXXABI().isMicrosoft() &&
        gd.getDtorType() == Dtor_Complete &&
        md->getParent()->getNumVBases() == 0)
      errorNYI(md->getSourceRange(),
               "getAddrAndTypeOfCXXStructor: MS ABI complete destructor");
  }

  if (!fnType) {
    if (!fnInfo)
      fnInfo = &getTypes().arrangeCXXStructorDeclaration(gd);
    fnType = getTypes().getFunctionType(*fnInfo);
  }

  auto fn = getOrCreateCIRFunction(getMangledName(gd), fnType, gd,
                                   /*ForVtable=*/false, dontDefer,
                                   /*IsThunk=*/false, isForDefinition);

  return {fnType, fn};
}

cir::FuncOp CIRGenModule::getAddrOfFunction(clang::GlobalDecl gd,
                                            mlir::Type funcType, bool forVTable,
                                            bool dontDefer,
                                            ForDefinition_t isForDefinition) {
  assert(!cast<FunctionDecl>(gd.getDecl())->isConsteval() &&
         "consteval function should never be emitted");

  if (!funcType) {
    const auto *fd = cast<FunctionDecl>(gd.getDecl());
    funcType = convertType(fd->getType());
  }

  // Devirtualized destructor calls may come through here instead of via
  // getAddrOfCXXStructor. Make sure we use the MS ABI base destructor instead
  // of the complete destructor when necessary.
  if (const auto *dd = dyn_cast<CXXDestructorDecl>(gd.getDecl())) {
    if (getTarget().getCXXABI().isMicrosoft() &&
        gd.getDtorType() == Dtor_Complete &&
        dd->getParent()->getNumVBases() == 0)
      errorNYI(dd->getSourceRange(),
               "getAddrOfFunction: MS ABI complete destructor");
  }

  StringRef mangledName = getMangledName(gd);
  cir::FuncOp func =
      getOrCreateCIRFunction(mangledName, funcType, gd, forVTable, dontDefer,
                             /*isThunk=*/false, isForDefinition);
  // Returns kernel handle for HIP kernel stub function.
  if (langOpts.CUDA && !langOpts.CUDAIsDevice &&
      cast<FunctionDecl>(gd.getDecl())->hasAttr<CUDAGlobalAttr>()) {
    mlir::Operation *handle = getCUDARuntime().getKernelHandle(func, gd);

    // For HIP the kernel handle is a GlobalOp, which cannot be cast to
    // FuncOp. Return the stub directly in that case.
    bool isHIPHandle = mlir::isa<cir::GlobalOp>(*handle);
    if (isForDefinition || isHIPHandle)
      return func;
    return mlir::dyn_cast<cir::FuncOp>(*handle);
  }
  return func;
}

static std::string getMangledNameImpl(CIRGenModule &cgm, GlobalDecl gd,
                                      const NamedDecl *nd) {
  SmallString<256> buffer;

  llvm::raw_svector_ostream out(buffer);
  MangleContext &mc = cgm.getCXXABI().getMangleContext();

  assert(!cir::MissingFeatures::moduleNameHash());

  if (mc.shouldMangleDeclName(nd)) {
    mc.mangleName(gd.getWithDecl(nd), out);
  } else {
    IdentifierInfo *ii = nd->getIdentifier();
    assert(ii && "Attempt to mangle unnamed decl.");

    const auto *fd = dyn_cast<FunctionDecl>(nd);
    if (fd &&
        fd->getType()->castAs<FunctionType>()->getCallConv() == CC_X86RegCall) {
      cgm.errorNYI(nd->getSourceRange(), "getMangledName: X86RegCall");
    } else if (fd && fd->hasAttr<CUDAGlobalAttr>() &&
               gd.getKernelReferenceKind() == KernelReferenceKind::Stub) {
      out << "__device_stub__" << ii->getName();
    } else if (fd &&
               DeviceKernelAttr::isOpenCLSpelling(
                   fd->getAttr<DeviceKernelAttr>()) &&
               gd.getKernelReferenceKind() == KernelReferenceKind::Stub) {
      cgm.errorNYI(nd->getSourceRange(), "getMangledName: OpenCL Stub");
    } else {
      out << ii->getName();
    }
  }

  // Check if the module name hash should be appended for internal linkage
  // symbols. This should come before multi-version target suffixes are
  // appendded. This is to keep the name and module hash suffix of the internal
  // linkage function together. The unique suffix should only be added when name
  // mangling is done to make sure that the final name can be properly
  // demangled. For example, for C functions without prototypes, name mangling
  // is not done and the unique suffix should not be appended then.
  assert(!cir::MissingFeatures::moduleNameHash());

  if (const auto *fd = dyn_cast<FunctionDecl>(nd)) {
    if (fd->isMultiVersion()) {
      cgm.errorNYI(nd->getSourceRange(),
                   "getMangledName: multi-version functions");
    }
  }
  if (cgm.getLangOpts().GPURelocatableDeviceCode) {
    cgm.errorNYI(nd->getSourceRange(),
                 "getMangledName: GPU relocatable device code");
  }

  return std::string(out.str());
}

static FunctionDecl *
createOpenACCBindTempFunction(ASTContext &ctx, const IdentifierInfo *bindName,
                              const FunctionDecl *protoFunc) {
  // If this is a C no-prototype function, we can take the 'easy' way out and
  // just create a function with no arguments/functions, etc.
  if (!protoFunc->hasPrototype())
    return FunctionDecl::Create(
        ctx, /*DC=*/ctx.getTranslationUnitDecl(),
        /*StartLoc=*/SourceLocation{}, /*NLoc=*/SourceLocation{}, bindName,
        protoFunc->getType(), /*TInfo=*/nullptr, StorageClass::SC_None);

  QualType funcTy = protoFunc->getType();
  auto *fpt = cast<FunctionProtoType>(protoFunc->getType());

  // If this is a member function, add an explicit 'this' to the function type.
  if (auto *methodDecl = dyn_cast<CXXMethodDecl>(protoFunc);
      methodDecl && methodDecl->isImplicitObjectMemberFunction()) {
    llvm::SmallVector<QualType> paramTypes{fpt->getParamTypes()};
    paramTypes.insert(paramTypes.begin(), methodDecl->getThisType());

    funcTy = ctx.getFunctionType(fpt->getReturnType(), paramTypes,
                                 fpt->getExtProtoInfo());
    fpt = cast<FunctionProtoType>(funcTy);
  }

  auto *tempFunc =
      FunctionDecl::Create(ctx, /*DC=*/ctx.getTranslationUnitDecl(),
                           /*StartLoc=*/SourceLocation{},
                           /*NLoc=*/SourceLocation{}, bindName, funcTy,
                           /*TInfo=*/nullptr, StorageClass::SC_None);

  SmallVector<ParmVarDecl *> params;
  params.reserve(fpt->getNumParams());

  // Add all of the parameters.
  for (unsigned i = 0, e = fpt->getNumParams(); i != e; ++i) {
    ParmVarDecl *parm = ParmVarDecl::Create(
        ctx, tempFunc, /*StartLoc=*/SourceLocation{},
        /*IdLoc=*/SourceLocation{},
        /*Id=*/nullptr, fpt->getParamType(i), /*TInfo=*/nullptr,
        StorageClass::SC_None, /*DefArg=*/nullptr);
    parm->setScopeInfo(0, i);
    params.push_back(parm);
  }

  tempFunc->setParams(params);

  return tempFunc;
}

std::string
CIRGenModule::getOpenACCBindMangledName(const IdentifierInfo *bindName,
                                        const FunctionDecl *attachedFunction) {
  FunctionDecl *tempFunc = createOpenACCBindTempFunction(
      getASTContext(), bindName, attachedFunction);

  std::string ret = getMangledNameImpl(*this, GlobalDecl(tempFunc), tempFunc);

  // This does nothing (it is a do-nothing function), since this is a
  // slab-allocator, but leave a call in to immediately destroy this in case we
  // ever come up with a way of getting allocations back.
  getASTContext().Deallocate(tempFunc);
  return ret;
}

bool CIRGenModule::hasEmitCapableSelectedDeclDefinition(GlobalDecl gd) const {
  const auto *decl = dyn_cast<ValueDecl>(gd.getDecl());
  if (!decl || decl->hasAttr<WeakRefAttr>() || decl->hasAttr<DLLImportAttr>())
    return false;
  if (decl->hasAttr<AliasAttr>())
    return true;

  if (const auto *function = dyn_cast<FunctionDecl>(decl)) {
    if (isa<CXXDeductionGuideDecl>(function) || function->isDeleted() ||
        function->isConsteval())
      return false;
    if (function->getTemplateSpecializationKind() ==
        TSK_ExplicitInstantiationDeclaration)
      return false;
    if (const auto *dtor = dyn_cast<CXXDestructorDecl>(function))
      if (dtor->isTrivial() && !dtor->hasAttr<DLLExportAttr>())
        return false;
    if (const FunctionDecl *definition = function->getDefinition())
      if (definition->hasBody())
        return true;
    if (function->isDefaulted())
      return true;
    if (const FunctionDecl *pattern =
            function->getTemplateInstantiationPattern())
      return pattern->getDefinition() != nullptr;
    return false;
  }
  const auto *variable = dyn_cast<VarDecl>(decl);
  if (!variable)
    return false;

  if (isSelectedVariableTemplatePattern(variable))
    return isSelectedConstantVariableTemplatePattern(variable);
  if (variable->isStaticLocal())
    return false;
  if (variable->getDefinition())
    return true;
  for (const VarDecl *redecl : variable->redecls())
    if (redecl->isThisDeclarationADefinition() != VarDecl::DeclarationOnly ||
        astContext.isMSStaticDataMemberInlineDefinition(redecl))
      return true;
  return false;
}
GlobalDecl CIRGenModule::getEmitCapableSelectedDecl(GlobalDecl gd) const {
  if (const auto *function = dyn_cast<FunctionDecl>(gd.getDecl())) {
    if (const FunctionDecl *definition = function->getDefinition())
      return gd.getWithDecl(definition);

    // Defaulted functions do not necessarily acquire a body until Sema
    // prepares them. A unique defaulted redeclaration is nevertheless the
    // declaration that owns that eventual definition.
    const FunctionDecl *defaultedDefinition = nullptr;
    for (const FunctionDecl *redecl : function->redecls()) {
      if (!redecl->isDefaulted() || redecl->isDeleted())
        continue;
      if (defaultedDefinition)
        return gd;
      defaultedDefinition = redecl;
    }
    if (defaultedDefinition)
      return gd.getWithDecl(defaultedDefinition);
    return gd;
  }

  if (const auto *variable = dyn_cast<VarDecl>(gd.getDecl()))
    if (const VarDecl *definition = variable->getDefinition())
      return GlobalDecl(definition);
  return gd;
}
bool CIRGenModule::isSelectedStaticDataMemberDeclaration(
    const VarDecl *variable) {
  if (!selectedDeclRootMode || !variable || !variable->isStaticDataMember() ||
      variable->isThisDeclarationADefinition() != VarDecl::DeclarationOnly ||
      astContext.isMSStaticDataMemberInlineDefinition(variable) ||
      variable->getDefinition() ||
      !isSelectedDeclRoot(GlobalDecl(variable)))
    return false;

  const VarDecl *initializingDecl = nullptr;
  if (!variable->getAnyInitializer(initializingDecl) || !initializingDecl)
    return false;

  // Top-level declarations reach CIR while the parser is still streaming. An
  // exact selected static member can therefore name a record type whose
  // definition appears later, together with the member's out-of-line
  // definition. QualType::isConstantStorage queries CXX definition data; defer
  // classification until selected-variable traversal revisits the completed
  // AST instead of querying a fact the incomplete record does not own.
  QualType elementType = astContext.getBaseElementType(variable->getType());
  if (elementType->isIncompleteType() ||
      variable->needsDestruction(astContext) ||
      !variable->getType().isConstantStorage(astContext,
                                             /*ExcludeCtor=*/true,
                                             /*ExcludeDtor=*/true))
    return false;

  return initializingDecl->evaluateValue() != nullptr;
}

bool CIRGenModule::isSelectedVariableTemplatePattern(
    const VarDecl *variable) const {
  // A partial specialization declaration is a dependent template pattern and
  // does not own linker storage. Selected CIR can nevertheless preserve it
  // when its initializer is an exact non-dependent constant; emission marks
  // that definition available_externally. Concrete specializations remain
  // ordinary VarTemplateSpecializationDecls.
  return selectedDeclRootMode &&
         isa_and_nonnull<VarTemplatePartialSpecializationDecl>(variable);
}

bool CIRGenModule::isSelectedConstantVariableTemplatePattern(
    const VarDecl *variable) const {
  if (!isSelectedVariableTemplatePattern(variable))
    return false;
  const Expr *initializer = variable->getInit();
  if (!initializer || initializer->isTypeDependent() ||
      initializer->isValueDependent() ||
      initializer->isInstantiationDependent())
    return false;
  return variable->evaluateValue() != nullptr;
}

void CIRGenModule::addSelectedDeclDependency(GlobalDecl gd) {
  if (!selectedDeclRootMode || !hasEmitCapableSelectedDeclDefinition(gd))
    return;

  GlobalDecl canonical = gd.getCanonicalDecl();
  if (!selectedDeclDependencies.insert(canonical).second)
    return;
  gd = getEmitCapableSelectedDecl(gd);
  selectedDeclDependencyWorklist.push_back(gd);
}

static std::string selectedSourceSpanKey(llvm::StringRef file,
                                         unsigned startLine,
                                         unsigned startColumn,
                                         unsigned endLine,
                                         unsigned endColumn) {
  return (llvm::Twine(file.size()) + ":" + file + ":" +
          llvm::Twine(startLine) + ":" + llvm::Twine(startColumn) + ":" +
          llvm::Twine(endLine) + ":" + llvm::Twine(endColumn))
      .str();
}

static std::string canonicalExpansionPath(const SourceManager &sourceManager,
                                          SourceLocation location) {
  location = sourceManager.getExpansionLoc(location);
  if (location.isInvalid())
    return {};
  llvm::SmallString<256> path;
  if (auto file = sourceManager.getFileEntryRefForID(
          sourceManager.getFileID(location)))
    path = file->getName();
  if (path.empty())
    path = sourceManager.getFilename(location);
  if (path.empty())
    return {};
  if (!llvm::sys::path::is_absolute(path) &&
      llvm::sys::fs::make_absolute(path))
    return {};
  llvm::SmallString<256> realPath;
  if (!sourceManager.getFileManager().getVirtualFileSystem().getRealPath(
          path, realPath) &&
      !realPath.empty())
    path = realPath;
  llvm::sys::path::remove_dots(path, /*remove_dot_dot=*/true);
  return path.str().str();
}

static std::optional<std::string>
selectedSourceSpanKey(const SourceManager &sourceManager, const Decl *decl) {
  if (!decl)
    return std::nullopt;
  SourceLocation begin = sourceManager.getExpansionLoc(decl->getBeginLoc());
  SourceLocation end = sourceManager.getExpansionLoc(decl->getEndLoc());
  if (begin.isInvalid())
    return std::nullopt;
  if (end.isInvalid())
    end = begin;
  std::string file = canonicalExpansionPath(sourceManager, begin);
  if (file.empty())
    return std::nullopt;
  return selectedSourceSpanKey(
      file, sourceManager.getExpansionLineNumber(begin),
      sourceManager.getExpansionColumnNumber(begin),
      sourceManager.getExpansionLineNumber(end),
      sourceManager.getExpansionColumnNumber(end));
}

void CIRGenModule::loadSelectedDeclRoots() {
  if (codeGenOpts.ClangIRSelectedDeclsFile.empty())
    return;

  selectedDeclRootMode = true;
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> bufferOrErr =
      astContext.getSourceManager().getFileManager().getBufferForFile(
          codeGenOpts.ClangIRSelectedDeclsFile);
  if (!bufferOrErr) {
    unsigned diagID = diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "failed to read -fclangir-emit-selected-decls file '%0': %1");
    diags.Report(diagID) << codeGenOpts.ClangIRSelectedDeclsFile
                         << bufferOrErr.getError().message();
    return;
  }

  llvm::SmallVector<llvm::StringRef, 256> lines;
  (*bufferOrErr)
      ->getBuffer()
      .split(lines, '\n', /*MaxSplit=*/-1, /*KeepEmpty=*/false);
  for (llvm::StringRef line : lines) {
    line = line.trim();
    if (line.consume_front("source-root:")) {
      llvm::StringRef selector = line;
      auto takeNumber = [&](uint64_t &value) {
        auto [text, rest] = line.split(':');
        if (rest.empty() || text.empty() || text.getAsInteger(10, value) ||
            text != std::to_string(value))
          return false;
        line = rest;
        return true;
      };
      uint64_t startLine = 0;
      uint64_t startColumn = 0;
      uint64_t endLine = 0;
      uint64_t endColumn = 0;
      bool valid = takeNumber(startLine) && takeNumber(startColumn) &&
                   takeNumber(endLine) && takeNumber(endColumn) &&
                   startLine <= std::numeric_limits<unsigned>::max() &&
                   startColumn <= std::numeric_limits<unsigned>::max() &&
                   endLine <= std::numeric_limits<unsigned>::max() &&
                   endColumn <= std::numeric_limits<unsigned>::max();
      auto [file, symbol] = line.split('|');
      valid = valid && !file.empty() && !symbol.empty() &&
              file.find_first_of("\r\n") == llvm::StringRef::npos &&
              symbol.find_first_of("\r\n") == llvm::StringRef::npos;
      if (valid) {
        std::string key = selectedSourceSpanKey(
            file, static_cast<unsigned>(startLine),
            static_cast<unsigned>(startColumn),
            static_cast<unsigned>(endLine),
            static_cast<unsigned>(endColumn));
        auto [entry, inserted] =
            selectedDeclRootSymbolBySourceSpan.try_emplace(key, symbol.str());
        valid = inserted || entry->getValue() == symbol;
        selectedDeclRoots.insert(symbol);
      }
      if (!valid) {
        unsigned diagID = diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "invalid exact source-span root selector '%0' in "
            "-fclangir-emit-selected-decls file");
        diags.Report(diagID) << selector;
      }
    } else if (line.consume_front("parse-symbol:")) {
      if (!line.empty())
        selectedDeclParseSymbols.insert(line);
    } else if (line.consume_front("parse-usr:")) {
      if (!line.empty())
        selectedDeclParseUSRs.insert(line);
    } else if (line.starts_with("symbol-usr:")) {
      llvm::StringRef selector = line;
      llvm::StringRef payload = line.drop_front(sizeof("symbol-usr:") - 1);
      auto [symbolLengthText, identities] = payload.split(':');
      uint64_t symbolLength = 0;
      bool valid = !symbolLengthText.empty() &&
                   !symbolLengthText.getAsInteger(10, symbolLength) &&
                   symbolLengthText == std::to_string(symbolLength) &&
                   symbolLength < identities.size();
      llvm::StringRef symbol;
      llvm::StringRef usr;
      if (valid) {
        symbol = identities.take_front(symbolLength);
        usr = identities.drop_front(symbolLength);
        valid = !symbol.empty() && !usr.empty();
      }
      if (valid) {
        for (const auto &entry : selectedDeclRootUSRBySymbol)
          if (entry.getValue() == usr && entry.getKey() != symbol) {
            valid = false;
            break;
          }
      }
      if (valid) {
        selectedDeclRoots.insert(symbol);
        selectedDeclRootUSRBySymbol.try_emplace(symbol, usr.str());
      } else {
        unsigned diagID = diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "invalid exact symbol/USR identity selector '%0' in "
            "-fclangir-emit-selected-decls file");
        diags.Report(diagID) << selector;
      }
    } else if (line.starts_with("lambda-usr:")) {
      llvm::StringRef selector = line;
      llvm::StringRef payload = line.drop_front(sizeof("lambda-usr:") - 1);
      auto [indexText, afterIndex] = payload.split(':');
      auto [contextLengthText, identities] = afterIndex.split(':');
      unsigned index = 0;
      uint64_t contextLength = 0;
      bool valid = !indexText.empty() && !contextLengthText.empty() &&
                   !indexText.getAsInteger(10, index) &&
                   !contextLengthText.getAsInteger(10, contextLength) &&
                   indexText == std::to_string(index) &&
                   contextLengthText == std::to_string(contextLength) &&
                   contextLength < identities.size();
      if (valid) {
        llvm::StringRef contextUSR = identities.take_front(contextLength);
        llvm::StringRef declarationUSR = identities.drop_front(contextLength);
        valid = !contextUSR.empty() && !declarationUSR.empty();
        if (valid)
          selectedDeclLambdaRoots.try_emplace(
              selector, SelectedLambdaRoot{declarationUSR.str(),
                                           contextUSR.str(), index});
      }
      if (!valid) {
        unsigned diagID = diags.getCustomDiagID(
            DiagnosticsEngine::Error,
            "invalid exact lambda identity selector '%0' in "
            "-fclangir-emit-selected-decls file");
        diags.Report(diagID) << selector;
      }
    } else if (line.consume_front("usr:")) {
      if (!line.empty())
        selectedDeclRootUSRs.insert(line);
    } else if (!line.empty()) {
      selectedDeclRoots.insert(line);
    }
  }

  if (selectedDeclRoots.empty() && selectedDeclRootUSRs.empty() &&
      selectedDeclLambdaRoots.empty()) {
    unsigned diagID = diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "-fclangir-emit-selected-decls file '%0' did not contain any CIR "
        "symbol or declaration USR identities");
    diags.Report(diagID) << codeGenOpts.ClangIRSelectedDeclsFile;
  }
}

llvm::StringRef CIRGenModule::selectedLambdaRootSelector(GlobalDecl gd) const {
  const auto *method = dyn_cast<CXXMethodDecl>(gd.getDecl());
  if (!method || !method->getParent() || !method->getParent()->isLambda())
    return {};
  const CXXRecordDecl *closure = method->getParent();
  llvm::SmallString<256> declarationUSR;
  if (clang::index::generateUSRForDecl(method->getCanonicalDecl(),
                                       declarationUSR))
    return {};
  const Decl *context = closure->getLambdaContextDecl();
  if (!context)
    context = Decl::castFromDeclContext(closure->getDeclContext());
  llvm::SmallString<256> contextUSR;
  if (!context || clang::index::generateUSRForDecl(context, contextUSR))
    return {};

  llvm::StringRef match;
  for (const auto &entry : selectedDeclLambdaRoots) {
    const SelectedLambdaRoot &identity = entry.getValue();
    if (identity.index != closure->getLambdaIndexInContext() ||
        llvm::StringRef(identity.declarationUSR) != declarationUSR ||
        llvm::StringRef(identity.contextUSR) != contextUSR)
      continue;
    if (!match.empty())
      return {};
    match = entry.getKey();
  }
  return match;
}

llvm::StringRef
CIRGenModule::selectedSourceRootSymbol(GlobalDecl gd) const {
  auto key = selectedSourceSpanKey(astContext.getSourceManager(), gd.getDecl());
  if (!key)
    return {};
  auto selected = selectedDeclRootSymbolBySourceSpan.find(*key);
  if (selected == selectedDeclRootSymbolBySourceSpan.end())
    return {};
  return selected->getValue();
}

static bool functionCarriesExactDeclUSR(cir::FuncOp function,
                                        llvm::StringRef usr) {
  if (auto primary =
          function->getAttrOfType<mlir::StringAttr>("ast_decl_usr");
      primary && primary.getValue() == usr)
    return true;
  auto alternatives = function->getAttrOfType<mlir::ArrayAttr>(
      "ast_decl_usr_alternatives");
  return alternatives &&
         llvm::any_of(alternatives, [&](mlir::Attribute value) {
           auto candidate = mlir::dyn_cast<mlir::StringAttr>(value);
           return candidate && candidate.getValue() == usr;
         });
}

bool CIRGenModule::isSelectedDeclRoot(GlobalDecl gd) {
  if (!selectedDeclRootMode)
    return false;
  const Decl *decl = gd.getDecl();
  // emitSelectedMethods() walks every member of a DeclContext, including
  // declarations that are never code generated. A deduction guide exists only
  // for class template argument deduction: it has no definition and no linkage
  // name, so asking the Itanium mangler for one is unreachable. It can never
  // name a selected root, so answer before any mangling is attempted.
  if (isa<CXXDeductionGuideDecl>(decl))
    return false;
  if (!selectedSourceRootSymbol(gd).empty())
    return true;
  // Constructor and destructor USRs identify the source declaration but not
  // one ABI entry point. Selected-root manifests carry the exact producer
  // symbol as well as that shared USR, so only the symbol may select a
  // structor root; the other ABI variants remain ordinary closure
  // dependencies.
  if (isa<CXXConstructorDecl, CXXDestructorDecl>(decl))
    return selectedDeclRoots.contains(getMangledName(gd));
  if (const auto *method = dyn_cast<CXXMethodDecl>(decl);
      method && method->getParent() && method->getParent()->isLambda())
    return !selectedLambdaRootSelector(gd).empty() ||
           selectedDeclRoots.contains(getMangledName(gd));
  GlobalDecl canonicalGD = gd.getCanonicalDecl();
  llvm::SmallString<256> usr;
  if (!clang::index::generateUSRForDecl(canonicalGD.getDecl(), usr) &&
      selectedDeclRootUSRs.contains(usr))
    return true;
  if (!usr.empty())
    for (const auto &entry : selectedDeclRootUSRBySymbol)
      if (entry.getValue() == usr)
        return true;
  return selectedDeclRoots.contains(getMangledName(gd));
}

void CIRGenModule::noteSelectedDeclRootDefinition(GlobalDecl gd,
                                                  mlir::Operation *definition) {
  if (!selectedDeclRootMode)
    return;

  if (!definition)
    definition = getGlobalValue(getMangledName(gd));
  auto global =
      mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(definition);
  if (!global || !global.isDefinition())
    return;
  auto actualSymbol = definition->getAttrOfType<mlir::StringAttr>(
      mlir::SymbolTable::getSymbolAttrName());
  if (!actualSymbol || actualSymbol.getValue().empty())
    return;

  GlobalDecl canonicalGD = gd.getCanonicalDecl();
  llvm::SmallString<256> usr;
  const bool hasUSR =
      !clang::index::generateUSRForDecl(canonicalGD.getDecl(), usr);
  llvm::StringRef sourceSelectedSymbol = selectedSourceRootSymbol(gd);
  cir::FuncOp function;
  if (isa<FunctionDecl>(gd.getDecl())) {
    function = mlir::dyn_cast<cir::FuncOp>(definition);
    if (!function || function.getBody().empty())
      return;
    const bool carriesUSR =
        hasUSR && functionCarriesExactDeclUSR(function, usr);
    const auto *method = dyn_cast<CXXMethodDecl>(gd.getDecl());
    const bool isLambda =
        method && method->getParent() && method->getParent()->isLambda();
    // A generic lambda specialization's emitted function may retain the
    // primary call operator's ast_decl_usr. An exact emitted ABI symbol is
    // independently compiler-owned declaration identity, so it also
    // authenticates a selected symbol even when Clang collapses the
    const bool exactSelectedSymbol =
        selectedDeclRoots.contains(actualSymbol.getValue()) ||
        !sourceSelectedSymbol.empty();
    if (!exactSelectedSymbol && (!hasUSR || (!isLambda && !carriesUSR)))
      return;
  }

  emittedSelectedDeclDependencies.insert(canonicalGD);
  // A specialized lambda root may emit the primary call operator definition,
  // whose ast_decl_usr differs from the selected specialization USR. Bind that
  // already-emitted definition through the producer-owned context USR and
  // lambda index only when they select one manifest entry.
  if (function) {
    const auto context =
        function->getAttrOfType<mlir::StringAttr>("ast_lambda_context_usr");
    const auto index =
        function->getAttrOfType<mlir::IntegerAttr>("ast_lambda_index");
    llvm::StringRef contextualSelector;
    bool contextualAmbiguity = false;
    if (context && index) {
      for (const auto &entry : selectedDeclLambdaRoots) {
        const SelectedLambdaRoot &identity = entry.getValue();
        if (context.getValue() != llvm::StringRef(identity.contextUSR) ||
            index.getInt() != static_cast<int64_t>(identity.index))
          continue;
        if (contextualSelector.empty())
          contextualSelector = entry.getKey();
        else if (contextualSelector != entry.getKey())
          contextualAmbiguity = true;
      }
    }
    if (!contextualSelector.empty() && !contextualAmbiguity) {
      emittedSelectedDeclRootDefinitionsBySelector[contextualSelector].insert(
          actualSymbol.getValue());
    }
  }
  // The emitted ABI symbol is itself compiler-owned declaration identity.
  // Record an exact selected-symbol hit even when the source declaration was
  // reached through a template or lambda specialization path whose GlobalDecl
  // does not compare equal to the root-resolution candidate.
  if (selectedDeclRoots.contains(actualSymbol.getValue()))
    emittedSelectedDeclRootDefinitionsBySelector[actualSymbol.getValue()]
        .insert(actualSymbol.getValue());
  // A symbol-USR pair authenticates one exact ABI symbol. Do not fan that
  // selector out to every emitted declaration with the same USR: Clang can
  // collapse template and lambda specialization USRs, while their ABI symbols
  // and bodies remain distinct.
  if (!isSelectedDeclRoot(gd))
    return;
  llvm::StringRef lambdaSelector = selectedLambdaRootSelector(gd);
  if (!lambdaSelector.empty()) {
    const SelectedLambdaRoot &identity =
        selectedDeclLambdaRoots.find(lambdaSelector)->getValue();
    auto producerContextUSR =
        function->getAttrOfType<mlir::StringAttr>("ast_lambda_context_usr");
    auto producerIndex =
        function->getAttrOfType<mlir::IntegerAttr>("ast_lambda_index");
    if (!producerContextUSR ||
        producerContextUSR.getValue() != llvm::StringRef(identity.contextUSR) ||
        !producerIndex ||
        producerIndex.getInt() != static_cast<int64_t>(identity.index))
      return;
    emittedSelectedDeclRootDefinitionsBySelector[lambdaSelector].insert(
        actualSymbol.getValue());
  }
  if (!sourceSelectedSymbol.empty())
    emittedSelectedDeclRootDefinitionsBySelector[sourceSelectedSymbol].insert(
        actualSymbol.getValue());

  llvm::StringRef requestedSymbol = getMangledName(gd);
  if (selectedDeclRoots.contains(requestedSymbol))
    emittedSelectedDeclRootDefinitionsBySelector[requestedSymbol].insert(
        actualSymbol.getValue());
  if (hasUSR && selectedDeclRootUSRs.contains(usr)) {
    std::string selector = "usr:";
    selector.append(usr.data(), usr.size());
    emittedSelectedDeclRootDefinitionsBySelector[selector].insert(
        actualSymbol.getValue());
  }
}

void CIRGenModule::diagnoseUnemittedSelectedDeclRoots() {
  if (!selectedDeclRootMode)
    return;

  mlir::NamedAttrList completedBindings;
  auto exactDefinitionFor =
      [&](llvm::StringRef selector) -> std::optional<llvm::StringRef> {
    auto binding = emittedSelectedDeclRootDefinitionsBySelector.find(selector);
    if (binding == emittedSelectedDeclRootDefinitionsBySelector.end()) {
      // A deferred template specialization can be materialized by an
      // enclosing emission path before the selected-root worklist observes
      // its GlobalDecl. The module's exact symbol table is the final
      // compiler-owned authority, so prefer a definition under the selected
      // mangled symbol itself.
      auto direct = mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(
          getGlobalValue(selector));
      if (direct && direct.isDefinition()) {
        completedBindings.set(selector, builder.getStringAttr(selector));
        return selector;
      }

      // A symbol-USR entry may intentionally carry a stale ABI symbol. Only
      // when that exact symbol is absent may its paired compiler USR select a
      // replacement, and the replacement must be unique among emitted bodies.
      auto pairedUSR = selectedDeclRootUSRBySymbol.find(selector);
      if (pairedUSR == selectedDeclRootUSRBySymbol.end())
        return std::nullopt;
      llvm::StringRef uniqueCandidateSymbol;
      unsigned candidateCount = 0;
      theModule.walk([&](cir::FuncOp function) {
        if (!functionCarriesExactDeclUSR(function,
                                         pairedUSR->getValue()) ||
            function.getBody().empty())
          return;
        ++candidateCount;
        uniqueCandidateSymbol = function.getSymName();
      });
      if (candidateCount != 1)
        return std::nullopt;
      completedBindings.set(
          selector, builder.getStringAttr(uniqueCandidateSymbol));
      return uniqueCandidateSymbol;
    }
    if (binding->second.size() != 1)
      return std::nullopt;
    llvm::StringRef actualSymbol = binding->second.begin()->getKey();
    auto global = mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(
        getGlobalValue(actualSymbol));
    if (!global || !global.isDefinition())
      return std::nullopt;
    completedBindings.set(selector, builder.getStringAttr(actualSymbol));
    return actualSymbol;
  };

  for (const auto &root : selectedDeclRoots) {
    if (exactDefinitionFor(root.getKey()))
      continue;
    unsigned diagID = diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "failed to emit exact selected declaration symbol '%0': one "
        "identity-authenticated CIR definition was required");
    diags.Report(diagID) << root.getKey();
    if (auto pairedUSR = selectedDeclRootUSRBySymbol.find(root.getKey());
        pairedUSR != selectedDeclRootUSRBySymbol.end()) {
      unsigned usrDiagID = diags.getCustomDiagID(
          DiagnosticsEngine::Error,
          "failed to emit exact selected declaration USR '%0': one "
          "identity-authenticated CIR definition was required");
      diags.Report(usrDiagID) << pairedUSR->getValue();
    }
    if (getenv("AENEAS_SELECTED_NEAR_MISS")) {
      llvm::StringRef prefix = root.getKey().take_front(72);
      theModule.walk([&](cir::FuncOp function) {
        if (function.getSymName().starts_with(prefix))
          llvm::errs() << "AENEAS_NEAR_MISS missing=" << root.getKey()
                       << "\n  candidate=" << function.getSymName()
                       << " definition=" << !function.getBody().empty()
                       << "\n";
      });
    }
  }
  for (const auto &root : selectedDeclLambdaRoots) {
    if (exactDefinitionFor(root.getKey()))
      continue;
    const SelectedLambdaRoot &identity = root.getValue();
    std::string candidates;
    std::optional<std::string> uniqueCandidateSymbol;
    unsigned candidateCount = 0;
    theModule.walk([&](cir::FuncOp function) {
      const auto index =
          function->getAttrOfType<mlir::IntegerAttr>("ast_lambda_index");
      const auto context =
          function->getAttrOfType<mlir::StringAttr>("ast_lambda_context_usr");
      if (!index || !context ||
          index.getInt() != static_cast<int64_t>(identity.index) ||
          context.getValue() != llvm::StringRef(identity.contextUSR))
        return;
      if (function.getBody().empty())
        return;
      ++candidateCount;
      uniqueCandidateSymbol = function.getSymName().str();
      if (!candidates.empty())
        candidates += ",";
      const auto declaration =
          function->getAttrOfType<mlir::StringAttr>("ast_decl_usr");
      candidates += declaration ? declaration.getValue().str() : "<no-usr>";
      candidates += "=>";
      candidates += function.getSymName().str();
    });
    if (candidateCount == 1 && uniqueCandidateSymbol.has_value()) {
      completedBindings.set(root.getKey(),
                            builder.getStringAttr(*uniqueCandidateSymbol));
      continue;
    }
    if (candidates.empty())
      candidates = "<none>";
    unsigned diagID = diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "failed to emit exact selected lambda identity '%0': one "
        "identity-authenticated CIR definition was required; matching "
        "context/index candidates=%1");
    diags.Report(diagID) << root.getKey() << candidates;
  }
  for (const auto &root : selectedDeclRootUSRs) {
    std::string selector = "usr:";
    selector.append(root.getKey());
    if (exactDefinitionFor(selector))
      continue;
    unsigned diagID = diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "failed to emit exact selected declaration USR '%0': one "
        "identity-authenticated CIR definition was required");
    diags.Report(diagID) << root.getKey();
  }
  if (!completedBindings.empty())
    theModule->setAttr("cir.selected_decl_root_definitions",
                       completedBindings.getDictionary(&getMLIRContext()));
}
void CIRGenModule::diagnoseUnemittedSelectedDeclDependencies() {
  if (!selectedDeclRootMode)
    return;
  auto hasExactNonStructorDefinition = [&](GlobalDecl gd) {
    if (isa<CXXConstructorDecl, CXXDestructorDecl>(gd.getDecl()))
      return false;
    auto function =
        mlir::dyn_cast_or_null<cir::FuncOp>(getGlobalValue(getMangledName(gd)));
    if (!function || function.isDeclaration())
      return false;
    GlobalDecl canonicalGD = gd.getCanonicalDecl();
    llvm::SmallString<256> usr;
    if (clang::index::generateUSRForDecl(canonicalGD.getDecl(), usr))
      return false;
    return functionCarriesExactDeclUSR(function, usr);
  };

  for (GlobalDecl gd : selectedDeclDependencyWorklist) {
    GlobalDecl canonical = gd.getCanonicalDecl();
    if (emittedSelectedDeclDependencies.contains(canonical))
      continue;
    if (hasExactNonStructorDefinition(gd))
      continue;

    llvm::SmallString<256> usr;
    if (clang::index::generateUSRForDecl(canonical.getDecl(), usr))
      usr = "<unavailable>";
    std::string producerState =
        attemptedSelectedDeclDependencies.contains(canonical)
            ? "exact emission attempted; "
            : "exact emission was not attempted; ";
    mlir::Operation *operation = getGlobalValue(getMangledName(gd));
    auto global =
        mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(operation);
    if (!global) {
      producerState += "CIR symbol is absent";
    } else if (global.isDeclaration()) {
      producerState += "CIR symbol is declaration-only";
    } else if (auto function = mlir::dyn_cast<cir::FuncOp>(operation)) {
      if (auto emittedUSR =
              function->getAttrOfType<mlir::StringAttr>("ast_decl_usr"))
        producerState += "CIR definition carries Clang USR '" +
                         emittedUSR.getValue().str() + "'";
      else
        producerState += "CIR definition has no Clang USR";
    } else {
      producerState += "CIR global definition exists";
    }
    unsigned diagID = diags.getCustomDiagID(
        DiagnosticsEngine::Error,
        "failed to emit referenced selected-declaration closure definition "
        "for exact CIR symbol '%0' (Clang USR '%1'; %2)");
    diags.Report(gd.getDecl()->getLocation(), diagID)
        << getMangledName(gd) << usr << producerState;
  }
}
bool CIRGenModule::shouldEmitSelectedMethod(const FunctionDecl *fd) {
  if (!selectedDeclRootMode)
    return true;
  if (!fd)
    return false;
  auto selected = [&](GlobalDecl gd) {
    return isSelectedDeclRoot(gd) || isSelectedDeclDependency(gd);
  };
  if (const auto *ctor = dyn_cast<CXXConstructorDecl>(fd))
    return selected(GlobalDecl(ctor, Ctor_Complete)) ||
           selected(GlobalDecl(ctor, Ctor_Base));
  if (const auto *dtor = dyn_cast<CXXDestructorDecl>(fd))
    return selected(GlobalDecl(dtor, Dtor_Deleting)) ||
           selected(GlobalDecl(dtor, Dtor_Complete)) ||
           selected(GlobalDecl(dtor, Dtor_Base));
  return selected(GlobalDecl(fd));
}

bool CIRGenModule::shouldParseSelectedDeclBody(const FunctionDecl *fd) {
  if (!selectedDeclRootMode)
    return true;
  const FunctionDecl *canonicalFD = fd ? fd->getCanonicalDecl() : nullptr;
  llvm::SmallString<256> usr;
  if (canonicalFD && !clang::index::generateUSRForDecl(canonicalFD, usr) &&
      selectedDeclParseUSRs.contains(usr))
    return true;
  // A dependent friend defined inside a class template can acquire its
  // concrete selected linkage name only after the class is specialized.
  // Preserve that pattern body so a later exact selected-root match can emit
  // the instantiated definition; emission remains gated by the concrete ABI
  // symbol.
  if (fd->getFriendObjectKind() != Decl::FOK_None &&
      fd->getLexicalDeclContext() &&
      fd->getLexicalDeclContext()->isDependentContext())
    return true;
  if (const auto *ctor = dyn_cast<CXXConstructorDecl>(fd))
    return selectedDeclParseSymbols.contains(
               getMangledName(GlobalDecl(ctor, Ctor_Complete))) ||
           selectedDeclParseSymbols.contains(
               getMangledName(GlobalDecl(ctor, Ctor_Base))) ||
           isSelectedDeclRoot(GlobalDecl(ctor, Ctor_Complete)) ||
           isSelectedDeclRoot(GlobalDecl(ctor, Ctor_Base)) ||
           isSelectedDeclDependency(GlobalDecl(ctor, Ctor_Complete)) ||
           isSelectedDeclDependency(GlobalDecl(ctor, Ctor_Base));
  if (const auto *dtor = dyn_cast<CXXDestructorDecl>(fd))
    return selectedDeclParseSymbols.contains(
               getMangledName(GlobalDecl(dtor, Dtor_Deleting))) ||
           selectedDeclParseSymbols.contains(
               getMangledName(GlobalDecl(dtor, Dtor_Complete))) ||
           selectedDeclParseSymbols.contains(
               getMangledName(GlobalDecl(dtor, Dtor_Base))) ||
           isSelectedDeclRoot(GlobalDecl(dtor, Dtor_Deleting)) ||
           isSelectedDeclRoot(GlobalDecl(dtor, Dtor_Complete)) ||
           isSelectedDeclRoot(GlobalDecl(dtor, Dtor_Base)) ||
           isSelectedDeclDependency(GlobalDecl(dtor, Dtor_Deleting)) ||
           isSelectedDeclDependency(GlobalDecl(dtor, Dtor_Complete)) ||
           isSelectedDeclDependency(GlobalDecl(dtor, Dtor_Base));
  if (!isa<CXXDeductionGuideDecl>(fd) &&
      selectedDeclParseSymbols.contains(getMangledName(GlobalDecl(fd))))
    return true;
  return isSelectedDeclRoot(GlobalDecl(fd)) ||
         isSelectedDeclDependency(GlobalDecl(fd));
}

void CIRGenModule::emitSelectedMethods(
    const DeclContext *context,
    llvm::function_ref<void(llvm::MutableArrayRef<GlobalDecl>)>
        prepareForEmission) {
  if (!selectedDeclRootMode)
    return;

  llvm::SmallVector<const DeclContext *, 32> contexts;
  llvm::DenseSet<const DeclContext *> visitedContexts;
  llvm::SmallVector<const FunctionTemplateDecl *, 16> functionTemplates;
  llvm::DenseSet<const FunctionTemplateDecl *> visitedFunctionTemplates;
  llvm::DenseSet<const FunctionDecl *> visitedFunctionSpecializations;
  llvm::SmallVector<const ClassTemplateDecl *, 16> classTemplates;
  llvm::DenseSet<const ClassTemplateDecl *> visitedClassTemplates;
  llvm::DenseSet<const ClassTemplateSpecializationDecl *>
      visitedClassSpecializations;
  llvm::SmallVector<GlobalDecl, 32> methods;
  llvm::DenseSet<GlobalDecl> visitedMethods;
  llvm::SmallVector<GlobalDecl, 16> parseOnlyMethods;
  llvm::DenseSet<GlobalDecl> visitedParseOnlyMethods;
  llvm::DenseSet<const FunctionDecl *> scannedFunctionBodies;
  llvm::SmallVector<GlobalDecl, 16> directCallees;
  llvm::DenseSet<GlobalDecl> visitedDirectCallees;

  auto enqueueContext = [&](const DeclContext *declContext) {
    if (visitedContexts.insert(declContext).second)
      contexts.push_back(declContext);
  };
  auto enqueueMethod = [&](GlobalDecl gd) {
    // Canonical GlobalDecl is only the deduplication key.  Emission and Sema
    // preparation must retain the concrete specialization definition and the
    // original structor ABI variant: the canonical declaration can be an
    // earlier declaration with no instantiated body.
    GlobalDecl canonical = gd.getCanonicalDecl();
    if (!visitedMethods.insert(canonical).second)
      return;
    gd = getEmitCapableSelectedDecl(gd);
    methods.push_back(gd);
  };
  auto enqueueParseOnlyMethod = [&](GlobalDecl gd) {
    GlobalDecl canonical = gd.getCanonicalDecl();
    if (!visitedParseOnlyMethods.insert(canonical).second)
      return;
    parseOnlyMethods.push_back(getEmitCapableSelectedDecl(gd));
  };
  auto enqueueFunctionTemplate = [&](const FunctionTemplateDecl *decl) {
    if (visitedFunctionTemplates.insert(decl).second)
      functionTemplates.push_back(decl);
  };
  auto enqueueClassTemplate = [&](const ClassTemplateDecl *decl) {
    if (visitedClassTemplates.insert(decl).second)
      classTemplates.push_back(decl);
  };
  llvm::StringSet<> selectedLambdaContextUSRs;
  for (const auto &root : selectedDeclLambdaRoots)
    selectedLambdaContextUSRs.insert(root.getValue().contextUSR);
  auto ownsSelectedLambdaRoot = [&](const FunctionDecl *function) {
    auto matchesContextUSR = [&](const FunctionDecl *candidate) {
      if (!candidate)
        return false;
      llvm::SmallString<256> usr;
      return !clang::index::generateUSRForDecl(candidate->getCanonicalDecl(),
                                               usr) &&
             selectedLambdaContextUSRs.contains(usr);
    };
    return matchesContextUSR(function) ||
           matchesContextUSR(function->getTemplateInstantiationPattern());
  };
  auto enqueueFunction = [&](const FunctionDecl *function) {
    if (!shouldEmitSelectedMethod(function)) {
      if (!shouldParseSelectedDeclBody(function) ||
          !ownsSelectedLambdaRoot(function))
        return;
      if (auto *ctor = dyn_cast<CXXConstructorDecl>(function))
        enqueueParseOnlyMethod(GlobalDecl(ctor, Ctor_Complete));
      else if (auto *dtor = dyn_cast<CXXDestructorDecl>(function))
        enqueueParseOnlyMethod(GlobalDecl(dtor, Dtor_Complete));
      else
        enqueueParseOnlyMethod(GlobalDecl(function));
      return;
    }
    if (auto *ctor = dyn_cast<CXXConstructorDecl>(function)) {
      enqueueMethod(GlobalDecl(ctor, Ctor_Base));
      if (!ctor->getParent()->isAbstract())
        enqueueMethod(GlobalDecl(ctor, Ctor_Complete));
    } else if (auto *dtor = dyn_cast<CXXDestructorDecl>(function)) {
      enqueueMethod(GlobalDecl(dtor, Dtor_Base));
      enqueueMethod(GlobalDecl(dtor, Dtor_Complete));
      // A selected deleting-destructor root is authoritative evidence for the
      // ABI variant even when this AST declaration does not report virtuality
      // after canonicalization. Keep the complete virtual-destructor closure.
      if (dtor->isVirtual() ||
          isSelectedDeclRoot(GlobalDecl(dtor, Dtor_Deleting)) ||
          isSelectedDeclDependency(GlobalDecl(dtor, Dtor_Deleting)))
        enqueueMethod(GlobalDecl(dtor, Dtor_Deleting));
    } else {
      enqueueMethod(GlobalDecl(function));
    }
  };
  auto discoverFunctionBody = [&](const FunctionDecl *function) {
    const FunctionDecl *definition = function->getDefinition();
    if (!definition || !definition->hasBody() ||
        !scannedFunctionBodies.insert(definition).second)
      return;
    struct BodyDeclCollector : RecursiveASTVisitor<BodyDeclCollector> {
      llvm::SmallVector<const FunctionDecl *, 8> &lambdas;
      llvm::SmallVector<Decl *, 8> &declarations;
      BodyDeclCollector(llvm::SmallVector<const FunctionDecl *, 8> &lambdas,
                        llvm::SmallVector<Decl *, 8> &declarations)
          : lambdas(lambdas), declarations(declarations) {}
      bool VisitLambdaExpr(LambdaExpr *lambda) {
        lambdas.push_back(lambda->getCallOperator());
        return true;
      }
      bool VisitDecl(Decl *decl) {
        declarations.push_back(decl);
        return true;
      }
    };
    struct EvaluatedCalleeCollector
        : EvaluatedExprVisitor<EvaluatedCalleeCollector> {
      llvm::SmallVector<const FunctionDecl *, 8> &callees;
      EvaluatedCalleeCollector(
          const ASTContext &context,
          llvm::SmallVector<const FunctionDecl *, 8> &callees)
          : EvaluatedExprVisitor(context), callees(callees) {}
      bool shouldVisitDiscardedStmt() const { return false; }
      void VisitCallExpr(CallExpr *call) {
        if (call->isUnevaluatedBuiltinCall(Context))
          return;
        if (const FunctionDecl *callee = call->getDirectCallee())
          callees.push_back(callee);
        EvaluatedExprVisitor<EvaluatedCalleeCollector>::VisitCallExpr(call);
      }
    };
    llvm::SmallVector<const FunctionDecl *, 8> lambdas;
    llvm::SmallVector<Decl *, 8> declarations;
    llvm::SmallVector<const FunctionDecl *, 8> callees;
    BodyDeclCollector collector(lambdas, declarations);
    collector.TraverseStmt(const_cast<Stmt *>(definition->getBody()));
    EvaluatedCalleeCollector calleeCollector(astContext, callees);
    calleeCollector.Visit(const_cast<Stmt *>(definition->getBody()));
    for (const FunctionDecl *lambda : lambdas) {
      enqueueFunction(lambda);
      if (const FunctionTemplateDecl *functionTemplate =
              lambda->getDescribedFunctionTemplate())
        enqueueFunctionTemplate(functionTemplate);
    }
    for (Decl *decl : declarations) {
      if (auto *functionTemplate = dyn_cast<FunctionTemplateDecl>(decl))
        enqueueFunctionTemplate(functionTemplate);
      if (auto *classTemplate = dyn_cast<ClassTemplateDecl>(decl))
        enqueueClassTemplate(classTemplate);
      if (auto *nested = dyn_cast<DeclContext>(decl))
        enqueueContext(nested);
    }
    for (const FunctionDecl *callee : callees) {
      // Ordinary callees are materialized by their call lowering. This
      // producer-side supplement is specifically for an exact concrete
      // function-template specialization whose pattern body was discarded.
      // Structor calls additionally require an ABI variant chosen by CIRGen.
      if (!callee->getPrimaryTemplate() ||
          isa<CXXConstructorDecl, CXXDestructorDecl>(callee))
        continue;
      GlobalDecl calleeGD(callee);
      GlobalDecl canonical = calleeGD.getCanonicalDecl();
      if (visitedDirectCallees.insert(canonical).second)
        directCallees.push_back(calleeGD);
    }
  };

  enqueueContext(context);
  size_t contextCursor = 0;
  size_t methodCursor = 0;
  size_t parseOnlyMethodCursor = 0;
  for (;;) {
    while (contextCursor < contexts.size()) {
      llvm::SmallVector<Decl *, 32> decls;
      for (Decl *decl : contexts[contextCursor++]->decls())
        decls.push_back(decl);

      for (Decl *decl : decls) {
        if (auto *function = dyn_cast<FunctionDecl>(decl)) {
          enqueueFunction(function);
          discoverFunctionBody(function);
        }
        if (auto *record = dyn_cast<CXXRecordDecl>(decl))
          if (auto *destructor = record->getDestructor()) {
            enqueueFunction(destructor);
            discoverFunctionBody(destructor);
          }
        if (auto *functionTemplate = dyn_cast<FunctionTemplateDecl>(decl))
          enqueueFunctionTemplate(functionTemplate);
        if (auto *classTemplate = dyn_cast<ClassTemplateDecl>(decl))
          enqueueClassTemplate(classTemplate);
        if (auto *friendDecl = dyn_cast<FriendDecl>(decl)) {
          const auto *friendFunction =
              dyn_cast_or_null<FunctionDecl>(friendDecl->getFriendDecl());
          if (friendFunction && !isa<CXXMethodDecl>(friendFunction) &&
              !friendFunction->getType()->isDependentType() &&
              friendFunction->doesThisDeclarationHaveABody()) {
            enqueueFunction(friendFunction);
            discoverFunctionBody(friendFunction);
          }
        }
        if (auto *nested = dyn_cast<DeclContext>(decl))
          enqueueContext(nested);
      }
    }

    bool foundSpecialization = false;
    for (const FunctionTemplateDecl *functionTemplate : functionTemplates) {
      llvm::SmallVector<FunctionDecl *, 16> specializations;
      for (FunctionDecl *specialization : functionTemplate->specializations())
        specializations.push_back(specialization);
      for (FunctionDecl *specialization : specializations) {
        // Keep a specialization eligible for a later pass until it becomes an
        // exact root or dependency. Preparing an enclosing body can create the
        // definition and add it to the template's specialization collection.
        if (!shouldParseSelectedDeclBody(specialization) ||
            !visitedFunctionSpecializations.insert(specialization).second)
          continue;
        foundSpecialization = true;
        enqueueFunction(specialization);
        discoverFunctionBody(specialization);
      }
    }
    for (const ClassTemplateDecl *classTemplate : classTemplates) {
      // Member function-template registries live on the primary or partial
      // pattern, while their exact FunctionDecl specializations can be owned
      // by a concrete class specialization. Visit every producer-owned
      // context; enqueueFunction still gates bodies by exact symbol or USR.
      enqueueContext(classTemplate->getTemplatedDecl());
      llvm::SmallVector<ClassTemplatePartialSpecializationDecl *, 8>
          partialSpecializations;
      classTemplate->getPartialSpecializations(partialSpecializations);
      for (ClassTemplatePartialSpecializationDecl *partial :
           partialSpecializations)
        if (visitedClassSpecializations.insert(partial).second) {
          foundSpecialization = true;
          enqueueContext(partial);
        }

      llvm::SmallVector<ClassTemplateSpecializationDecl *, 16> specializations;
      for (ClassTemplateSpecializationDecl *specialization :
           classTemplate->specializations())
        specializations.push_back(specialization);
      for (ClassTemplateSpecializationDecl *specialization : specializations) {
        if (isa<ClassTemplatePartialSpecializationDecl>(specialization) ||
            !visitedClassSpecializations.insert(specialization).second)
          continue;
        foundSpecialization = true;
        enqueueContext(specialization);
      }
    }

    if (contextCursor < contexts.size() || foundSpecialization)
      continue;

    bool madeProgress = false;
    if (parseOnlyMethodCursor != parseOnlyMethods.size()) {
      // A parse-only concrete member can own the template instantiations and
      // local declarations named by exact selected roots. Materialize its body
      // through Sema, discover those declarations, but never enqueue the
      // member itself for CIR emission.
      llvm::SmallVector<GlobalDecl, 16> frontier(
          parseOnlyMethods.begin() + parseOnlyMethodCursor,
          parseOnlyMethods.end());
      parseOnlyMethodCursor = parseOnlyMethods.size();
      prepareForEmission(frontier);
      for (GlobalDecl &gd : frontier)
        gd = getEmitCapableSelectedDecl(gd);
      for (GlobalDecl gd : frontier)
        if (const auto *function = dyn_cast<FunctionDecl>(gd.getDecl()))
          discoverFunctionBody(function);
      madeProgress = true;
    }
    if (methodCursor != methods.size()) {
      // Emission can instantiate into one of the specialization collections
      // above. Emit a stable frontier, then resnapshot only those template
      // owners instead of walking the translation unit again.
      llvm::SmallVector<GlobalDecl, 32> frontier(methods.begin() + methodCursor,
                                                 methods.end());
      methodCursor = methods.size();
      prepareForEmission(frontier);
      // Sema preparation can attach the body to a later redeclaration. Rebind
      // every work item before body discovery and emission so selected
      // declaration and lambda roots authenticate the operation that actually
      // owns the definition.
      for (GlobalDecl &gd : frontier)
        gd = getEmitCapableSelectedDecl(gd);
      for (GlobalDecl gd : frontier)
        if (const auto *function = dyn_cast<FunctionDecl>(gd.getDecl()))
          discoverFunctionBody(function);
      for (GlobalDecl gd : frontier) {
        if (const auto *ctor = dyn_cast<CXXConstructorDecl>(gd.getDecl());
            ctor && ctor->getParent()->getNumVBases() != 0) {
          if (selectedConstructorFamilies.insert(ctor->getCanonicalDecl())
                  .second) {
            // A virtual-base constructor's complete and base variants have
            // different signatures and bodies. Once either exact root selects
            // the source constructor, the ABI producer owns both definitions.
            bool wasEmittingDependency = emittingSelectedDeclDependency;
            emittingSelectedDeclDependency = true;
            getCXXABI().emitCXXConstructors(ctor);
            emittingSelectedDeclDependency = wasEmittingDependency;
          } else {
            mlir::Operation *existing = getGlobalValue(getMangledName(gd));
            auto global =
                mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(existing);
            if (!global || !global.isDefinition()) {
              // Family ownership authenticates this exact ABI variant even
              // when only its sibling spelling was the manifest root.
              emitGlobalDecl(gd);
            } else {
              noteSelectedDeclRootDefinition(gd, existing);
            }
          }
          continue;
        }
        if (const auto *dtor = dyn_cast<CXXDestructorDecl>(gd.getDecl())) {
          // Trivial destructors have no callable ABI body unless DLL export
          // explicitly requires one. Selected-root discovery can encounter
          // them through an instantiated class closure, but forcing a family
          // definition violates the same precondition as ordinary codegen.
          if (dtor->isTrivial() && !dtor->hasAttr<DLLExportAttr>())
            continue;
          if (selectedDestructorFamilies.insert(dtor->getCanonicalDecl())
                  .second) {
            bool wasEmittingDependency = emittingSelectedDeclDependency;
            if (!isSelectedDeclRoot(gd))
              emittingSelectedDeclDependency = true;
            getCXXABI().emitCXXDestructors(dtor);
            emittingSelectedDeclDependency = wasEmittingDependency;
          } else {
            // A previously emitted family may have omitted this exact ABI
            // entry point because the concrete redeclaration did not carry
            // virtuality. An exact selected/dependency variant still owns its
            // own body and must not be suppressed by family deduplication.
            mlir::Operation *existing = getGlobalValue(getMangledName(gd));
            auto global =
                mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(existing);
            if (!global || !global.isDefinition()) {
              if (isSelectedDeclRoot(gd))
                emitGlobalDecl(gd);
              else
                emitGlobal(gd);
            } else {
              // Inline callbacks can emit a local/lambda definition before the
              // selected worklist reaches it. Authenticate the already-owned
              // definition through the same exact USR/symbol checks instead
              // of silently treating its body as an unbound root.
              noteSelectedDeclRootDefinition(gd, existing);
            }
          }
          continue;
        }
        mlir::Operation *existing = getGlobalValue(getMangledName(gd));
        auto global =
            mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(existing);
        if (!global || !global.isDefinition()) {
          if (isSelectedDeclRoot(gd))
            emitGlobalDecl(gd);
          else
            emitGlobal(gd);
        } else {
          noteSelectedDeclRootDefinition(gd, existing);
        }
      }
      madeProgress = true;
    }

    for (;;) {
      llvm::SmallVector<GlobalDecl, 16> dependencies =
          takeSelectedDeclDependencyFrontier();
      if (dependencies.empty())
        break;
      prepareForEmission(dependencies);
      for (GlobalDecl &gd : dependencies)
        gd = getEmitCapableSelectedDecl(gd);
      for (GlobalDecl gd : dependencies)
        if (const auto *function = dyn_cast<FunctionDecl>(gd.getDecl()))
          discoverFunctionBody(function);
      emitSelectedDependencies(dependencies);
      madeProgress = true;
    }

    if (!madeProgress)
      break;
  }

  // A selected template body can retain an exact concrete direct callee even
  // when -skip-function-bodies discarded the callee's template pattern body.
  // Materialize that typed FunctionDecl through the ordinary declaration path
  // now. Call lowering may already have created its declaration while the
  // caller was active; createCIRFunction deliberately inserts such a
  // declaration before the caller, and later definition emission upgrades the
  // operation in place. Move the exact operation to this completed frontier so
  // the selected caller and its call precede the discovered callee without
  // creating a second FuncOp.
  for (GlobalDecl callee : directCallees) {
    const CIRGenFunctionInfo &info =
        getTypes().arrangeGlobalDeclaration(callee);
    cir::FuncOp function =
        getAddrOfFunction(callee, getTypes().getFunctionType(info));
    mlir::Block *moduleBody = theModule.getBody();
    if (function && function->getBlock() == moduleBody &&
        function.getOperation() != &moduleBody->back())
      function->moveAfter(&moduleBody->back());
  }
}

void CIRGenModule::emitSelectedVariables(const DeclContext *context) {
  if (!selectedDeclRootMode)
    return;

  llvm::SmallVector<const DeclContext *, 32> contexts;
  llvm::DenseSet<const DeclContext *> visitedContexts;
  llvm::SmallVector<const ClassTemplateDecl *, 16> classTemplates;
  llvm::DenseSet<const ClassTemplateDecl *> visitedClassTemplates;
  llvm::DenseSet<const ClassTemplateSpecializationDecl *>
      visitedClassSpecializations;
  llvm::SmallVector<const VarTemplateDecl *, 16> varTemplates;
  llvm::DenseSet<const VarTemplateDecl *> visitedVarTemplates;
  llvm::DenseSet<const VarTemplateSpecializationDecl *>
      visitedVarSpecializations;
  llvm::SmallVector<GlobalDecl, 32> variables;
  llvm::DenseSet<GlobalDecl> visitedVariables;

  auto enqueueContext = [&](const DeclContext *declContext) {
    if (visitedContexts.insert(declContext).second)
      contexts.push_back(declContext);
  };
  auto enqueueVariable = [&](const VarDecl *variable) {
    if (isSelectedVariableTemplatePattern(variable) &&
        !isSelectedConstantVariableTemplatePattern(variable))
      return;
    if (!variable->isFileVarDecl() && !variable->isStaticDataMember())
      return;
    if (variable->isThisDeclarationADefinition() ==
            VarDecl::DeclarationOnly &&
        !astContext.isMSStaticDataMemberInlineDefinition(variable) &&
        !isSelectedStaticDataMemberDeclaration(variable))
      return;
    GlobalDecl gd(variable);
    if (!isSelectedDeclRoot(gd))
      return;
    if (visitedVariables.insert(gd.getCanonicalDecl()).second)
      variables.push_back(gd);
  };
  auto enqueueClassTemplate = [&](const ClassTemplateDecl *decl) {
    if (visitedClassTemplates.insert(decl).second)
      classTemplates.push_back(decl);
  };
  auto enqueueVarTemplate = [&](const VarTemplateDecl *decl) {
    if (visitedVarTemplates.insert(decl).second)
      varTemplates.push_back(decl);
  };

  enqueueContext(context);
  size_t contextCursor = 0;
  size_t variableCursor = 0;
  for (;;) {
    while (contextCursor < contexts.size()) {
      llvm::SmallVector<Decl *, 32> decls;
      for (Decl *decl : contexts[contextCursor++]->decls())
        decls.push_back(decl);

      for (Decl *decl : decls) {
        if (auto *variable = dyn_cast<VarDecl>(decl))
          enqueueVariable(variable);
        if (auto *classTemplate = dyn_cast<ClassTemplateDecl>(decl))
          enqueueClassTemplate(classTemplate);
        if (auto *varTemplate = dyn_cast<VarTemplateDecl>(decl))
          enqueueVarTemplate(varTemplate);
        if (auto *nested = dyn_cast<DeclContext>(decl))
          enqueueContext(nested);
      }
    }

    bool foundSpecialization = false;
    for (const ClassTemplateDecl *classTemplate : classTemplates) {
      llvm::SmallVector<ClassTemplateSpecializationDecl *, 16> specializations;
      for (ClassTemplateSpecializationDecl *specialization :
           classTemplate->specializations())
        specializations.push_back(specialization);
      for (ClassTemplateSpecializationDecl *specialization : specializations) {
        if (isa<ClassTemplatePartialSpecializationDecl>(specialization) ||
            !specialization->isCompleteDefinition() ||
            !visitedClassSpecializations.insert(specialization).second)
          continue;
        foundSpecialization = true;
        enqueueContext(specialization);
      }
    }
    for (const VarTemplateDecl *varTemplate : varTemplates) {
      llvm::SmallVector<VarTemplateSpecializationDecl *, 16> specializations;
      for (VarTemplateSpecializationDecl *specialization :
           varTemplate->specializations())
        specializations.push_back(specialization);
      for (VarTemplateSpecializationDecl *specialization : specializations) {
        if ((specialization->isThisDeclarationADefinition() !=
                 VarDecl::Definition &&
             !astContext.isMSStaticDataMemberInlineDefinition(
                 specialization)) ||
            !visitedVarSpecializations.insert(specialization).second)
          continue;
        foundSpecialization = true;
        enqueueVariable(specialization);
      }
    }

    bool madeProgress = false;
    if (variableCursor != variables.size()) {
      llvm::SmallVector<GlobalDecl, 32> frontier(
          variables.begin() + variableCursor, variables.end());
      variableCursor = variables.size();
      for (GlobalDecl gd : frontier) {
        auto global = mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(
            getGlobalValue(getMangledName(gd)));
        if (global && global.isDefinition()) {
          noteSelectedDeclRootDefinition(gd);
          continue;
        }
        const auto *variable = cast<VarDecl>(gd.getDecl());
        if (variable->isThisDeclarationADefinition() ==
            VarDecl::TentativeDefinition) {
          emitTentativeDefinition(variable);
          noteSelectedDeclRootDefinition(gd);
        } else {
          emitGlobal(gd);
        }
      }
      madeProgress = true;
    }

    if (contextCursor < contexts.size() || foundSpecialization)
      continue;
    if (!madeProgress)
      break;
  }
}

void CIRGenModule::emitSelectedDependencies(
    llvm::ArrayRef<GlobalDecl> dependencies) {
  if (!selectedDeclRootMode)
    return;

  bool wasEmittingSelectedDeclDependency = emittingSelectedDeclDependency;
  emittingSelectedDeclDependency = true;
  auto emitExactDefinition = [&](GlobalDecl gd) {
    attemptedSelectedDeclDependencies.insert(gd.getCanonicalDecl());
    const auto *function = dyn_cast<FunctionDecl>(gd.getDecl());
    if (function && !function->hasAttr<AliasAttr>())
      emitGlobalDecl(gd);
    else
      emitGlobal(gd);
  };
  for (GlobalDecl gd : dependencies) {
    gd = getEmitCapableSelectedDecl(gd);
    if (const auto *ctor = dyn_cast<CXXConstructorDecl>(gd.getDecl());
        ctor && ctor->getParent()->getNumVBases() != 0) {
      if (selectedConstructorFamilies.insert(ctor->getCanonicalDecl()).second) {
        getCXXABI().emitCXXConstructors(ctor);
      } else {
        auto global = mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(
            getGlobalValue(getMangledName(gd)));
        if (!global || !global.isDefinition())
          emitExactDefinition(gd);
      }
      continue;
    }
    if (const auto *dtor = dyn_cast<CXXDestructorDecl>(gd.getDecl())) {
      if (dtor->isTrivial() && !dtor->hasAttr<DLLExportAttr>())
        continue;
      if (selectedDestructorFamilies.insert(dtor->getCanonicalDecl()).second) {
        getCXXABI().emitCXXDestructors(dtor);
      } else {
        auto global = mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(
            getGlobalValue(getMangledName(gd)));
        if (!global || !global.isDefinition())
          emitExactDefinition(gd);
      }
      continue;
    }
    auto global = mlir::dyn_cast_or_null<cir::CIRGlobalValueInterface>(
        getGlobalValue(getMangledName(gd)));
    bool hasExactDefinition = global && global.isDefinition();
    if (hasExactDefinition)
      if (auto function = mlir::dyn_cast<cir::FuncOp>(global.getOperation())) {
        llvm::SmallString<256> usr;
        hasExactDefinition =
            !clang::index::generateUSRForDecl(
                gd.getCanonicalDecl().getDecl(), usr) &&
            functionCarriesExactDeclUSR(function, usr);
      }
    if (!hasExactDefinition)
      emitExactDefinition(gd);
  }
  emittingSelectedDeclDependency = wasEmittingSelectedDeclDependency;
  emitDeferred();
}
void CIRGenModule::emitSelectedDeclDependencyClosure(
    llvm::function_ref<void(llvm::MutableArrayRef<GlobalDecl>)>
        prepareForEmission) {
  if (!selectedDeclRootMode)
    return;

  for (;;) {
    // Vtables and constant initializers can materialize exact CIR symbol
    // references without an active CIRGenFunction. Their producer-owned
    // GlobalDecls are enqueued by the address-creation paths above.
    emitDeferred();
    emitVTablesOpportunistically();

    llvm::SmallVector<GlobalDecl, 16> dependencies =
        takeSelectedDeclDependencyFrontier();
    if (dependencies.empty())
      break;
    prepareForEmission(dependencies);
    emitSelectedDependencies(dependencies);
  }
}

StringRef CIRGenModule::getMangledName(GlobalDecl gd) {
  GlobalDecl canonicalGd = gd.getCanonicalDecl();

  // Some ABIs don't have constructor variants. Make sure that base and complete
  // constructors get mangled the same.
  if (const auto *cd = dyn_cast<CXXConstructorDecl>(canonicalGd.getDecl())) {
    if (!getTarget().getCXXABI().hasConstructorVariants()) {
      errorNYI(cd->getSourceRange(),
               "getMangledName: C++ constructor without variants");
      return cast<NamedDecl>(gd.getDecl())->getIdentifier()->getName();
    }
  }

  // Keep the first result in the case of a mangling collision.
  const auto *nd = cast<NamedDecl>(gd.getDecl());
  std::string mangledName = getMangledNameImpl(*this, gd, nd);

  auto result = manglings.insert(std::make_pair(mangledName, gd));
  return mangledDeclNames[canonicalGd] = result.first->first();
}

void CIRGenModule::emitTentativeDefinition(const VarDecl *d) {
  assert(!d->getInit() && "Cannot emit definite definitions here!");

  StringRef mangledName = getMangledName(d);
  mlir::Operation *gv = getGlobalValue(mangledName);

  // If we already have a definition, not declaration, with the same mangled
  // name, emitting of declaration is not required (and would actually overwrite
  // the emitted definition).
  if (gv && !mlir::cast<cir::GlobalOp>(gv).isDeclaration())
    return;

  // If we have not seen a reference to this variable yet, place it into the
  // deferred declarations table to be emitted if needed later.
  if (!mustBeEmitted(d) && !gv) {
    deferredDecls[mangledName] = d;
    return;
  }

  // The tentative definition is the only definition.
  emitGlobalVarDefinition(d);
}

bool CIRGenModule::mustBeEmitted(const ValueDecl *global) {
  if (selectedDeclRootMode) {
    if (const auto *ctor = dyn_cast<CXXConstructorDecl>(global))
      return isSelectedDeclRoot(GlobalDecl(ctor, Ctor_Complete)) ||
             isSelectedDeclRoot(GlobalDecl(ctor, Ctor_Base));
    if (const auto *dtor = dyn_cast<CXXDestructorDecl>(global))
      return isSelectedDeclRoot(GlobalDecl(dtor, Dtor_Deleting)) ||
             isSelectedDeclRoot(GlobalDecl(dtor, Dtor_Complete)) ||
             isSelectedDeclRoot(GlobalDecl(dtor, Dtor_Base));
    return isSelectedDeclRoot(GlobalDecl(global));
  }

  // Never defer when EmitAllDecls is specified.
  if (langOpts.EmitAllDecls)
    return true;

  const auto *vd = dyn_cast<VarDecl>(global);
  if (vd &&
      ((codeGenOpts.KeepPersistentStorageVariables &&
        (vd->getStorageDuration() == SD_Static ||
         vd->getStorageDuration() == SD_Thread)) ||
       (codeGenOpts.KeepStaticConsts && vd->getStorageDuration() == SD_Static &&
        vd->getType().isConstQualified())))
    return true;

  return getASTContext().DeclMustBeEmitted(global);
}

bool CIRGenModule::mayBeEmittedEagerly(const ValueDecl *global) {
  // In OpenMP 5.0 variables and function may be marked as
  // device_type(host/nohost) and we should not emit them eagerly unless we sure
  // that they must be emitted on the host/device. To be sure we need to have
  // seen a declare target with an explicit mentioning of the function, we know
  // we have if the level of the declare target attribute is -1. Note that we
  // check somewhere else if we should emit this at all.
  if (langOpts.OpenMP >= 50 && !langOpts.OpenMPSimd) {
    std::optional<OMPDeclareTargetDeclAttr *> activeAttr =
        OMPDeclareTargetDeclAttr::getActiveAttr(global);
    if (!activeAttr || (*activeAttr)->getLevel() != (unsigned)-1)
      return false;
  }

  const auto *fd = dyn_cast<FunctionDecl>(global);
  if (fd) {
    // Implicit template instantiations may change linkage if they are later
    // explicitly instantiated, so they should not be emitted eagerly.
    if (fd->getTemplateSpecializationKind() == TSK_ImplicitInstantiation)
      return false;
    // Defer until all versions have been semantically checked.
    if (fd->hasAttr<TargetVersionAttr>() && !fd->isMultiVersion())
      return false;
    if (langOpts.SYCLIsDevice) {
      errorNYI(fd->getSourceRange(), "mayBeEmittedEagerly: SYCL");
      return false;
    }
  }
  const auto *vd = dyn_cast<VarDecl>(global);
  if (vd)
    if (astContext.getInlineVariableDefinitionKind(vd) ==
        ASTContext::InlineVariableDefinitionKind::WeakUnknown)
      // A definition of an inline constexpr static data member may change
      // linkage later if it's redeclared outside the class.
      return false;

  // If OpenMP is enabled and threadprivates must be generated like TLS, delay
  // codegen for global variables, because they may be marked as threadprivate.
  if (langOpts.OpenMP && langOpts.OpenMPUseTLS &&
      astContext.getTargetInfo().isTLSSupported() && isa<VarDecl>(global) &&
      !global->getType().isConstantStorage(astContext, false, false) &&
      !OMPDeclareTargetDeclAttr::isDeclareTargetDeclaration(global))
    return false;

  assert((fd || vd) &&
         "Only FunctionDecl and VarDecl should hit this path so far.");
  return true;
}

static bool shouldAssumeDSOLocal(const CIRGenModule &cgm,
                                 cir::CIRGlobalValueInterface gv) {
  if (gv.hasLocalLinkage())
    return true;

  if (!gv.hasDefaultVisibility() && !gv.hasExternalWeakLinkage())
    return true;

  // DLLImport explicitly marks the GV as external.
  // so it shouldn't be dso_local
  // But we don't have the info set now
  assert(!cir::MissingFeatures::opGlobalDLLImportExport());

  const llvm::Triple &tt = cgm.getTriple();
  const CodeGenOptions &cgOpts = cgm.getCodeGenOpts();
  if (tt.isOSCygMing()) {
    // In MinGW and Cygwin, variables without DLLImport can still be
    // automatically imported from a DLL by the linker; don't mark variables
    // that potentially could come from another DLL as DSO local.

    // With EmulatedTLS, TLS variables can be autoimported from other DLLs
    // (and this actually happens in the public interface of libstdc++), so
    // such variables can't be marked as DSO local. (Native TLS variables
    // can't be dllimported at all, though.)
    cgm.errorNYI("shouldAssumeDSOLocal: MinGW");
  }

  // On COFF, don't mark 'extern_weak' symbols as DSO local. If these symbols
  // remain unresolved in the link, they can be resolved to zero, which is
  // outside the current DSO.
  if (tt.isOSBinFormatCOFF() && gv.hasExternalWeakLinkage())
    return false;

  // Every other GV is local on COFF.
  // Make an exception for windows OS in the triple: Some firmware builds use
  // *-win32-macho triples. This (accidentally?) produced windows relocations
  // without GOT tables in older clang versions; Keep this behaviour.
  // FIXME: even thread local variables?
  if (tt.isOSBinFormatCOFF() || (tt.isOSWindows() && tt.isOSBinFormatMachO()))
    return true;

  // Only handle COFF and ELF for now.
  if (!tt.isOSBinFormatELF())
    return false;

  llvm::Reloc::Model rm = cgOpts.RelocationModel;
  const LangOptions &lOpts = cgm.getLangOpts();
  if (rm != llvm::Reloc::Static && !lOpts.PIE) {
    // On ELF, if -fno-semantic-interposition is specified and the target
    // supports local aliases, there will be neither CC1
    // -fsemantic-interposition nor -fhalf-no-semantic-interposition. Set
    // dso_local on the function if using a local alias is preferable (can avoid
    // PLT indirection).
    if (!(isa<cir::FuncOp>(gv) && gv.canBenefitFromLocalAlias()))
      return false;
    return !(lOpts.SemanticInterposition || lOpts.HalfNoSemanticInterposition);
  }

  // A definition cannot be preempted from an executable.
  if (!gv.isDeclarationForLinker())
    return true;

  // Most PIC code sequences that assume that a symbol is local cannot produce a
  // 0 if it turns out the symbol is undefined. While this is ABI and relocation
  // depended, it seems worth it to handle it here.
  if (rm == llvm::Reloc::PIC_ && gv.hasExternalWeakLinkage())
    return false;

  // PowerPC64 prefers TOC indirection to avoid copy relocations.
  if (tt.isPPC64())
    return false;

  if (cgOpts.DirectAccessExternalData) {
    // If -fdirect-access-external-data (default for -fno-pic), set dso_local
    // for non-thread-local variables. If the symbol is not defined in the
    // executable, a copy relocation will be needed at link time. dso_local is
    // excluded for thread-local variables because they generally don't support
    // copy relocations.
    if (auto globalOp = dyn_cast<cir::GlobalOp>(gv.getOperation())) {
      // Assume variables are not thread-local until that support is added.
      assert(!cir::MissingFeatures::opGlobalThreadLocal());
      return true;
    }

    // -fno-pic sets dso_local on a function declaration to allow direct
    // accesses when taking its address (similar to a data symbol). If the
    // function is not defined in the executable, a canonical PLT entry will be
    // needed at link time. -fno-direct-access-external-data can avoid the
    // canonical PLT entry. We don't generalize this condition to -fpie/-fpic as
    // it could just cause trouble without providing perceptible benefits.
    if (isa<cir::FuncOp>(gv) && !cgOpts.NoPLT && rm == llvm::Reloc::Static)
      return true;
  }

  // If we can use copy relocations we can assume it is local.

  // Otherwise don't assume it is local.

  return false;
}

void CIRGenModule::setGlobalVisibility(cir::CIRGlobalValueInterface gv,
                                       const NamedDecl *d) const {
  // Internal definitions always have default visibility.
  if (gv.hasLocalLinkage()) {
    gv.setGlobalVisibility(cir::VisibilityKind::Default);
    return;
  }
  if (!d)
    return;

  // Set visibility for definitions, and for declarations if requested globally
  // or set explicitly.
  LinkageInfo lv = d->getLinkageAndVisibility();

  // OpenMP declare target variables must be visible to the host so they can
  // be registered. We require protected visibility unless the variable has
  // the DT_nohost modifier and does not need to be registered.
  if (getASTContext().getLangOpts().OpenMP &&
      getASTContext().getLangOpts().OpenMPIsTargetDevice && isa<VarDecl>(d) &&
      d->hasAttr<OMPDeclareTargetDeclAttr>() &&
      d->getAttr<OMPDeclareTargetDeclAttr>()->getDevType() !=
          OMPDeclareTargetDeclAttr::DT_NoHost &&
      lv.getVisibility() == HiddenVisibility) {
    llvm_unreachable("setGlobalVisibility: OpenMP is NYI");
    return;
  }

  // CUDA/HIP device kernels and global variables must be visible to the host
  // so they can be registered / initialized. We require protected visibility
  // unless the user explicitly requested hidden via an attribute.
  if (getASTContext().getLangOpts().CUDAIsDevice &&
      lv.getVisibility() == HiddenVisibility && !lv.isVisibilityExplicit() &&
      !d->hasAttr<OMPDeclareTargetDeclAttr>()) {
    bool needsProtected = false;
    if (isa<FunctionDecl>(d)) {
      needsProtected =
          d->hasAttr<CUDAGlobalAttr>() || d->hasAttr<DeviceKernelAttr>();
    } else if (const auto *vd = dyn_cast<VarDecl>(d)) {
      needsProtected = vd->hasAttr<CUDADeviceAttr>() ||
                       vd->hasAttr<CUDAConstantAttr>() ||
                       vd->getType()->isCUDADeviceBuiltinSurfaceType() ||
                       vd->getType()->isCUDADeviceBuiltinTextureType();
    }
    if (needsProtected) {
      gv.setGlobalVisibility(cir::VisibilityKind::Protected);
      return;
    }
  }

  if (getASTContext().getLangOpts().HLSL && !d->isInExportDeclContext()) {
    gv.setGlobalVisibility(cir::VisibilityKind::Hidden);
    return;
  }

  assert(!cir::MissingFeatures::opGlobalDLLImportExport());

  if (lv.isVisibilityExplicit() || getLangOpts().SetVisibilityForExternDecls ||
      !gv.isDeclarationForLinker())
    gv.setGlobalVisibility(getCIRVisibilityKind(lv.getVisibility()));
}

void CIRGenModule::setDSOLocal(cir::CIRGlobalValueInterface gv) const {
  gv.setDSOLocal(shouldAssumeDSOLocal(*this, gv));
}

void CIRGenModule::setDSOLocal(mlir::Operation *op) const {
  if (auto globalValue = dyn_cast<cir::CIRGlobalValueInterface>(op))
    setDSOLocal(globalValue);
}

void CIRGenModule::setGVProperties(mlir::Operation *op,
                                   const NamedDecl *d) const {
  assert(!cir::MissingFeatures::opGlobalDLLImportExport());
  setGVPropertiesAux(op, d);
}

void CIRGenModule::setGVPropertiesAux(mlir::Operation *op,
                                      const NamedDecl *d) const {
  setGlobalVisibility(cast<cir::CIRGlobalValueInterface>(op), d);
  setDSOLocal(op);
  assert(!cir::MissingFeatures::opGlobalPartition());
}

bool CIRGenModule::lookupRepresentativeDecl(StringRef mangledName,
                                            GlobalDecl &result) const {
  auto res = manglings.find(mangledName);
  if (res == manglings.end())
    return false;
  result = res->getValue();
  return true;
}

cir::TLS_Model CIRGenModule::getDefaultCIRTLSModel() const {
  switch (getCodeGenOpts().getDefaultTLSModel()) {
  case CodeGenOptions::GeneralDynamicTLSModel:
    return cir::TLS_Model::GeneralDynamic;
  case CodeGenOptions::LocalDynamicTLSModel:
    return cir::TLS_Model::LocalDynamic;
  case CodeGenOptions::InitialExecTLSModel:
    return cir::TLS_Model::InitialExec;
  case CodeGenOptions::LocalExecTLSModel:
    return cir::TLS_Model::LocalExec;
  }
  llvm_unreachable("Invalid TLS model!");
}

static cir::TLS_Model GetCIRTLSModel(StringRef S) {
  return llvm::StringSwitch<cir::TLS_Model>(S)
      .Case("global-dynamic", cir::TLS_Model::GeneralDynamic)
      .Case("local-dynamic", cir::TLS_Model::LocalDynamic)
      .Case("initial-exec", cir::TLS_Model::InitialExec)
      .Case("local-exec", cir::TLS_Model::LocalExec);
}

void CIRGenModule::setTLSMode(mlir::Operation *op, const VarDecl &d,
                              bool isExtendingDecl) {
  assert(d.getTLSKind() && "setting TLS mode on non-TLS var!");

  cir::TLS_Model tlm = getDefaultCIRTLSModel();

  // Override the TLS model if it is explicitly specified.
  if (const TLSModelAttr *attr = d.getAttr<TLSModelAttr>())
    tlm = GetCIRTLSModel(attr->getModel());

  auto global = cast<cir::GlobalOp>(op);
  global.setTlsModel(tlm);

  // For namespace-scope dyanmic TLS we need to set the wrapper, int, or guard
  // info.
  if (d.isStaticLocal() || tlm != cir::TLS_Model::GeneralDynamic)
    return;

  // If this function was called to set the TLS mode for a temporary whose
  // lifetime is extended by the variable declared by `d`, don't emit the
  // wrapper, init, and guard info.
  if (isExtendingDecl)
    return;

  setGlobalTlsReferences(d, global);
}
void CIRGenModule::setCIRFunctionAttributes(GlobalDecl globalDecl,
                                            const CIRGenFunctionInfo &info,
                                            cir::FuncOp func, bool isThunk) {
  const auto *emissionDecl = globalDecl.getDecl();
  const auto *emissionFunctionDecl =
      dyn_cast_or_null<FunctionDecl>(emissionDecl);
  // Selected-declaration closure discovery can first enqueue a canonical
  // function-template specialization and later replace the worklist entry
  // with its defining redeclaration. Keep the semantic identity anchored to
  // the canonical instantiated FunctionDecl across that emission rewrite;
  // the exact GlobalDecl linkage name below remains the ABI endpoint fact.
  const auto *identityFunctionDecl =
      emissionFunctionDecl ? emissionFunctionDecl->getCanonicalDecl() : nullptr;
  const Decl *decl = identityFunctionDecl
                         ? static_cast<const Decl *>(identityFunctionDecl)
                         : emissionDecl;
  llvm::SmallString<256> astDeclUSR;
  bool hasASTDeclUSR =
      decl && !clang::index::generateUSRForDecl(decl, astDeclUSR);
  if (hasASTDeclUSR)
    func->setAttr("ast_decl_usr", builder.getStringAttr(astDeclUSR));
  if (identityFunctionDecl) {
    const std::string sourceName = identityFunctionDecl->getNameAsString();
    if (!sourceName.empty())
      func->setAttr("ast_decl_source_name", builder.getStringAttr(sourceName));
  }
  if (const auto *method =
          dyn_cast_or_null<CXXMethodDecl>(identityFunctionDecl);
      method && !isThunk && !isa<CXXConstructorDecl>(method) &&
      !isa<CXXDestructorDecl>(method)) {
    auto exact = buildCIRGenVirtualMethodIdentityAttrs(
        *this, getMLIRContext(), globalDecl);
    mlir::NamedAttrList identity;
    identity.set("method_symbol", exact.method);
    identity.set("is_virtual", builder.getBoolAttr(method->isVirtual()));
    if (exact.methodUSR)
      identity.set("method_usr", exact.methodUSR);
    if (auto declaringClass = getRecordUSRAttr(method->getParent()))
      identity.set("method_declaring_class_usr", declaringClass);
    if (exact.rootMethodUSR)
      identity.set("virtual_root_method_usr", exact.rootMethodUSR);
    if (exact.declaringClassUSR)
      identity.set("virtual_root_declaring_class_usr",
                   exact.declaringClassUSR);
    if (exact.rootAlternatives)
      identity.set("virtual_root_alternatives", exact.rootAlternatives);
    func->setAttr("ast_method_callable_identity",
                  identity.getDictionary(&getMLIRContext()));
  }
  // The USR authenticates the canonical source declaration, while the exact
  // GlobalDecl linkage name authenticates the public ABI endpoint from which
  // this operation was produced. Inline builtin emission may move the body to
  // a distinct private symbol; retaining both facts lets consumers distinguish
  // that definition from the declaration-only public alias without decoding
  // either symbol spelling.
  if (decl)
    func->setAttr("ast_decl_linkage_name",
                  builder.getStringAttr(getMangledName(globalDecl)));
  if (isa<CXXConstructorDecl>(decl)) {
    llvm::StringRef variant;
    switch (globalDecl.getCtorType()) {
    case Ctor_Complete:
      variant = "complete";
      break;
    case Ctor_Base:
      variant = "base";
      break;
    case Ctor_Comdat:
      variant = "comdat";
      break;
    case Ctor_CopyingClosure:
      variant = "copying_closure";
      break;
    case Ctor_DefaultClosure:
      variant = "default_closure";
      break;
    case Ctor_Unified:
      variant = "unified";
      break;
    }
    func->setAttr("abi_ctor_variant", builder.getStringAttr(variant));
    func->setAttr(
        "abi_has_vtt",
        builder.getBoolAttr(getCXXABI().needsVTTParameter(globalDecl)));
  } else if (isa<CXXDestructorDecl>(decl)) {
    llvm::StringRef variant;
    switch (globalDecl.getDtorType()) {
    case Dtor_Deleting:
      variant = "deleting";
      break;
    case Dtor_Complete:
      variant = "complete";
      break;
    case Dtor_Base:
      variant = "base";
      break;
    case Dtor_Comdat:
      variant = "comdat";
      break;
    case Dtor_Unified:
      variant = "unified";
      break;
    case Dtor_VectorDeleting:
      variant = "vector_deleting";
      break;
    }
    func->setAttr("abi_dtor_variant", builder.getStringAttr(variant));
    func->setAttr(
        "abi_has_vtt",
        builder.getBoolAttr(getCXXABI().needsVTTParameter(globalDecl)));
  }
  // Declaration identity is owned by the compiler USR and the canonical AST
  // declaration. CIR does not mint a source-location discriminator here;
  // consumers that need a local structural discriminator compute it from the
  // exact AST declaration chain.

  // A thunk has no FunctionDecl of its own. GlobalDecl is the exact ABI target
  // supplied by Clang's thunk emission path; preserve it explicitly rather
  // than requiring a consumer to decode the thunk symbol.
  if (isThunk && identityFunctionDecl) {
    mlir::NamedAttrList identity;
    identity.set("mangled_name",
                 builder.getStringAttr(getMangledName(globalDecl)));
    if (hasASTDeclUSR)
      identity.set("usr", builder.getStringAttr(astDeclUSR));
    func->setAttr("ast_thunk_target_identity",
                  identity.getDictionary(&getMLIRContext()));
  }

  // Preserve a concrete template specialization's exact producer symbol,
  // declaration USR, and template-pattern USR. Consumers may use the USR pair
  // only when both producer and consumer prove it unique.
  const FunctionDecl *templateInstantiationPattern =
      identityFunctionDecl
          ? identityFunctionDecl->getTemplateInstantiationPattern()
          : nullptr;
  const bool isConcreteClassTemplateMemberInstantiation =
      templateInstantiationPattern &&
      templateInstantiationPattern != identityFunctionDecl;
  if (identityFunctionDecl &&
      (identityFunctionDecl->isFunctionTemplateSpecialization() ||
       isConcreteClassTemplateMemberInstantiation)) {
    mlir::NamedAttrList identity;
    identity.set("mangled_name", builder.getStringAttr(func.getSymName()));
    if (hasASTDeclUSR)
      identity.set("usr", builder.getStringAttr(astDeclUSR));

    llvm::SmallString<256> patternUSR;
    if (templateInstantiationPattern &&
        !clang::index::generateUSRForDecl(
            templateInstantiationPattern->getCanonicalDecl(), patternUSR)) {
      identity.set("template_pattern_usr", builder.getStringAttr(patternUSR));
      if (auto poi = specializationPointOfInstantiationIdentity(
              getASTContext(), identityFunctionDecl))
        identity.set("poi", builder.getStringAttr(*poi));
    }

    func->setAttr("ast_decl_specialization_identity",
                  identity.getDictionary(&getMLIRContext()));
    // The pattern USR spelled at emission time can come from an earlier
    // redeclaration whose written types print differently (dependent
    // expressions change shape with the declarations visible at their parse
    // point). Remember the exact instantiation decl so release() can
    // reconcile the identity against the final AST.
    specializationIdentityDeclByOperation[func.getOperation()] =
        identityFunctionDecl;
  }
  const FunctionDecl *functionDecl = emissionFunctionDecl;
  auto memberPointerFacts = [&](QualType type,
                                const FunctionDecl *resultDecl =
                                    nullptr) -> mlir::DictionaryAttr {
    const auto *mpt = type.getCanonicalType()->getAs<MemberPointerType>();
    if (!mpt)
      return {};
    const CXXRecordDecl *record = mpt->getMostRecentCXXRecordDecl();
    const std::optional<std::string> recordID =
        record ? recordDeclIdentity(*this, record) : std::nullopt;
    if (!recordID.has_value() || recordID->empty())
      return {};
    const TypeInfo adjustmentInfo =
        getASTContext().getTypeInfo(getASTContext().getPointerDiffType());
    if (!adjustmentInfo.Width || !adjustmentInfo.Align)
      return {};
    mlir::NamedAttrList facts;
    facts.set("target_record_usr", builder.getStringAttr(*recordID));
    if (!mpt->isMemberFunctionPointer()) {
      if (getASTContext().getTargetInfo().getCXXABI().isMicrosoft())
        return {};
      facts.set("kind", builder.getStringAttr("data"));
      auto cirType = mlir::cast<cir::DataMemberType>(convertType(type));
      facts.set("class_type", mlir::TypeAttr::get(cirType.getClassTy()));
      facts.set("pointee_type", mlir::TypeAttr::get(cirType.getMemberTy()));
      std::string provenance = "runtime_projection";
      std::optional<std::string> fieldID;
      if (resultDecl && resultDecl->hasBody()) {
        bool sawReturn = false;
        bool closed = true;
        std::string commonKind;
        std::optional<std::string> commonFieldID;
        std::function<void(const Stmt *)> visit = [&](const Stmt *stmt) {
          if (!stmt || isa<LambdaExpr>(stmt))
            return;
          if (const auto *ret = dyn_cast<ReturnStmt>(stmt)) {
            std::string kind = "runtime_projection";
            std::optional<std::string> returnedFieldID;
            const Expr *value = ret->getRetValue();
            if (value) {
              value = value->IgnoreParens();
              if (value->isNullPointerConstant(
                      getASTContext(), Expr::NPC_ValueDependentIsNull)) {
                kind = "null";
              } else if (const auto *address = dyn_cast<UnaryOperator>(value);
                         address && address->getOpcode() == UO_AddrOf) {
                const Expr *operand =
                    address->getSubExpr()->IgnoreParenImpCasts();
                if (const auto *reference = dyn_cast<DeclRefExpr>(operand)) {
                  if (const auto *field =
                          dyn_cast<FieldDecl>(reference->getDecl())) {
                    returnedFieldID = fieldDeclIdentity(*this, field);
                    if (returnedFieldID.has_value() &&
                        !returnedFieldID->empty())
                      kind = "field_decl";
                  }
                }
              }
            }
            if (!sawReturn) {
              sawReturn = true;
              commonKind = kind;
              commonFieldID = returnedFieldID;
            } else if (commonKind != kind || commonFieldID != returnedFieldID) {
              closed = false;
            }
            return;
          }
          for (const Stmt *child : stmt->children())
            visit(child);
        };
        visit(resultDecl->getBody());
        if (sawReturn && closed) {
          provenance = commonKind;
          fieldID = std::move(commonFieldID);
        }
      }
      facts.set("provenance_kind", builder.getStringAttr(provenance));
      if (provenance == "field_decl")
        facts.set("field_decl_id", builder.getStringAttr(*fieldID));
      facts.set("carrier_bits",
                builder.getI64IntegerAttr(adjustmentInfo.Width));
      facts.set("carrier_align_bits",
                builder.getI64IntegerAttr(adjustmentInfo.Align));
      facts.set("carrier_signed", builder.getBoolAttr(true));
      facts.set("null_value", builder.getStringAttr("-1"));
      return facts.getDictionary(&getMLIRContext());
    }
    const TypeInfo pointerInfo =
        getASTContext().getTypeInfo(getASTContext().VoidPtrTy);
    if (!pointerInfo.Width || !pointerInfo.Align)
      return {};
    facts.set("kind", builder.getStringAttr("function"));
    facts.set("pointee_function_type",
              builder.getStringAttr(mpt->getPointeeType().getAsString()));
    facts.set("pointer_bits", builder.getI64IntegerAttr(pointerInfo.Width));
    facts.set("pointer_align_bits",
              builder.getI64IntegerAttr(pointerInfo.Align));
    facts.set("adjustment_bits",
              builder.getI64IntegerAttr(adjustmentInfo.Width));
    facts.set("adjustment_align_bits",
              builder.getI64IntegerAttr(adjustmentInfo.Align));
    facts.set("adjustment_signed", builder.getBoolAttr(true));
    facts.set("null_function_value", builder.getStringAttr("0"));
    facts.set("null_adjustment_value", builder.getStringAttr("0"));
    return facts.getDictionary(&getMLIRContext());
  };
  if (functionDecl) {
    auto sourceTypeLayer = [&](QualType type, StringRef kind,
                               bool referenceStorage) {
      mlir::NamedAttrList layer;
      layer.set("kind", builder.getStringAttr(kind));
      const Qualifiers qualifiers = type.getQualifiers();
      layer.set("is_const", builder.getBoolAttr(qualifiers.hasConst()));
      layer.set("is_volatile", builder.getBoolAttr(qualifiers.hasVolatile()));
      layer.set("is_restrict", builder.getBoolAttr(qualifiers.hasRestrict()));
      layer.set("is_atomic", builder.getBoolAttr(type->isAtomicType()));
      layer.set("clang_address_space",
                builder.getI64IntegerAttr(
                    static_cast<uint64_t>(qualifiers.getAddressSpace())));
      layer.set("target_address_space",
                builder.getI64IntegerAttr(getASTContext().getTargetAddressSpace(
                    qualifiers.getAddressSpace())));
      QualType layoutType = referenceStorage ? getASTContext().VoidPtrTy : type;
      if (!layoutType->isIncompleteType() && !layoutType->isFunctionType() &&
          !layoutType->isVoidType()) {
        const TypeInfo info = getASTContext().getTypeInfo(layoutType);
        layer.set("bit_width", builder.getI64IntegerAttr(info.Width));
        layer.set("align_bits", builder.getI64IntegerAttr(info.Align));
      }
      if (type->isIntegerType() || type->isEnumeralType()) {
        layer.set("is_signed", builder.getBoolAttr(
                                   type->isSignedIntegerOrEnumerationType()));
      }
      return layer.getDictionary(&getMLIRContext());
    };
    auto sourceType = [&](QualType type) -> std::optional<mlir::ArrayAttr> {
      llvm::SmallVector<mlir::Attribute, 4> layers;
      QualType current = type;
      QualType leaf = current;
      while (leaf->isPointerType() || leaf->isReferenceType())
        leaf = leaf->getPointeeType();
      if (!leaf->isObjCObjectPointerType())
        return buildCastEndpointSourceType(type);
      while (true) {
        if (current->isLValueReferenceType()) {
          layers.push_back(sourceTypeLayer(current, "lvalue_reference", true));
          current = current->getPointeeType();
          continue;
        }
        if (current->isRValueReferenceType()) {
          layers.push_back(sourceTypeLayer(current, "rvalue_reference", true));
          current = current->getPointeeType();
          continue;
        }
        if (current->isPointerType()) {
          layers.push_back(sourceTypeLayer(current, "pointer", false));
          current = current->getPointeeType();
          continue;
        }
        if (current->isObjCObjectPointerType()) {
          bool hasCompleteIdentity = true;
          mlir::DictionaryAttr objcFacts =
              getObjCSourceTypeFacts(*this, current, hasCompleteIdentity);
          if (!hasCompleteIdentity || !objcFacts)
            return std::nullopt;

          mlir::NamedAttrList layer;
          const mlir::DictionaryAttr genericFacts =
              sourceTypeLayer(current, "objc_object_pointer", false);
          for (mlir::NamedAttribute attribute : genericFacts.getValue())
            layer.set(attribute.getName(), attribute.getValue());
          layer.set("objc", objcFacts);
          layers.push_back(layer.getDictionary(&getMLIRContext()));
          break;
        }
        layers.push_back(sourceTypeLayer(current, "value", false));
        break;
      }
      return builder.getArrayAttr(layers);
    };
    if (functionDecl->getReturnType()->isObjCObjectPointerType()) {
      std::optional<mlir::ArrayAttr> returnSourceType =
          sourceType(functionDecl->getReturnType());
      if (!returnSourceType) {
        errorNYI(functionDecl->getSourceRange(),
                 "function result type without complete ObjC identity");
        return;
      }
      func->setAttr("ast_return_source_type", *returnSourceType);
    }

    const unsigned explicitParams = functionDecl->getNumParams();
    llvm::SmallVector<mlir::Attribute, 8> parameterSourceTypes;
    parameterSourceTypes.reserve(explicitParams + 1);
    if (const auto *method = dyn_cast<CXXMethodDecl>(functionDecl);
        method && !method->isStatic()) {
      std::optional<mlir::ArrayAttr> thisSourceType =
          sourceType(method->getThisType());
      if (!thisSourceType) {
        errorNYI(method->getSourceRange(),
                 "function object parameter without complete ObjC identity");
        return;
      }
      parameterSourceTypes.push_back(*thisSourceType);
    }
    for (unsigned index = 0; index < explicitParams; ++index) {
      std::optional<mlir::ArrayAttr> parameterSourceType =
          sourceType(functionDecl->getParamDecl(index)->getType());
      if (!parameterSourceType) {
        errorNYI(functionDecl->getParamDecl(index)->getSourceRange(),
                 "function parameter type without complete ObjC identity");
        return;
      }
      parameterSourceTypes.push_back(*parameterSourceType);
    }
    func->setAttr("ast_param_source_types",
                  builder.getArrayAttr(parameterSourceTypes));
  }
  if (const auto *method =
          dyn_cast_or_null<CXXMethodDecl>(globalDecl.getDecl());
      method && method->getParent() && method->getParent()->isLambda()) {
    const CXXRecordDecl *closure = method->getParent();
    llvm::SmallString<256> contextUSR;
    const Decl *context = closure->getLambdaContextDecl();
    bool haveContextUSR =
        context && !clang::index::generateUSRForDecl(context, contextUSR);
    if (!haveContextUSR) {
      contextUSR.clear();
      context = Decl::castFromDeclContext(closure->getDeclContext());
      haveContextUSR =
          context && !clang::index::generateUSRForDecl(context, contextUSR);
    }
    func->setAttr("ast_lambda_index", builder.getI32IntegerAttr(
                                          closure->getLambdaIndexInContext()));
    // getLambdaIndexInContext numbers closures within one context decl and
    // cannot separate sibling closures a macro spells from one argument.
    // The lexical ordinal among the owning function's unnamed tags is the
    // same source fact the record identity binds through #fnlocal.
    {
      const FunctionDecl *ordinalOwner = nullptr;
      for (const DeclContext *dc = closure->getDeclContext(); dc;
           dc = dc->getParent()) {
        if (const auto *fnOwner = dyn_cast<FunctionDecl>(dc)) {
          ordinalOwner = fnOwner;
          break;
        }
      }
      if (ordinalOwner && !ordinalOwner->isTemplated()) {
        if (std::optional<unsigned> ordinal =
                functionLocalUnnamedTagLexicalOrdinal(ordinalOwner, closure)) {
          func->setAttr("ast_lambda_ordinal",
                        builder.getI32IntegerAttr(*ordinal));
        }
      }
    }
    SourceLocation lambdaLocation =
        getASTContext().getSourceManager().getSpellingLoc(
            closure->getLocation());
    PresumedLoc lambdaPresumed =
        getASTContext().getSourceManager().getPresumedLoc(lambdaLocation);
    if (lambdaPresumed.isValid()) {
      func->setAttr("ast_lambda_file",
                    builder.getStringAttr(lambdaPresumed.getFilename()));
      func->setAttr("ast_lambda_line",
                    builder.getI32IntegerAttr(lambdaPresumed.getLine()));
      func->setAttr("ast_lambda_column",
                    builder.getI32IntegerAttr(lambdaPresumed.getColumn()));
    }
    if (haveContextUSR) {
      func->setAttr("ast_lambda_context_usr",
                    builder.getStringAttr(contextUSR));
    }
    const auto *contextFunction = dyn_cast_or_null<FunctionDecl>(
        Decl::castFromDeclContext(closure->getDeclContext()));
    if (contextFunction) {
      SourceLocation poi = getASTContext().getSourceManager().getExpansionLoc(
          contextFunction->getPointOfInstantiation());
      PresumedLoc presumed =
          getASTContext().getSourceManager().getPresumedLoc(poi);
      if (presumed.isValid()) {
        func->setAttr("ast_lambda_context_poi_file",
                      builder.getStringAttr(presumed.getFilename()));
        func->setAttr("ast_lambda_context_poi_line",
                      builder.getI32IntegerAttr(presumed.getLine()));
        func->setAttr("ast_lambda_context_poi_column",
                      builder.getI32IntegerAttr(presumed.getColumn()));
      }
    }
  }
  // TODO(cir): More logic of constructAttributeList is needed.
  cir::CallingConv callingConv;
  cir::SideEffect sideEffect;

  // TODO(cir): The current list should be initialized with the extra function
  // attributes, but we don't have those yet.  For now, the PAL is initialized
  // with nothing.
  assert(!cir::MissingFeatures::opFuncExtraAttrs());
  // Initialize PAL with existing attributes to merge attributes.
  mlir::NamedAttrList pal{};
  std::vector<mlir::NamedAttrList> argAttrs(info.arguments().size());
  mlir::NamedAttrList retAttrs{};
  constructAttributeList(func.getName(), info, globalDecl, pal, argAttrs,
                         retAttrs, callingConv, sideEffect,
                         /*attrOnCallSite=*/false, isThunk);

  for (mlir::NamedAttribute attr : pal)
    func->setAttr(attr.getName(), attr.getValue());

  llvm::for_each(llvm::enumerate(argAttrs), [func](auto idx_arg_pair) {
    mlir::function_interface_impl::setArgAttrs(func, idx_arg_pair.index(),
                                               idx_arg_pair.value());
  });
  // `constructAttributeList` replaces the complete per-argument attribute
  // dictionary, so source-type facts must be attached after ABI attributes.
  if (auto sourceTypes =
          func->getAttrOfType<mlir::ArrayAttr>("ast_param_source_types")) {
    const unsigned cirParams = func.getNumArguments();
    const auto *functionDecl =
        dyn_cast_or_null<FunctionDecl>(globalDecl.getDecl());
    const bool isStructor = isa_and_nonnull<CXXConstructorDecl>(functionDecl) ||
                            isa_and_nonnull<CXXDestructorDecl>(functionDecl);
    if (isStructor && !sourceTypes.empty()) {
      // `this` is always the first CIR structor parameter. The Itanium VTT
      // parameter, when present, is inserted immediately after it and has no
      // source-level parameter type.
      assert(cirParams && "CIR structor must have a this parameter");
      func.setArgAttr(0, "cir.ast_source_type", sourceTypes[0]);

      bool passesExplicitParams = true;
      if (const auto *constructor =
              dyn_cast<CXXConstructorDecl>(functionDecl)) {
        if (auto inherited = constructor->getInheritedConstructor())
          passesExplicitParams = getTypes().inheritingCtorHasParams(
              inherited, globalDecl.getCtorType());
      }

      // A base variant of an inheriting constructor can intentionally omit
      // every explicit source parameter when the inherited constructor
      // constructs a virtual base. Keep those types in the function-level
      // source metadata, but do not attach them to nonexistent CIR arguments.
      if (passesExplicitParams) {
        unsigned cirIndex = getCXXABI().needsVTTParameter(globalDecl) ? 2 : 1;
        for (unsigned sourceIndex = 1; sourceIndex < sourceTypes.size();
             ++sourceIndex)
          func.setArgAttr(cirIndex++, "cir.ast_source_type",
                          sourceTypes[sourceIndex]);
      }
    } else if (cirParams >= sourceTypes.size()) {
      const unsigned offset = cirParams - sourceTypes.size();
      for (unsigned index = 0; index < sourceTypes.size(); ++index)
        func.setArgAttr(offset + index, "cir.ast_source_type",
                        sourceTypes[index]);
    }
  }
  if (!retAttrs.empty())
    mlir::function_interface_impl::setResultAttrs(func, 0, retAttrs);
  if (auto sourceTypes =
          func->getAttrOfType<mlir::ArrayAttr>("ast_param_source_types")) {
    const unsigned cirParams = func.getNumArguments();
    if (cirParams >= sourceTypes.size()) {
      const auto *fd = dyn_cast_or_null<FunctionDecl>(globalDecl.getDecl());
      const bool isStructor = isa_and_nonnull<CXXConstructorDecl>(fd) ||
                              isa_and_nonnull<CXXDestructorDecl>(fd);
      const bool hasThis = isa_and_nonnull<CXXMethodDecl>(fd) &&
                           !cast<CXXMethodDecl>(fd)->isStatic();
      const unsigned sourceOffset = cirParams - sourceTypes.size();
      unsigned cirIndex =
          isStructor ? (getCXXABI().needsVTTParameter(globalDecl) ? 2 : 1)
                     : sourceOffset + (hasThis ? 1 : 0);
      if (fd) {
        for (unsigned index = 0; index < fd->getNumParams(); ++index) {
          if (cirIndex >= cirParams)
            break;
          if (auto facts =
                  memberPointerFacts(fd->getParamDecl(index)->getType()))
            func.setArgAttr(cirIndex, "cir.ast_member_pointer", facts);
          ++cirIndex;
        }
      }
    }
  }
  if (functionDecl) {
    if (auto facts =
            memberPointerFacts(functionDecl->getReturnType(), functionDecl)) {
      mlir::NamedAttrList resultAttrs = retAttrs;
      resultAttrs.set("cir.ast_member_pointer", facts);
      mlir::function_interface_impl::setResultAttrs(func, 0, resultAttrs);
    }
  }
  // TODO(cir): Check X86_VectorCall incompatibility wiht WinARM64EC

  // TODO(cir): Set the calling convention computed by constructAttributeList
  // on the function. FuncOp supports calling_conv, but target-specific
  // CodeGen is needed to set it correctly (e.g., AMDGPU kernel functions
  // should be marked with AMDGPUKernel).
  assert(!cir::MissingFeatures::opFuncCallingConv());
}

void CIRGenModule::setFunctionAttributes(GlobalDecl globalDecl,
                                         cir::FuncOp func,
                                         bool isIncompleteFunction,
                                         bool isThunk) {
  // NOTE(cir): Original CodeGen checks if this is an intrinsic. In CIR we
  // represent them in dedicated ops. The correct attributes are ensured during
  // translation to LLVM. Thus, we don't need to check for them here.

  const auto *funcDecl = cast<FunctionDecl>(globalDecl.getDecl());

  if (!isIncompleteFunction)
    setCIRFunctionAttributes(globalDecl,
                             getTypes().arrangeGlobalDeclaration(globalDecl),
                             func, isThunk);

  if (!isIncompleteFunction && func.isDeclaration())
    getTargetCIRGenInfo().setTargetAttributes(funcDecl, func, *this);

  // Mirrors setLinkageForGV in CodeGenModule::SetFunctionAttributes.
  setLinkageForFunction(*this, func, funcDecl);

  // If we plan on emitting this inline builtin, we can't treat it as a builtin.
  if (funcDecl->isInlineBuiltinDeclaration()) {
    const FunctionDecl *fdBody;
    bool hasBody = funcDecl->hasBody(fdBody);
    (void)hasBody;
    assert(hasBody && "Inline builtin declarations should always have an "
                      "available body!");
    assert(!cir::MissingFeatures::attributeNoBuiltin());
  }

  if (funcDecl->isReplaceableGlobalAllocationFunction()) {
    // A replaceable global allocation function does not act like a builtin by
    // default, only if it is invoked by a new-expression or delete-expression.
    func->setAttr(cir::CIRDialect::getNoBuiltinAttrName(),
                  mlir::UnitAttr::get(&getMLIRContext()));
  }
}

/// Determines whether the language options require us to model
/// unwind exceptions.  We treat -fexceptions as mandating this
/// except under the fragile ObjC ABI with only ObjC exceptions
/// enabled.  This means, for example, that C with -fexceptions
/// enables this.
static bool hasUnwindExceptions(const LangOptions &langOpts) {
  // If exceptions are completely disabled, obviously this is false.
  if (!langOpts.Exceptions)
    return false;
  // If C++ exceptions are enabled, this is true.
  if (langOpts.CXXExceptions)
    return true;
  // If ObjC exceptions are enabled, this depends on the ABI.
  if (langOpts.ObjCExceptions)
    return langOpts.ObjCRuntime.hasUnwindExceptions();
  return true;
}

void CIRGenModule::setCIRFunctionAttributesForDefinition(
    const clang::FunctionDecl *decl, cir::FuncOp f) {
  assert(!cir::MissingFeatures::opFuncUnwindTablesAttr());
  assert(!cir::MissingFeatures::stackProtector());

  if (!hasUnwindExceptions(langOpts))
    f->setAttr(cir::CIRDialect::getNoThrowAttrName(),
               mlir::UnitAttr::get(&getMLIRContext()));

  std::optional<cir::InlineKind> existingInlineKind = f.getInlineKind();
  bool isNoInline =
      existingInlineKind && *existingInlineKind == cir::InlineKind::NoInline;
  bool isAlwaysInline = existingInlineKind &&
                        *existingInlineKind == cir::InlineKind::AlwaysInline;
  if (!decl) {
    assert(!cir::MissingFeatures::hlsl());

    if (!isAlwaysInline &&
        codeGenOpts.getInlining() == CodeGenOptions::OnlyAlwaysInlining) {
      // If inlining is disabled and we don't have a declaration to control
      // inlining, mark the function as 'noinline' unless it is explicitly
      // marked as 'alwaysinline'.
      f.setInlineKind(cir::InlineKind::NoInline);
    }

    return;
  }

  assert(!cir::MissingFeatures::opFuncArmStreamingAttr());
  assert(!cir::MissingFeatures::opFuncArmNewAttr());
  assert(!cir::MissingFeatures::opFuncOptNoneAttr());
  assert(!cir::MissingFeatures::opFuncMinSizeAttr());
  assert(!cir::MissingFeatures::opFuncNakedAttr());
  assert(!cir::MissingFeatures::opFuncNoDuplicateAttr());
  assert(!cir::MissingFeatures::hlsl());

  // Handle inline attributes
  if (decl->hasAttr<NoInlineAttr>() && !isAlwaysInline) {
    // Add noinline if the function isn't always_inline.
    f.setInlineKind(cir::InlineKind::NoInline);
  } else if (decl->hasAttr<AlwaysInlineAttr>() && !isNoInline) {
    // Don't override AlwaysInline with NoInline, or vice versa, since we can't
    // specify both in IR.
    f.setInlineKind(cir::InlineKind::AlwaysInline);
  } else if (codeGenOpts.getInlining() == CodeGenOptions::OnlyAlwaysInlining) {
    // If inlining is disabled, force everything that isn't always_inline
    // to carry an explicit noinline attribute.
    if (!isAlwaysInline)
      f.setInlineKind(cir::InlineKind::NoInline);
  } else {
    // Otherwise, propagate the inline hint attribute and potentially use its
    // absence to mark things as noinline.
    // Search function and template pattern redeclarations for inline.
    if (auto *fd = dyn_cast<FunctionDecl>(decl)) {
      // TODO: Share this checkForInline implementation with classic codegen.
      // This logic is likely to change over time, so sharing would help ensure
      // consistency.
      auto checkForInline = [](const FunctionDecl *decl) {
        auto checkRedeclForInline = [](const FunctionDecl *redecl) {
          return redecl->isInlineSpecified();
        };
        if (any_of(decl->redecls(), checkRedeclForInline))
          return true;
        const FunctionDecl *pattern = decl->getTemplateInstantiationPattern();
        if (!pattern)
          return false;
        return any_of(pattern->redecls(), checkRedeclForInline);
      };
      if (checkForInline(fd)) {
        f.setInlineKind(cir::InlineKind::InlineHint);
      } else if (codeGenOpts.getInlining() ==
                     CodeGenOptions::OnlyHintInlining &&
                 !fd->isInlined() && !isAlwaysInline) {
        f.setInlineKind(cir::InlineKind::NoInline);
      }
    }
  }

  assert(!cir::MissingFeatures::opFuncColdHotAttr());
}

cir::FuncOp CIRGenModule::getOrCreateCIRFunction(
    StringRef mangledName, mlir::Type funcType, GlobalDecl gd, bool forVTable,
    bool dontDefer, bool isThunk, ForDefinition_t isForDefinition,
    mlir::NamedAttrList extraAttrs) {
  const Decl *d = gd.getDecl();
  if (selectedDeclRootMode && d && !isSelectedDeclRoot(gd))
    addSelectedDeclDependency(gd);

  if (const auto *fd = cast_or_null<FunctionDecl>(d)) {
    // For the device, mark the function as one that should be emitted.
    if (getLangOpts().OpenMPIsTargetDevice && openMPRuntime &&
        !getOpenMPRuntime().markAsGlobalTarget(gd) && fd->isDefined() &&
        !dontDefer && !isForDefinition) {
      if (const FunctionDecl *fdDef = fd->getDefinition()) {
        GlobalDecl gdDef;
        if (const auto *cd = dyn_cast<CXXConstructorDecl>(fdDef))
          gdDef = GlobalDecl(cd, gd.getCtorType());
        else if (const auto *dd = dyn_cast<CXXDestructorDecl>(fdDef))
          gdDef = GlobalDecl(dd, gd.getDtorType());
        else
          gdDef = GlobalDecl(fdDef);
        emitGlobal(gdDef);
      }
    }

    // Any attempts to use a MultiVersion function should result in retrieving
    // the iFunc instead. Name mangling will handle the rest of the changes.
    if (fd->isMultiVersion())
      errorNYI(fd->getSourceRange(), "getOrCreateCIRFunction: multi-version");
  }

  // Lookup the entry, lazily creating it if necessary.
  mlir::Operation *entry = getGlobalValue(mangledName);
  if (entry) {
    assert(mlir::isa<cir::FuncOp>(entry));

    assert(!cir::MissingFeatures::weakRefReference());

    // Handle dropped DLL attributes.
    if (d && !d->hasAttr<DLLImportAttr>() && !d->hasAttr<DLLExportAttr>()) {
      assert(!cir::MissingFeatures::setDLLStorageClass());
      setDSOLocal(entry);
    }

    // A selected closure can contain two exact Clang specializations whose
    // source types are distinct but whose ABI encodings, CIR function types,
    // and linkage names coincide (for example GNU vector_size and
    // ext_vector_type). The linker has one body, while analysis must retain
    // both declaration identities. Merge only that exact ABI-equivalent case;
    // ordinary conflicting definitions remain diagnosed.
    auto fn = cast<cir::FuncOp>(entry);
    if (isForDefinition && fn && !fn.isDeclaration()) {
      GlobalDecl otherGd;
      const bool hasDistinctDefinition =
          lookupRepresentativeDecl(mangledName, otherGd) &&
          gd.getCanonicalDecl().getDecl() !=
              otherGd.getCanonicalDecl().getDecl();
      bool mergedExactIdentities = false;
      if (selectedDeclRootMode && hasDistinctDefinition &&
          fn.getFunctionType() == funcType) {
        llvm::SmallString<256> incomingUSR;
        llvm::SmallString<256> existingUSR;
        if (!clang::index::generateUSRForDecl(
                gd.getCanonicalDecl().getDecl(), incomingUSR) &&
            !clang::index::generateUSRForDecl(
                otherGd.getCanonicalDecl().getDecl(), existingUSR) &&
            incomingUSR != existingUSR) {
          llvm::SmallVector<mlir::Attribute, 4> alternatives;
          auto appendUnique = [&](mlir::StringAttr usr) {
            if (!llvm::is_contained(alternatives,
                                    static_cast<mlir::Attribute>(usr)))
              alternatives.push_back(usr);
          };
          if (auto prior = fn->getAttrOfType<mlir::ArrayAttr>(
                  "ast_decl_usr_alternatives"))
            for (mlir::Attribute value : prior)
              appendUnique(mlir::cast<mlir::StringAttr>(value));
          appendUnique(builder.getStringAttr(existingUSR));
          appendUnique(builder.getStringAttr(incomingUSR));
          fn->setAttr("ast_decl_usr_alternatives",
                      builder.getArrayAttr(alternatives));
          mergedExactIdentities = true;
        }
      }
      // Check that GD is not yet in DiagnosedConflictingDefinitions to issue
      // an ordinary source collision only once.
      if (hasDistinctDefinition && !mergedExactIdentities &&
          diagnosedConflictingDefinitions.insert(gd).second) {
        getDiags().Report(d->getLocation(), diag::err_duplicate_mangled_name)
            << mangledName;
        getDiags().Report(otherGd.getDecl()->getLocation(),
                          diag::note_previous_definition);
      }
    }

    if (fn.getFunctionType() == funcType) {
      if (d && (isa<CXXConstructorDecl>(d) || isa<CXXDestructorDecl>(d)))
        setFunctionAttributes(gd, fn, /*isIncompleteFunction=*/false, isThunk);
      return fn;
    }

    if (!isForDefinition) {
      return fn;
    }

    // TODO(cir): classic codegen checks here if this is a llvm::GlobalAlias.
    // How will we support this?
  }

  auto *funcDecl = llvm::cast_or_null<FunctionDecl>(gd.getDecl());
  bool invalidLoc = !funcDecl ||
                    funcDecl->getSourceRange().getBegin().isInvalid() ||
                    funcDecl->getSourceRange().getEnd().isInvalid();
  cir::FuncOp funcOp = createCIRFunction(
      invalidLoc ? theModule->getLoc() : getLoc(funcDecl->getSourceRange()),
      mangledName, mlir::cast<cir::FuncType>(funcType), funcDecl);

  if (funcDecl && funcDecl->hasAttr<AnnotateAttr>())
    deferredAnnotations[mangledName] = funcDecl;

  // If we already created a function with the same mangled name (but different
  // type) before, take its name and add it to the list of functions to be
  // replaced with F at the end of CodeGen.
  //
  // This happens if there is a prototype for a function (e.g. "int f()") and
  // then a definition of a different type (e.g. "int f(int x)").
  if (entry) {

    // Fetch a generic symbol-defining operation and its uses.
    auto symbolOp = mlir::cast<mlir::SymbolOpInterface>(entry);

    // This might be an implementation of a function without a prototype, in
    // which case, try to do special replacement of calls which match the new
    // prototype. The really key thing here is that we also potentially drop
    // arguments from the call site so as to make a direct call, which makes the
    // inliner happier and suppresses a number of optimizer warnings (!) about
    // dropping arguments.
    if (symbolOp.getSymbolUses(symbolOp->getParentOp()))
      replaceUsesOfNonProtoTypeWithRealFunction(entry, funcOp);

    // Obliterate no-proto declaration.
    eraseGlobalSymbol(entry);
    entry->erase();
  }

  if (d)
    setFunctionAttributes(gd, funcOp, /*isIncompleteFunction=*/false, isThunk);
  if (!extraAttrs.empty()) {
    extraAttrs.append(funcOp->getAttrs());
    funcOp->setAttrs(extraAttrs);
  }

  // 'dontDefer' actually means don't move this to the deferredDeclsToEmit list.
  if (dontDefer) {
    // TODO(cir): This assertion will need an additional condition when we
    // support incomplete functions.
    assert(funcOp.getFunctionType() == funcType);
    return funcOp;
  }

  // All MSVC dtors other than the base dtor are linkonce_odr and delegate to
  // each other bottoming out wiht the base dtor. Therefore we emit non-base
  // dtors on usage, even if there is no dtor definition in the TU.
  if (isa_and_nonnull<CXXDestructorDecl>(d) &&
      getCXXABI().useThunkForDtorVariant(cast<CXXDestructorDecl>(d),
                                         gd.getDtorType()))
    errorNYI(d->getSourceRange(), "getOrCreateCIRFunction: dtor");

  // This is the first use or definition of a mangled name. If there is a
  // deferred decl with this name, remember that we need to emit it at the end
  // of the file.
  auto ddi = deferredDecls.find(mangledName);
  if (ddi != deferredDecls.end()) {
    // Move the potentially referenced deferred decl to the
    // DeferredDeclsToEmit list, and remove it from DeferredDecls (since we
    // don't need it anymore).
    addDeferredDeclToEmit(ddi->second);
    deferredDecls.erase(ddi);

    // Otherwise, there are cases we have to worry about where we're using a
    // declaration for which we must emit a definition but where we might not
    // find a top-level definition.
    //   - member functions defined inline in their classes
    //   - friend functions defined inline in some class
    //   - special member functions with implicit definitions
    // If we ever change our AST traversal to walk into class methods, this
    // will be unnecessary.
    //
    // We also don't emit a definition for a function if it's going to be an
    // entry in a vtable, unless it's already marked as used.
  } else if (getLangOpts().CPlusPlus && d) {
    // Look for a declaration that's lexically in a record.
    for (const auto *fd = cast<FunctionDecl>(d)->getMostRecentDecl(); fd;
         fd = fd->getPreviousDecl()) {
      if (isa<CXXRecordDecl>(fd->getLexicalDeclContext())) {
        if (fd->doesThisDeclarationHaveABody()) {
          addDeferredDeclToEmit(gd.getWithDecl(fd));
          break;
        }
      }
    }
  }

  return funcOp;
}

cir::FuncOp
CIRGenModule::createCIRFunction(mlir::Location loc, StringRef name,
                                cir::FuncType funcType,
                                const clang::FunctionDecl *funcDecl) {
  cir::FuncOp func;
  {
    mlir::OpBuilder::InsertionGuard guard(builder);

    // Some global emissions are triggered while emitting a function, e.g.
    // void s() { x.method() }
    //
    // Be sure to insert a new function before a current one.
    CIRGenFunction *cgf = this->curCGF;
    if (cgf)
      builder.setInsertionPoint(cgf->curFn);
    else
      builder.setInsertionPointToEnd(theModule.getBody());

    func = cir::FuncOp::create(builder, loc, name, funcType);

    symbolLookupCache[func.getSymNameAttr()] = func;

    assert(!cir::MissingFeatures::opFuncAstDeclAttr());

    if (funcDecl && !funcDecl->hasPrototype())
      func.setNoProto(true);

    assert(func.isDeclaration() && "expected empty body");

    // A declaration gets private visibility by default, but external linkage
    // as the default linkage.
    func.setLinkageAttr(cir::GlobalLinkageKindAttr::get(
        &getMLIRContext(), cir::GlobalLinkageKind::ExternalLinkage));
    mlir::SymbolTable::setSymbolVisibility(
        func, mlir::SymbolTable::Visibility::Private);

    assert(!cir::MissingFeatures::opFuncExtraAttrs());

    // Mark C++ special member functions (Constructor, Destructor etc.)
    setCXXSpecialMemberAttr(func, funcDecl);

    if (this->getLangOpts().OpenACC) {
      // We only have to handle this attribute, since OpenACCAnnotAttrs are
      // handled via the end-of-TU work.
      for (const auto *attr :
           funcDecl->specific_attrs<OpenACCRoutineDeclAttr>())
        emitOpenACCRoutineDecl(funcDecl, func, attr->getLocation(),
                               attr->Clauses);
    }
  }
  return func;
}

cir::FuncOp
CIRGenModule::createCIRBuiltinFunction(mlir::Location loc, StringRef name,
                                       cir::FuncType ty,
                                       const clang::FunctionDecl *fd) {
  cir::FuncOp fnOp = createCIRFunction(loc, name, ty, fd);
  fnOp.setBuiltin(true);
  return fnOp;
}

static cir::CtorKind getCtorKindFromDecl(const CXXConstructorDecl *ctor) {
  if (ctor->isDefaultConstructor())
    return cir::CtorKind::Default;
  if (ctor->isCopyConstructor())
    return cir::CtorKind::Copy;
  if (ctor->isMoveConstructor())
    return cir::CtorKind::Move;
  return cir::CtorKind::Custom;
}

static cir::AssignKind getAssignKindFromDecl(const CXXMethodDecl *method) {
  if (method->isCopyAssignmentOperator())
    return cir::AssignKind::Copy;
  if (method->isMoveAssignmentOperator())
    return cir::AssignKind::Move;
  llvm_unreachable("not a copy or move assignment operator");
}

void CIRGenModule::setCXXSpecialMemberAttr(
    cir::FuncOp funcOp, const clang::FunctionDecl *funcDecl) {
  if (!funcDecl)
    return;

  if (const auto *dtor = dyn_cast<CXXDestructorDecl>(funcDecl)) {
    auto cxxDtor = cir::CXXDtorAttr::get(
        convertType(getASTContext().getCanonicalTagType(dtor->getParent())),
        dtor->isTrivial());
    funcOp.setCxxSpecialMemberAttr(cxxDtor);
    return;
  }

  if (const auto *ctor = dyn_cast<CXXConstructorDecl>(funcDecl)) {
    cir::CtorKind kind = getCtorKindFromDecl(ctor);
    auto cxxCtor = cir::CXXCtorAttr::get(
        convertType(getASTContext().getCanonicalTagType(ctor->getParent())),
        kind, ctor->isTrivial());
    funcOp.setCxxSpecialMemberAttr(cxxCtor);
    return;
  }

  const auto *method = dyn_cast<CXXMethodDecl>(funcDecl);
  if (method && (method->isCopyAssignmentOperator() ||
                 method->isMoveAssignmentOperator())) {
    cir::AssignKind assignKind = getAssignKindFromDecl(method);
    auto cxxAssign = cir::CXXAssignAttr::get(
        convertType(getASTContext().getCanonicalTagType(method->getParent())),
        assignKind, method->isTrivial());
    funcOp.setCxxSpecialMemberAttr(cxxAssign);
    return;
  }
}

static void setWindowsItaniumDLLImport(CIRGenModule &cgm, bool isLocal,
                                       cir::FuncOp funcOp, StringRef name) {
  // In Windows Itanium environments, try to mark runtime functions
  // dllimport. For Mingw and MSVC, don't. We don't really know if the user
  // will link their standard library statically or dynamically. Marking
  // functions imported when they are not imported can cause linker errors
  // and warnings.
  if (!isLocal && cgm.getTarget().getTriple().isWindowsItaniumEnvironment() &&
      !cgm.getCodeGenOpts().LTOVisibilityPublicStd) {
    assert(!cir::MissingFeatures::getRuntimeFunctionDecl());
    assert(!cir::MissingFeatures::setDLLStorageClass());
    assert(!cir::MissingFeatures::opGlobalDLLImportExport());
  }
}

cir::FuncOp CIRGenModule::createRuntimeFunction(cir::FuncType ty,
                                                StringRef name,
                                                mlir::NamedAttrList extraAttrs,
                                                bool isLocal,
                                                bool assumeConvergent) {
  if (assumeConvergent)
    errorNYI("createRuntimeFunction: assumeConvergent");

  cir::FuncOp entry = getOrCreateCIRFunction(name, ty, GlobalDecl(),
                                             /*forVtable=*/false, extraAttrs);

  if (entry) {
    // TODO(cir): set the attributes of the function.
    assert(!cir::MissingFeatures::setLLVMFunctionFEnvAttributes());
    assert(!cir::MissingFeatures::opFuncCallingConv());
    setWindowsItaniumDLLImport(*this, isLocal, entry, name);
    entry.setDSOLocal(true);
  }

  return entry;
}

mlir::SymbolTable::Visibility
CIRGenModule::getMLIRVisibility(cir::GlobalOp op) {
  // MLIR doesn't accept public symbols declarations (only
  // definitions).
  if (op.isDeclaration())
    return mlir::SymbolTable::Visibility::Private;
  return getMLIRVisibilityFromCIRLinkage(op.getLinkage());
}

mlir::SymbolTable::Visibility
CIRGenModule::getMLIRVisibilityFromCIRLinkage(cir::GlobalLinkageKind glk) {
  switch (glk) {
  case cir::GlobalLinkageKind::InternalLinkage:
  case cir::GlobalLinkageKind::PrivateLinkage:
    return mlir::SymbolTable::Visibility::Private;
  case cir::GlobalLinkageKind::ExternalLinkage:
  case cir::GlobalLinkageKind::ExternalWeakLinkage:
  case cir::GlobalLinkageKind::LinkOnceODRLinkage:
  case cir::GlobalLinkageKind::AvailableExternallyLinkage:
  case cir::GlobalLinkageKind::CommonLinkage:
  case cir::GlobalLinkageKind::WeakAnyLinkage:
  case cir::GlobalLinkageKind::WeakODRLinkage:
    return mlir::SymbolTable::Visibility::Public;
  default: {
    llvm::errs() << "visibility not implemented for '"
                 << stringifyGlobalLinkageKind(glk) << "'\n";
    assert(0 && "not implemented");
  }
  }
  llvm_unreachable("linkage should be handled above!");
}

cir::VisibilityKind CIRGenModule::getGlobalVisibilityKindFromClangVisibility(
    clang::VisibilityAttr::VisibilityType visibility) {
  switch (visibility) {
  case clang::VisibilityAttr::VisibilityType::Default:
    return cir::VisibilityKind::Default;
  case clang::VisibilityAttr::VisibilityType::Hidden:
    return cir::VisibilityKind::Hidden;
  case clang::VisibilityAttr::VisibilityType::Protected:
    return cir::VisibilityKind::Protected;
  }
  llvm_unreachable("unexpected visibility value");
}

cir::VisibilityAttr
CIRGenModule::getGlobalVisibilityAttrFromDecl(const Decl *decl) {
  const clang::VisibilityAttr *va = decl->getAttr<clang::VisibilityAttr>();
  cir::VisibilityAttr cirVisibility =
      cir::VisibilityAttr::get(&getMLIRContext());
  if (va) {
    cirVisibility = cir::VisibilityAttr::get(
        &getMLIRContext(),
        getGlobalVisibilityKindFromClangVisibility(va->getVisibility()));
  }
  return cirVisibility;
}
void CIRGenModule::release() {
  emitDeferred();
  emitVTablesOpportunistically();
  applyReplacements();
  diagnoseUnemittedSelectedDeclRoots();
  diagnoseUnemittedSelectedDeclDependencies();
  refreshExactRecordOperationIdentities();

  theModule->setAttr(cir::CIRDialect::getModuleLevelAsmAttrName(),
                     builder.getArrayAttr(globalScopeAsm));

  emitGlobalAnnotations();

  if (!recordLayoutEntries.empty())
    theModule->setAttr(
        cir::CIRDialect::getRecordLayoutsAttrName(),
        mlir::DictionaryAttr::get(&getMLIRContext(), recordLayoutEntries));
  if (!recordDeclIdentityEntries.empty())
    theModule->setAttr("cir.record_decl_identities",
                       mlir::DictionaryAttr::get(&getMLIRContext(),
                                                 recordDeclIdentityEntries));
  if (!emptyRecordSchemaEntries.empty())
    theModule->setAttr(
        "cir.empty_record_schemas",
        mlir::DictionaryAttr::get(&getMLIRContext(), emptyRecordSchemaEntries));
  if (!objcProtocolEntries.empty())
    theModule->setAttr("cir.objc_protocols",
                       builder.getArrayAttr(objcProtocolEntries));
  if (!objcInterfaceEntries.empty())
    theModule->setAttr("cir.objc_interfaces",
                       builder.getArrayAttr(objcInterfaceEntries));
  if (!objcCategoryEntries.empty())
    theModule->setAttr("cir.objc_categories",
                       builder.getArrayAttr(objcCategoryEntries));

  if (getTriple().isAMDGPU() ||
      (getTriple().isSPIRV() && getTriple().getVendor() == llvm::Triple::AMD))
    emitAMDGPUMetadata();

  if (getLangOpts().HIP) {
    // Emit a unique ID so that host and device binaries from the same
    // compilation unit can be associated.
    std::string cuidName =
        ("__hip_cuid_" + getASTContext().getCUIDHash()).str();
    auto int8Ty = cir::IntType::get(&getMLIRContext(), 8, /*isSigned=*/false);
    auto loc = builder.getUnknownLoc();
    mlir::ptr::MemorySpaceAttrInterface addrSpace =
        cir::LangAddressSpaceAttr::get(&getMLIRContext(),
                                       getGlobalVarAddressSpace(nullptr));

    auto gv = createGlobalOp(loc, cuidName, int8Ty,
                             /*isConstant=*/false, addrSpace);
    gv.setLinkage(cir::GlobalLinkageKind::ExternalLinkage);
    // Initialize with zero
    auto zeroAttr = cir::IntAttr::get(int8Ty, 0);
    gv.setInitialValueAttr(zeroAttr);
    // External linkage requires public visibility
    mlir::SymbolTable::setSymbolVisibility(
        gv, mlir::SymbolTable::Visibility::Public);

    addCompilerUsedGlobal(gv);
  }

  if (astContext.getLangOpts().CUDA && cudaRuntime)
    getCUDARuntime().finalizeModule();

  emitLLVMUsed();

  // Classic codegen calls `checkAliases` here to validate any alias
  // definitions emitted during codegen.
  assert(!cir::MissingFeatures::checkAliases());

  // There's a lot of code that is not implemented yet.
  assert(!cir::MissingFeatures::cgmRelease());
}

void CIRGenModule::emitAliasDefinition(GlobalDecl gd) {
  const auto *d = cast<ValueDecl>(gd.getDecl());
  const AliasAttr *aa = d->getAttr<AliasAttr>();
  assert(aa && "Not an alias?");

  StringRef mangledName = getMangledName(gd);

  if (aa->getAliasee() == mangledName) {
    diags.Report(aa->getLocation(), diag::err_cyclic_alias) << 0;
    return;
  }

  // If there is a definition in the module, then it wins over the alias.
  // This is dubious, but allow it to be safe. Just ignore the alias.
  mlir::Operation *entry = getGlobalValue(mangledName);
  if (entry) {
    auto entryGV = mlir::dyn_cast<cir::CIRGlobalValueInterface>(entry);
    if (entryGV && entryGV.isDefinition())
      return;
  }

  // Classic codegen pushes the alias onto an `Aliases` list at this point so
  // that `checkAliases` can later validate the alias and recover on error.
  assert(!cir::MissingFeatures::checkAliases());

  mlir::Location loc = getLoc(d->getSourceRange());
  bool isFunction = isa<FunctionDecl>(d);

  // Get the linkage and the type of the alias.
  mlir::Type declTy;
  cir::GlobalLinkageKind linkage;
  if (isFunction) {
    declTy = getTypes().getFunctionType(gd);
    linkage = getFunctionLinkage(gd);
  } else {
    declTy = getTypes().convertTypeForMem(d->getType());
    const auto *vd = cast<VarDecl>(d);
    linkage = getCIRLinkageVarDefinition(vd);
  }

  // Aliases that target weak symbols must themselves be marked weak.
  if (d->hasAttr<WeakAttr>() || d->hasAttr<WeakRefAttr>() ||
      d->isWeakImported())
    linkage = cir::GlobalLinkageKind::WeakAnyLinkage;

  // Create the alias op. If there is an existing declaration with the same
  // name, erase it: any references to it via flat symbol reference will
  // automatically resolve to the new alias.
  if (entry) {
    eraseGlobalSymbol(entry);
    entry->erase();
  }

  // Aliases are always definitions, so the MLIR visibility should match the
  // linkage rather than defaulting to private.
  mlir::SymbolTable::Visibility visibility =
      getMLIRVisibilityFromCIRLinkage(linkage);

  // TODO(cir): Make GlobalAlias a separate op.
  cir::CIRGlobalValueInterface alias =
      isFunction ? mlir::cast<cir::CIRGlobalValueInterface>(
                       createCIRFunction(loc, mangledName,
                                         mlir::cast<cir::FuncType>(declTy),
                                         cast<FunctionDecl>(d))
                           .getOperation())
                 : mlir::cast<cir::CIRGlobalValueInterface>(
                       createGlobalOp(loc, mangledName, declTy).getOperation());
  alias.setAliasee(aa->getAliasee());
  alias.setLinkage(linkage);
  mlir::SymbolTable::setSymbolVisibility(alias, visibility);
  assert(!cir::MissingFeatures::opGlobalThreadLocal());
  setCommonAttributes(gd, alias);
  assert(!cir::MissingFeatures::generateDebugInfo());
}

void CIRGenModule::emitAliasForGlobal(StringRef mangledName,
                                      mlir::Operation *op, GlobalDecl aliasGD,
                                      cir::FuncOp aliasee,
                                      cir::GlobalLinkageKind linkage) {

  auto *aliasFD = dyn_cast<FunctionDecl>(aliasGD.getDecl());
  assert(aliasFD && "expected FunctionDecl");

  // The aliasee function type is different from the alias one, this difference
  // is specific to CIR because in LLVM the ptr types are already erased at this
  // point.
  const CIRGenFunctionInfo &fnInfo =
      getTypes().arrangeCXXStructorDeclaration(aliasGD);
  cir::FuncType fnType = getTypes().getFunctionType(fnInfo);

  cir::FuncOp alias =
      createCIRFunction(getLoc(aliasGD.getDecl()->getSourceRange()),
                        mangledName, fnType, aliasFD);
  alias.setAliasee(aliasee.getName());
  alias.setLinkage(linkage);
  // Declarations cannot have public MLIR visibility, just mark them private
  // but this really should have no meaning since CIR should not be using
  // this information to derive linkage information.
  mlir::SymbolTable::setSymbolVisibility(
      alias, mlir::SymbolTable::Visibility::Private);

  // Alias constructors and destructors are always unnamed_addr.
  assert(!cir::MissingFeatures::opGlobalUnnamedAddr());

  if (op) {
    // Any existing users of the existing function declaration will be
    // referencing the function by flat symbol reference (i.e. the name), so
    // those uses will automatically resolve to the alias now that we've
    // replaced the function declaration. We can safely erase the existing
    // function declaration.
    assert(cast<cir::FuncOp>(op).getFunctionType() == alias.getFunctionType() &&
           "declaration exists with different type");
    eraseGlobalSymbol(op);
    op->erase();
  } else {
    // Name already set by createCIRFunction
  }

  // Finally, set up the alias with its proper name and attributes.
  setCommonAttributes(aliasGD, alias);
}

mlir::Type CIRGenModule::convertType(QualType type) {
  return genTypes.convertType(type);
}

bool CIRGenModule::verifyModule() const {
  // Verify the module after we have finished constructing it, this will
  // check the structural properties of the IR and invoke any specific
  // verifiers we have on the CIR operations.
  return mlir::verify(theModule).succeeded();
}

mlir::Attribute CIRGenModule::getAddrOfRTTIDescriptor(mlir::Location loc,
                                                      QualType ty, bool forEh) {
  // Return a bogus pointer if RTTI is disabled, unless it's for EH.
  // FIXME: should we even be calling this method if RTTI is disabled
  // and it's not for EH?
  if (!shouldEmitRTTI(forEh))
    return builder.getConstNullPtrAttr(builder.getUInt8PtrTy());

  if (forEh && ty->isObjCObjectPointerType() &&
      langOpts.ObjCRuntime.isGNUFamily()) {
    errorNYI(loc, "getAddrOfRTTIDescriptor: Objc PtrType & Objc RT GUN");
    return {};
  }

  return getCXXABI().getAddrOfRTTIDescriptor(loc, ty);
}

// TODO(cir): this can be shared with LLVM codegen.
CharUnits CIRGenModule::computeNonVirtualBaseClassOffset(
    const CXXRecordDecl *derivedClass,
    llvm::iterator_range<CastExpr::path_const_iterator> path) {
  CharUnits offset = CharUnits::Zero();

  const ASTContext &astContext = getASTContext();
  const CXXRecordDecl *rd = derivedClass;

  for (const CXXBaseSpecifier *base : path) {
    assert(!base->isVirtual() && "Should not see virtual bases here!");

    // Get the layout.
    const ASTRecordLayout &layout = astContext.getASTRecordLayout(rd);

    const auto *baseDecl = base->getType()->castAsCXXRecordDecl();

    // Add the offset.
    offset += layout.getBaseClassOffset(baseDecl);

    rd = baseDecl;
  }

  return offset;
}

void CIRGenModule::recordNYI(SourceRange sourceRange,
                             std::optional<mlir::Location> mlirLocation,
                             llvm::StringRef shape,
                             llvm::StringRef message) const {
  if (!diagnosticCensus)
    return;

  const SourceManager &sourceManager = astContext.getSourceManager();
  const Decl *ownerDecl = currentDiagnosticDecl;
  if (!ownerDecl && curCGF)
    ownerDecl = curCGF->curGD.getDecl();

  std::optional<std::string> sourceNodeID;
  if (ownerDecl) {
    llvm::SmallString<256> usr;
    if (!clang::index::generateUSRForDecl(ownerDecl, usr))
      sourceNodeID = usr.str().str();
    else
      sourceNodeID =
          sourceLocationIdentity(astContext, ownerDecl->getLocation());
  }

  std::string owner = "translation_unit";
  if (sourceNodeID)
    owner = "clang_decl:" + *sourceNodeID;
  else if (curCGF && curCGF->curFn)
    if (auto symbol = mlir::dyn_cast<mlir::SymbolOpInterface>(curCGF->curFn))
      owner = ("cir_symbol:" + symbol.getName()).str();

  std::string unitID = "clang_tu:unavailable";
  FileID mainFile = sourceManager.getMainFileID();
  if (mainFile.isValid())
    if (auto identity = sourceLocationIdentity(
            astContext, sourceManager.getLocForStartOfFile(mainFile)))
      unitID = "clang_tu:" + *identity;

  struct Span {
    std::string file;
    unsigned startLine;
    unsigned startColumn;
    unsigned endLine;
    unsigned endColumn;
  };
  std::optional<Span> span;

  if (sourceRange.isValid()) {
    SourceLocation start = sourceManager.getSpellingLoc(sourceRange.getBegin());
    SourceLocation end = sourceManager.getSpellingLoc(sourceRange.getEnd());
    SourceLocation tokenEnd =
        Lexer::getLocForEndOfToken(end, 0, sourceManager, langOpts);
    if (tokenEnd.isValid())
      end = tokenEnd;
    PresumedLoc presumedStart = sourceManager.getPresumedLoc(start);
    PresumedLoc presumedEnd = sourceManager.getPresumedLoc(end);
    if (presumedStart.isValid() && presumedEnd.isValid() &&
        llvm::StringRef(presumedStart.getFilename()) ==
            presumedEnd.getFilename())
      span = Span{presumedStart.getFilename(), presumedStart.getLine(),
                  presumedStart.getColumn(), presumedEnd.getLine(),
                  presumedEnd.getColumn()};
  } else if (mlirLocation) {
    if (auto fileLocation =
            (*mlirLocation)->findInstanceOf<mlir::FileLineColLoc>())
      span = Span{fileLocation.getFilename().str(), fileLocation.getLine(),
                  fileLocation.getColumn(), fileLocation.getLine(),
                  fileLocation.getColumn()};
  }

  llvm::json::OStream json(*diagnosticCensus);
  json.object([&] {
    json.attribute(
        "id",
        ("LLVM_CIR_NYI:" + llvm::Twine(++diagnosticCensusSequence)).str());
    json.attribute("code", "LLVM_CIR_NYI");
    json.attribute("stage", "cir_codegen");
    json.attribute("owner", owner);
    json.attribute("unit_id", unitID);
    if (sourceNodeID)
      json.attribute("source_node_id", *sourceNodeID);
    else
      json.attribute("source_node_id", llvm::json::Value(nullptr));
    if (span) {
      json.attributeObject("span", [&] {
        json.attribute("file", span->file);
        json.attribute("start_line", span->startLine);
        json.attribute("start_column", span->startColumn);
        json.attribute("end_line", span->endLine);
        json.attribute("end_column", span->endColumn);
      });
    } else {
      json.attribute("span", llvm::json::Value(nullptr));
    }
    json.attribute("message", message);
    json.attribute("shape", shape);
    json.attributeArray("blocked_by", [] {});
  });
  *diagnosticCensus << '\n';
  diagnosticCensus->flush();
  if (diagnosticCensus->has_error())
    llvm::report_fatal_error("failed writing requested CIR diagnostic census");
}

DiagnosticBuilder CIRGenModule::errorNYI(SourceLocation loc,
                                         llvm::StringRef feature) {
  if (diagnosticCensus)
    recordNYI(SourceRange(loc), std::nullopt, feature,
              ("ClangIR code gen Not Yet Implemented: " + feature).str());
  unsigned diagID = diags.getCustomDiagID(
      DiagnosticsEngine::Error, "ClangIR code gen Not Yet Implemented: %0");
  return diags.Report(loc, diagID) << feature;
}

DiagnosticBuilder CIRGenModule::errorNYI(SourceRange loc,
                                         llvm::StringRef feature) {
  if (diagnosticCensus)
    recordNYI(loc, std::nullopt, feature,
              ("ClangIR code gen Not Yet Implemented: " + feature).str());
  unsigned diagID = diags.getCustomDiagID(
      DiagnosticsEngine::Error, "ClangIR code gen Not Yet Implemented: %0");
  return diags.Report(loc.getBegin(), diagID) << feature << loc;
}

void CIRGenModule::error(SourceLocation loc, StringRef error) {
  unsigned diagID = getDiags().getCustomDiagID(DiagnosticsEngine::Error, "%0");
  getDiags().Report(astContext.getFullLoc(loc), diagID) << error;
}

/// Print out an error that codegen doesn't support the specified stmt yet.
void CIRGenModule::errorUnsupported(const Stmt *s, llvm::StringRef type) {
  unsigned diagId = diags.getCustomDiagID(DiagnosticsEngine::Error,
                                          "cannot compile this %0 yet");
  diags.Report(astContext.getFullLoc(s->getBeginLoc()), diagId)
      << type << s->getSourceRange();
}

/// Print out an error that codegen doesn't support the specified decl yet.
void CIRGenModule::errorUnsupported(const Decl *d, llvm::StringRef type) {
  unsigned diagId = diags.getCustomDiagID(DiagnosticsEngine::Error,
                                          "cannot compile this %0 yet");
  diags.Report(astContext.getFullLoc(d->getLocation()), diagId) << type;
}

void CIRGenModule::mapBlockAddress(cir::BlockAddrInfoAttr blockInfo,
                                   cir::LabelOp label) {
  [[maybe_unused]] auto result =
      blockAddressInfoToLabel.try_emplace(blockInfo, label);
  assert(result.second &&
         "attempting to map a blockaddress info that is already mapped");
}

void CIRGenModule::mapConstantBlockAddress(cir::BlockAddrInfoAttr blockInfo) {
  constantBlockAddresses.insert(blockInfo);
}

void CIRGenModule::mapUnresolvedBlockAddress(cir::BlockAddressOp op) {
  [[maybe_unused]] auto result = unresolvedBlockAddressToLabel.insert(op);
  assert(result.second &&
         "attempting to map a blockaddress operation that is already mapped");
}

void CIRGenModule::mapResolvedBlockAddress(cir::BlockAddressOp op,
                                           cir::LabelOp label) {
  [[maybe_unused]] auto result = blockAddressToLabel.try_emplace(op, label);
  assert(result.second &&
         "attempting to map a blockaddress operation that is already mapped");
}

void CIRGenModule::updateResolvedBlockAddress(cir::BlockAddressOp op,
                                              cir::LabelOp newLabel) {
  auto *it = blockAddressToLabel.find(op);
  assert(it != blockAddressToLabel.end() &&
         "trying to update a blockaddress not previously mapped");
  assert(!it->second && "blockaddress already has a resolved label");

  it->second = newLabel;
}

cir::LabelOp
CIRGenModule::lookupBlockAddressInfo(cir::BlockAddrInfoAttr blockInfo) {
  return blockAddressInfoToLabel.lookup(blockInfo);
}

mlir::Operation *
CIRGenModule::getAddrOfGlobalTemporary(const MaterializeTemporaryExpr *mte,
                                       const Expr *init) {
  assert((mte->getStorageDuration() == SD_Static ||
          mte->getStorageDuration() == SD_Thread) &&
         "not a global temporary");
  const auto *varDecl = cast<VarDecl>(mte->getExtendingDecl());

  // Use the MaterializeTemporaryExpr's type if it has the same unqualified
  // base type as Init. This preserves cv-qualifiers (e.g. const from a
  // constexpr or const-ref binding) that skipRValueSubobjectAdjustments may
  // have dropped via NoOp casts, while correctly falling back to Init's type
  // when a real subobject adjustment changed the type (e.g. member access or
  // base-class cast in C++98), where E->getType() reflects the reference type,
  // not the actual storage type.
  QualType materializedType = init->getType();
  if (getASTContext().hasSameUnqualifiedType(mte->getType(), materializedType))
    materializedType = mte->getType();

  CharUnits align = getASTContext().getTypeAlignInChars(materializedType);
  mlir::Location loc = getLoc(mte->getSourceRange());

  // FIXME: If an externally-visible declaration extends multiple temporaries,
  // we need to give each temporary the same name in every translation unit (and
  // we also need to make the temporaries externally-visible).
  llvm::SmallString<256> name;
  llvm::raw_svector_ostream out(name);
  getCXXABI().getMangleContext().mangleReferenceTemporary(
      varDecl, mte->getManglingNumber(), out);

  auto insertResult = materializedGlobalTemporaryMap.insert({mte, nullptr});
  if (!insertResult.second) {
    mlir::Type type = getTypes().convertTypeForMem(materializedType);
    // We've seen this before: either we already created it or we're in the
    // process of doing so.
    if (!insertResult.first->second) {
      // We recursively re-entered this function, probably during emission of
      // the initializer. Create a placeholder.
      insertResult.first->second =
          createGlobalOp(loc, name, type, /*isConstant=*/false);
    }
    return insertResult.first->second;
  }

  APValue *value = nullptr;
  if (mte->getStorageDuration() == SD_Static && varDecl->evaluateValue()) {
    // If the initializer of the extending declaration is a constant
    // initializer, we should have a cached constant initializer for this
    // temporay. Note taht this m ight have a different value from the value
    // computed by evaluating the initializer if the surrounding constant
    // expression modifies the temporary.
    value = mte->getOrCreateValue(/*MayCreate=*/false);
  }

  // Try evaluating it now, it might have a constant initializer
  Expr::EvalResult evalResult;
  if (!value && init->EvaluateAsRValue(evalResult, getASTContext()) &&
      !evalResult.hasSideEffects())
    value = &evalResult.Val;

  assert(!cir::MissingFeatures::addressSpace());

  std::optional<ConstantEmitter> emitter;
  mlir::Attribute initialValue = nullptr;
  bool isConstant = false;
  mlir::Type type;

  if (value) {
    emitter.emplace(*this);
    initialValue = emitter->emitForInitializer(*value, materializedType);

    isConstant = materializedType.isConstantStorage(
        getASTContext(), /*ExcludeCtor=*/value, /*ExcludeDtor=*/false);

    type = mlir::cast<mlir::TypedAttr>(initialValue).getType();
  } else {
    // No initializer, the initialization will be provided when we initialize
    // the declaration which performed lifetime extension.
    type = getTypes().convertTypeForMem(materializedType);
  }

  // Create a global variable for this lifetime-extended temporary.
  cir::GlobalLinkageKind linkage = getCIRLinkageVarDefinition(varDecl);
  if (linkage == cir::GlobalLinkageKind::ExternalLinkage) {
    const VarDecl *initVD;
    if (varDecl->isStaticDataMember() && varDecl->getAnyInitializer(initVD) &&
        isa<CXXRecordDecl>(initVD->getLexicalDeclContext())) {
      // Temporaries defined inside a class get linkonce_odr linkage because the
      // calss can be defined in multiple translation units.
      errorNYI(mte->getSourceRange(), "static data member initialization");
    } else {
      // There is no need for this temporary to have external linkage if the
      // VarDecl has external linkage.
      linkage = cir::GlobalLinkageKind::InternalLinkage;
    }
  }
  cir::GlobalOp gv = createGlobalOp(loc, name, type, isConstant);
  gv.setInitialValueAttr(initialValue);
  gv.setLinkage(linkage);
  gv.setVisibility(getMLIRVisibilityFromCIRLinkage(linkage));
  if (const auto *memberPointer =
          materializedType.getCanonicalType()->getAs<MemberPointerType>();
      memberPointer && memberPointer->isMemberFunctionPointer()) {
    setMemberPointerTargetMetadata(gv.getOperation(), materializedType);
    const Expr *constantExpr = init->IgnoreParenImpCasts();
    const CXXMethodDecl *methodDecl = nullptr;
    if (const auto *address = dyn_cast<UnaryOperator>(constantExpr);
        address && address->getOpcode() == UO_AddrOf) {
      const Expr *referenced = address->getSubExpr()->IgnoreParenImpCasts();
      if (const auto *declRef = dyn_cast<DeclRefExpr>(referenced))
        methodDecl = dyn_cast<CXXMethodDecl>(declRef->getDecl());
      else if (const auto *member = dyn_cast<MemberExpr>(referenced))
        methodDecl = dyn_cast<CXXMethodDecl>(member->getMemberDecl());
    }
    mlir::NamedAttrList identity;
    if (methodDecl) {
      auto exact = buildCIRGenVirtualMethodIdentityAttrs(
          *this, getMLIRContext(), GlobalDecl(methodDecl));
      identity.set("kind", builder.getStringAttr("method"));
      identity.set("method_symbol", exact.method);
      identity.set("is_virtual", builder.getBoolAttr(methodDecl->isVirtual()));
      if (exact.methodUSR)
        identity.set("method_usr", exact.methodUSR);
      if (auto declaringClass = getRecordUSRAttr(methodDecl->getParent()))
        identity.set("method_declaring_class_usr", declaringClass);
      if (exact.rootMethodUSR)
        identity.set("virtual_root_method_usr", exact.rootMethodUSR);
      if (exact.declaringClassUSR)
        identity.set("virtual_root_declaring_class_usr",
                     exact.declaringClassUSR);
      if (exact.rootAlternatives)
        identity.set("virtual_root_alternatives", exact.rootAlternatives);
    } else if (init->isNullPointerConstant(getASTContext(),
                                           Expr::NPC_ValueDependentIsNull)) {
      identity.set("kind", builder.getStringAttr("null"));
      identity.set("null_function_value", builder.getStringAttr("0"));
      identity.set("null_adjustment_value", builder.getStringAttr("0"));
    }
    if (!identity.empty()) {
      gv->setAttr("ast_member_function_pointer_constant",
                  identity.getDictionary(&getMLIRContext()));
    }
  }

  if (emitter)
    emitter->finalize(gv);
  // Don't assign dllimport or dllexport to local linkage globals
  if (!gv.hasLocalLinkage()) {
    setGVProperties(gv, varDecl);
    assert(!cir::MissingFeatures::setDLLStorageClass());
  }

  gv.setAlignment(align.getAsAlign().value());
  if (supportsCOMDAT() && gv.isWeakForLinker() &&
      !gv.hasAvailableExternallyLinkage())
    gv.setComdat(true);
  if (varDecl->getTLSKind())
    setTLSMode(gv, *varDecl, /*isExtendingDecl=*/true);
  mlir::Operation *cv = gv;

  assert(!cir::MissingFeatures::addressSpace());

  // Update the map with the new temporary. If we created a placeholder above,
  // erase it as well, the name will have been the same, so our symbol
  // references would have been correct. We still do a 'replaceAllUsesWith' in
  // case some sort of expression formed a reference to the placeholder
  // temporary.
  mlir::Operation *&entry = materializedGlobalTemporaryMap[mte];
  if (entry) {
    entry->replaceAllUsesWith(cv);
    eraseGlobalSymbol(entry);
    entry->erase();
  }
  entry = cv;

  return cv;
}

cir::GlobalOp CIRGenModule::getAddrOfUnnamedGlobalConstantDecl(
    const UnnamedGlobalConstantDecl *gcd) {
  unsigned numEntries = unnamedGlobalConstantDeclMap.size();
  cir::GlobalOp *globalOpEntry = &unnamedGlobalConstantDeclMap[gcd];

  if (*globalOpEntry)
    return *globalOpEntry;

  ConstantEmitter emitter(*this);

  const APValue &value = gcd->getValue();
  assert(!value.isAbsent());
  assert(!cir::MissingFeatures::addressSpace() &&
         "emitForInitializer should take gcd->getType().getAddressSpace()");
  mlir::Attribute init = emitter.emitForInitializer(value, gcd->getType());
  auto typedInit = dyn_cast<mlir::TypedAttr>(init);

  if (!typedInit)
    errorNYI(gcd->getSourceRange(),
             "getAddrOfUnnamedGlobalConstantDecl: non-typed initializer");

  assert(!cir::MissingFeatures::addressSpace());

  // Classic codegen always creates these with .constant, then counts on the
  // auto-addition of '.#'. CIR global doesn't have this, so we'll just auto-add
  // one if this isn't the first.  We could probably choose a better name than
  // .constant to be unique for this type of decl, but this is consistent with
  // classic codegen.
  std::string name = numEntries == 0
                         ? ".constant"
                         : (Twine(".constant.") + Twine(numEntries)).str();
  auto globalOp = createGlobalOp(builder.getUnknownLoc(), name,
                                 typedInit.getType(), /*is_constant=*/true);
  globalOp.setLinkage(cir::GlobalLinkageKind::PrivateLinkage);

  CharUnits alignment = getASTContext().getTypeAlignInChars(gcd->getType());
  globalOp.setAlignment(alignment.getAsAlign().value());
  CIRGenModule::setInitializer(globalOp, init);

  emitter.finalize(globalOp);
  *globalOpEntry = globalOp;
  return globalOp;
}

cir::GlobalOp
CIRGenModule::getAddrOfTemplateParamObject(const TemplateParamObjectDecl *tpo) {
  StringRef name = getMangledName(tpo);
  CharUnits alignment = getNaturalTypeAlignment(tpo->getType());

  if (auto globalOp =
          mlir::dyn_cast_or_null<cir::GlobalOp>(getGlobalValue(name)))
    return globalOp;

  ConstantEmitter emitter(*this);
  assert(!cir::MissingFeatures::addressSpace() &&
         "emitForInitializer should take tpo->getType().getAddressSpace()");
  mlir::Attribute init =
      emitter.emitForInitializer(tpo->getValue(), tpo->getType());

  if (!init) {
    errorUnsupported(tpo, "template parameter object");
    return {};
  }

  mlir::TypedAttr typedInit = cast<mlir::TypedAttr>(init);

  cir::GlobalLinkageKind linkage =
      isExternallyVisible(tpo->getLinkageAndVisibility().getLinkage())
          ? cir::GlobalLinkageKind::LinkOnceODRLinkage
          : cir::GlobalLinkageKind::InternalLinkage;

  assert(!cir::MissingFeatures::addressSpace());
  auto globalOp = createGlobalOp(builder.getUnknownLoc(), name,
                                 typedInit.getType(), /*is_constant=*/true);
  globalOp.setLinkage(linkage);
  globalOp.setAlignment(alignment.getAsAlign().value());
  globalOp.setComdat(supportsCOMDAT() &&
                     linkage == cir::GlobalLinkageKind::LinkOnceODRLinkage);

  CIRGenModule::setInitializer(globalOp, init);
  emitter.finalize(globalOp);

  insertGlobalSymbol(globalOp);

  return globalOp;
}

//===----------------------------------------------------------------------===//
// Annotations
//===----------------------------------------------------------------------===//

mlir::ArrayAttr
CIRGenModule::getOrCreateAnnotationArgs(const AnnotateAttr *attr) {
  ArrayRef<Expr *> exprs = {attr->args_begin(), attr->args_size()};
  // Return a null attr for no-args annotations so OptionalParameter omits
  // the args portion entirely from the printed IR.
  if (exprs.empty())
    return {};

  llvm::FoldingSetNodeID id;
  for (Expr *e : exprs)
    id.Add(cast<clang::ConstantExpr>(e)->getAPValueResult());

  mlir::ArrayAttr &lookup = annotationArgs[id.ComputeHash()];
  if (lookup)
    return lookup;

  llvm::SmallVector<mlir::Attribute> args;
  args.reserve(exprs.size());
  for (Expr *e : exprs) {
    if (auto *strE = dyn_cast<clang::StringLiteral>(e->IgnoreParenCasts())) {
      args.push_back(builder.getStringAttr(strE->getString()));
    } else if (auto *intE =
                   dyn_cast<clang::IntegerLiteral>(e->IgnoreParenCasts())) {
      auto intTy = builder.getIntegerType(intE->getValue().getBitWidth());
      args.push_back(builder.getIntegerAttr(intTy, intE->getValue()));
    } else {
      errorNYI(e->getExprLoc(), "annotation argument expression");
    }
  }

  return lookup = builder.getArrayAttr(args);
}

cir::AnnotationAttr CIRGenModule::emitAnnotateAttr(const AnnotateAttr *aa) {
  mlir::StringAttr annoGV = builder.getStringAttr(aa->getAnnotation());
  mlir::ArrayAttr args = getOrCreateAnnotationArgs(aa);
  return cir::AnnotationAttr::get(&getMLIRContext(), annoGV, args);
}

void CIRGenModule::addGlobalAnnotations(const ValueDecl *d,
                                        mlir::Operation *gv) {
  assert(d->hasAttr<AnnotateAttr>() && "no annotate attribute");
  assert((isa<cir::GlobalOp>(gv) || isa<cir::FuncOp>(gv)) &&
         "annotation only on globals");
  llvm::SmallVector<mlir::Attribute> annotations;
  for (const auto *i : d->specific_attrs<AnnotateAttr>())
    annotations.push_back(emitAnnotateAttr(i));
  if (auto global = dyn_cast<cir::GlobalOp>(gv))
    global.setAnnotationsAttr(builder.getArrayAttr(annotations));
  else if (auto func = dyn_cast<cir::FuncOp>(gv))
    func.setAnnotationsAttr(builder.getArrayAttr(annotations));
}

void CIRGenModule::emitGlobalAnnotations() {
  for (const auto &[mangledName, vd] : deferredAnnotations) {
    mlir::Operation *gv = getGlobalValue(mangledName);
    if (gv)
      addGlobalAnnotations(vd, gv);
  }
  deferredAnnotations.clear();
}
