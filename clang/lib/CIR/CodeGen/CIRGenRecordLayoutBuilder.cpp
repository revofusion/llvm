//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This contains code to compute the layout of a record.
//
//===----------------------------------------------------------------------===//

#include "CIRGenBuilder.h"
#include "CIRGenCXXABI.h"
#include "CIRGenModule.h"
#include "CIRGenTypes.h"
#include "TargetInfo.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/DeclTemplate.h"
#include "clang/AST/DynamicRecursiveASTVisitor.h"
#include "clang/AST/ExprCXX.h"
#include "clang/AST/GlobalDecl.h"
#include "clang/AST/Mangle.h"
#include "clang/AST/ODRHash.h"
#include "clang/AST/RecordLayout.h"
#include "clang/AST/Type.h"
#include "clang/Basic/SourceManager.h"
#include "clang/CIR/Dialect/IR/CIRAttrs.h"
#include "clang/CIR/Dialect/IR/CIRDataLayout.h"
#include "clang/CIR/MissingFeatures.h"
#include "clang/UnifiedSymbolResolution/USRGeneration.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SHA256.h"
#include "llvm/Support/raw_ostream.h"

#include <iomanip>
#include <memory>
#include <sstream>
#include <vector>

using namespace llvm;
using namespace clang;
using namespace clang::CIRGen;

namespace {
/// The CIRRecordLowering is responsible for lowering an ASTRecordLayout to an
/// mlir::Type. Some of the lowering is straightforward, some is not.
// TODO: Detail some of the complexities and weirdnesses?
// (See CGRecordLayoutBuilder.cpp)
struct CIRRecordLowering final {

  // MemberInfo is a helper structure that contains information about a record
  // member. In addition to the standard member types, there exists a sentinel
  // member type that ensures correct rounding.
  struct MemberInfo final {
    CharUnits offset;
    enum class InfoKind { VFPtr, Field, Base, VBase } kind;
    mlir::Type data;
    union {
      const FieldDecl *fieldDecl;
      const CXXRecordDecl *cxxRecordDecl;
    };
    MemberInfo(CharUnits offset, InfoKind kind, mlir::Type data,
               const FieldDecl *fieldDecl = nullptr)
        : offset{offset}, kind{kind}, data{data}, fieldDecl{fieldDecl} {}
    MemberInfo(CharUnits offset, InfoKind kind, mlir::Type data,
               const CXXRecordDecl *rd)
        : offset{offset}, kind{kind}, data{data}, cxxRecordDecl{rd} {}
    // MemberInfos are sorted so we define a < operator.
    bool operator<(const MemberInfo &other) const {
      return offset < other.offset;
    }
  };
  // The constructor.
  CIRRecordLowering(CIRGenTypes &cirGenTypes, const RecordDecl *recordDecl,
                    bool packed);

  /// Constructs a MemberInfo instance from an offset and mlir::Type.
  MemberInfo makeStorageInfo(CharUnits offset, mlir::Type data) {
    return MemberInfo(offset, MemberInfo::InfoKind::Field, data);
  }

  // Layout routines.
  void setBitFieldInfo(const FieldDecl *fd, CharUnits startOffset,
                       mlir::Type storageType);

  void lower(bool NonVirtualBaseType);
  void lowerUnion();

  /// Determines if we need a packed llvm struct.
  void determinePacked(bool nvBaseType);
  /// Inserts padding everywhere it's needed.
  void insertPadding();

  void computeVolatileBitfields();
  void accumulateBases();
  void accumulateVPtrs();
  void accumulateVBases();
  void accumulateFields();
  RecordDecl::field_iterator
  accumulateBitFields(RecordDecl::field_iterator field,
                      RecordDecl::field_iterator fieldEnd);

  mlir::Type getVFPtrType();

  bool isAAPCS() const {
    return astContext.getTargetInfo().getABI().starts_with("aapcs");
  }

  /// Helper function to check if the target machine is BigEndian.
  bool isBigEndian() const { return astContext.getTargetInfo().isBigEndian(); }

  // The Itanium base layout rule allows virtual bases to overlap
  // other bases, which complicates layout in specific ways.
  //
  // Note specifically that the ms_struct attribute doesn't change this.
  bool isOverlappingVBaseABI() {
    return !astContext.getTargetInfo().getCXXABI().isMicrosoft();
  }
  // Recursively searches all of the bases to find out if a vbase is
  // not the primary vbase of some base class.
  bool hasOwnStorage(const CXXRecordDecl *decl, const CXXRecordDecl *query);

  /// The Microsoft bitfield layout rule allocates discrete storage
  /// units of the field's formal type and only combines adjacent
  /// fields of the same formal type.  We want to emit a layout with
  /// these discrete storage units instead of combining them into a
  /// continuous run.
  bool isDiscreteBitFieldABI() {
    return astContext.getTargetInfo().getCXXABI().isMicrosoft() ||
           recordDecl->isMsStruct(astContext);
  }

  CharUnits bitsToCharUnits(uint64_t bitOffset) {
    return astContext.toCharUnitsFromBits(bitOffset);
  }

  void calculateZeroInit();

  CharUnits getSize(mlir::Type Ty) {
    return CharUnits::fromQuantity(dataLayout.layout.getTypeSize(Ty));
  }
  CharUnits getSizeInBits(mlir::Type ty) {
    return CharUnits::fromQuantity(dataLayout.layout.getTypeSizeInBits(ty));
  }
  CharUnits getAlignment(mlir::Type Ty) {
    return CharUnits::fromQuantity(dataLayout.layout.getTypeABIAlignment(Ty));
  }

  bool isZeroInitializable(const FieldDecl *fd) {
    return cirGenTypes.isZeroInitializable(fd->getType());
  }
  bool isZeroInitializable(const RecordDecl *rd) {
    return cirGenTypes.isZeroInitializable(rd);
  }

  /// Wraps cir::IntType with some implicit arguments.
  mlir::Type getUIntNType(uint64_t numBits) {
    unsigned alignedBits = llvm::PowerOf2Ceil(numBits);
    alignedBits = std::max(8u, alignedBits);
    return cir::IntType::get(&cirGenTypes.getMLIRContext(), alignedBits,
                             /*isSigned=*/false);
  }

  mlir::Type getCharType() {
    return cir::IntType::get(&cirGenTypes.getMLIRContext(),
                             astContext.getCharWidth(),
                             /*isSigned=*/false);
  }

  mlir::Type getByteArrayType(CharUnits numberOfChars) {
    assert(!numberOfChars.isZero() && "Empty byte arrays aren't allowed.");
    mlir::Type type = getCharType();
    return numberOfChars == CharUnits::One()
               ? type
               : cir::ArrayType::get(type, numberOfChars.getQuantity());
  }

  // Gets the CIR BaseSubobject type from a CXXRecordDecl.
  mlir::Type getStorageType(const CXXRecordDecl *RD) {
    return cirGenTypes.getCIRGenRecordLayout(RD).getBaseSubobjectCIRType();
  }
  // This is different from LLVM traditional codegen because CIRGen uses arrays
  // of bytes instead of arbitrary-sized integers. This is important for packed
  // structures support.
  mlir::Type getBitfieldStorageType(unsigned numBits) {
    unsigned alignedBits = llvm::alignTo(numBits, astContext.getCharWidth());
    if (cir::isValidFundamentalIntWidth(alignedBits))
      return builder.getUIntNTy(alignedBits);

    mlir::Type type = getCharType();
    return cir::ArrayType::get(type, alignedBits / astContext.getCharWidth());
  }

  mlir::Type getStorageType(const FieldDecl *fieldDecl) {
    mlir::Type type = cirGenTypes.convertTypeForMem(fieldDecl->getType());
    if (fieldDecl->isBitField()) {
      cirGenTypes.getCGModule().errorNYI(recordDecl->getSourceRange(),
                                         "getStorageType for bitfields");
    }
    return type;
  }

  uint64_t getFieldBitOffset(const FieldDecl *fieldDecl) {
    return astRecordLayout.getFieldOffset(fieldDecl->getFieldIndex());
  }

  /// Fills out the structures that are ultimately consumed.
  void fillOutputFields();

  void appendPaddingBytes(CharUnits size) {
    if (size.isZero())
      return;
    mlir::Type padTy = getByteArrayType(size);
    padded = true;
    if (recordDecl->isUnion()) {
      assert(!unionPadding && "at most one union tail-padding type");
      unionPadding = padTy;
    } else {
      fieldTypes.push_back(padTy);
    }
  }

  CIRGenTypes &cirGenTypes;
  CIRGenBuilderTy &builder;
  const ASTContext &astContext;
  const RecordDecl *recordDecl;
  const CXXRecordDecl *cxxRecordDecl;
  const ASTRecordLayout &astRecordLayout;
  // Helpful intermediate data-structures
  std::vector<MemberInfo> members;
  // Output fields, consumed by CIRGenTypes::computeRecordLayout
  llvm::SmallVector<mlir::Type, 16> fieldTypes;
  mlir::Type unionPadding;
  llvm::DenseMap<const FieldDecl *, CIRGenBitFieldInfo> bitFields;
  llvm::DenseMap<const FieldDecl *, unsigned> fieldIdxMap;
  llvm::DenseMap<const CXXRecordDecl *, unsigned> nonVirtualBases;
  llvm::DenseMap<const CXXRecordDecl *, unsigned> virtualBases;
  cir::CIRDataLayout dataLayout;

  LLVM_PREFERRED_TYPE(bool)
  unsigned zeroInitializable : 1;
  LLVM_PREFERRED_TYPE(bool)
  unsigned zeroInitializableAsBase : 1;
  LLVM_PREFERRED_TYPE(bool)
  unsigned packed : 1;
  LLVM_PREFERRED_TYPE(bool)
  unsigned padded : 1;

private:
  CIRRecordLowering(const CIRRecordLowering &) = delete;
  void operator=(const CIRRecordLowering &) = delete;
}; // CIRRecordLowering
static const FunctionTemplateDecl *
getEnclosingFunctionTemplatePattern(const RecordDecl *record,
                                    bool &isLocalRecord) {
  for (const DeclContext *context = record->getDeclContext(); context;
       context = context->getParent()) {
    const auto *function = dyn_cast<FunctionDecl>(context);
    if (!function)
      continue;

    isLocalRecord = true;
    const FunctionDecl *pattern = function->getTemplateInstantiationPattern();
    if (!pattern)
      pattern = function;
    if (const auto *functionTemplate = pattern->getDescribedFunctionTemplate())
      return functionTemplate;
  }
  return nullptr;
}

struct StableSourceRecordLocation final {
  std::string file;
  unsigned offset;
  unsigned line;
  unsigned column;
};

static std::optional<StableSourceRecordLocation>
stableSourceRecordLocation(const SourceManager &sourceManager,
                           SourceLocation location) {
  if (location.isInvalid() || location.isMacroID())
    return std::nullopt;
  FileIDAndOffset decomposed = sourceManager.getDecomposedLoc(location);
  if (decomposed.first.isInvalid())
    return std::nullopt;
  PresumedLoc presumed = sourceManager.getPresumedLoc(location);
  if (!presumed.isValid() || !presumed.getFilename())
    return std::nullopt;

  llvm::SmallString<256> normalizedFile(presumed.getFilename());
  if (!llvm::sys::path::is_absolute(normalizedFile))
    sourceManager.getFileManager().makeAbsolutePath(normalizedFile);
  llvm::sys::path::remove_dots(normalizedFile, /*remove_dot_dot=*/true);
  if (normalizedFile.empty() || !llvm::sys::path::is_absolute(normalizedFile))
    return std::nullopt;

  return StableSourceRecordLocation{normalizedFile.str().str(),
                                    decomposed.second, presumed.getLine(),
                                    presumed.getColumn()};
}

static std::optional<llvm::StringRef>
stableSourceRecordTagKind(const RecordDecl *record) {
  if (record->isStruct())
    return "struct";
  if (record->isClass())
    return "class";
  if (record->isUnion())
    return "union";
  return std::nullopt;
}

static std::optional<std::string>
stableSourceRecordPatternUSR(const FunctionTemplateDecl *enclosingTemplate) {
  if (!enclosingTemplate)
    return std::string();

  llvm::SmallString<256> usr;
  if (clang::index::generateUSRForDecl(enclosingTemplate->getTemplatedDecl(),
                                       usr))
    return std::nullopt;
  return usr.str().str();
}

static std::optional<std::string>
localRecordSourceIdentity(CIRGenModule &cgm, const RecordDecl *record) {
  bool isLocalRecord = false;
  const FunctionTemplateDecl *enclosingTemplate =
      getEnclosingFunctionTemplatePattern(record, isLocalRecord);
  SourceLocation recordLocation = record->getLocation();
  if (!isLocalRecord || recordLocation.isInvalid() ||
      recordLocation.isMacroID())
    return std::nullopt;

  const SourceManager &sourceManager = cgm.getASTContext().getSourceManager();
  auto sourceLocation = stableSourceRecordLocation(
      sourceManager, sourceManager.getSpellingLoc(recordLocation));
  auto tagKind = stableSourceRecordTagKind(record);
  auto patternUSR = stableSourceRecordPatternUSR(enclosingTemplate);
  if (!sourceLocation || !tagKind || !patternUSR)
    return std::nullopt;

  std::string identity;
  llvm::raw_string_ostream stream(identity);
  stream << "cxx-source-record:v1:" << sourceLocation->file.size() << ':'
         << sourceLocation->file << ':' << sourceLocation->offset << ':'
         << sourceLocation->line << ':' << sourceLocation->column << ':'
         << *tagKind << ':' << patternUSR->size() << ':' << *patternUSR;
  stream.flush();
  return identity;
}

static std::optional<std::string>
localMacroRecordSourceIdentity(CIRGenModule &cgm, const RecordDecl *record) {
  bool isLocalRecord = false;
  const FunctionTemplateDecl *enclosingTemplate =
      getEnclosingFunctionTemplatePattern(record, isLocalRecord);
  SourceLocation recordLocation = record->getLocation();
  if (!isLocalRecord || recordLocation.isInvalid() ||
      !recordLocation.isMacroID())
    return std::nullopt;

  const SourceManager &sourceManager = cgm.getASTContext().getSourceManager();
  auto expansionLocation = stableSourceRecordLocation(
      sourceManager, sourceManager.getExpansionLoc(recordLocation));
  auto spellingLocation = stableSourceRecordLocation(
      sourceManager, sourceManager.getSpellingLoc(recordLocation));
  auto tagKind = stableSourceRecordTagKind(record);
  auto patternUSR = stableSourceRecordPatternUSR(enclosingTemplate);
  if (!expansionLocation || !spellingLocation || !tagKind || !patternUSR)
    return std::nullopt;

  std::string identity;
  llvm::raw_string_ostream stream(identity);
  stream << "cxx-source-record:v2:" << expansionLocation->file.size() << ':'
         << expansionLocation->file << ':' << expansionLocation->offset << ':'
         << expansionLocation->line << ':' << expansionLocation->column << ':'
         << spellingLocation->file.size() << ':' << spellingLocation->file
         << ':' << spellingLocation->offset << ':' << spellingLocation->line
         << ':' << spellingLocation->column << ':' << *tagKind << ':'
         << patternUSR->size() << ':' << *patternUSR;
  stream.flush();
  return identity;
}

} // namespace

namespace {

std::string sha256Hex(std::initializer_list<llvm::StringRef> parts) {
  llvm::SHA256 hasher;
  for (llvm::StringRef part : parts)
    hasher.update(part);
  const auto digest = hasher.final();
  static constexpr char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve(digest.size() * 2);
  for (uint8_t byte : digest) {
    out.push_back(hex[byte >> 4]);
    out.push_back(hex[byte & 0x0f]);
  }
  return out;
}
uint64_t fnv1a64(llvm::StringRef value) {
  uint64_t hash = 1469598103934665603ULL;
  for (unsigned char c : value) {
    hash ^= static_cast<uint64_t>(c);
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::string hex64(uint64_t value) {
  std::ostringstream out;
  out << std::hex << std::nouppercase << std::setfill('0') << std::setw(16)
      << value;
  return out.str();
}

std::optional<std::string>
declarationLayoutSourcePayload(CIRGenModule &cgm,
                               const NamedDecl *decl) {
  if (!decl)
    return std::nullopt;
  const SourceManager &sm = cgm.getASTContext().getSourceManager();
  SourceLocation loc = decl->getLocation();
  loc = loc.isMacroID() ? sm.getExpansionLoc(loc) : sm.getSpellingLoc(loc);
  if (loc.isInvalid())
    return std::nullopt;
  const FileID file = sm.getFileID(loc);
  if (file.isInvalid())
    return std::nullopt;
  const auto fileEntry = sm.getFileEntryRefForID(file);
  if (!fileEntry)
    return std::nullopt;
  std::ostringstream out;
  if (auto prefix = cgm.layoutSourceFileAnchorPrefix(file)) {
    out << prefix->str() << "\noffset:" << sm.getFileOffset(loc);
    return out.str();
  }
  llvm::SmallString<256> canonicalPath;
  if (sm.getFileManager().getVirtualFileSystem().getRealPath(
          fileEntry->getName(), canonicalPath) ||
      canonicalPath.empty() || !llvm::sys::path::is_absolute(canonicalPath))
    return std::nullopt;
  llvm::sys::path::remove_dots(canonicalPath, /*remove_dot_dot=*/true);
  bool invalid = false;
  llvm::StringRef buffer = sm.getBufferData(file, &invalid);
  if (invalid)
    return std::nullopt;
  std::ostringstream prefix;
  prefix << "path:" << canonicalPath.str().str()
         << "\nsha256:"
         << sha256Hex({llvm::StringRef(buffer.data(), buffer.size())});
  out << cgm.rememberLayoutSourceFileAnchorPrefix(file, prefix.str()).str()
      << "\noffset:" << sm.getFileOffset(loc);
  return out.str();
}

bool needsLayoutSourceDiscriminator(const RecordDecl *record) {
  return record &&
         (record->getIdentifier() == nullptr ||
          record->getParentFunctionOrMethod() != nullptr ||
          !record->isExternallyVisible());
}

// Lexical ordinal of a function-local unnamed tag among every unnamed tag the
// owning function's definition spells, in traversal order. Rewritten default
// argument initializers are traversed explicitly: they materialize fresh
// per-callsite closure types whose only exact owner facts are this function
// and this ordinal. Mirrors the importer's symbol_identity computation.
std::optional<unsigned>
functionLocalUnnamedTagOrdinal(const FunctionDecl *function,
                               const RecordDecl *record) {
  const FunctionDecl *definition = function->getDefinition();
  if (!definition)
    definition = function;

  struct Collector : DynamicRecursiveASTVisitor {
    llvm::SmallVector<const TagDecl *, 16> Tags;
    llvm::SmallPtrSet<const TagDecl *, 16> Seen;
    void add(const TagDecl *TD) {
      if (!TD)
        return;
      TD = cast<TagDecl>(TD->getCanonicalDecl());
      if (TD->getIdentifier() || TD->getTypedefNameForAnonDecl())
        return;
      if (Seen.insert(TD).second)
        Tags.push_back(TD);
    }
    bool VisitLambdaExpr(LambdaExpr *E) override {
      add(E->getLambdaClass());
      return true;
    }
    bool VisitTagDecl(TagDecl *TD) override {
      const auto *Record = dyn_cast<CXXRecordDecl>(TD);
      if (!Record || !Record->isLambda())
        add(TD);
      return true;
    }
    bool TraverseCXXDefaultArgExpr(CXXDefaultArgExpr *E) override {
      if (E && E->hasRewrittenInit())
        TraverseStmt(E->getExpr());
      return true;
    }
  };
  Collector collector;
  for (const ParmVarDecl *param : definition->parameters())
    if (param->hasDefaultArg() && !param->hasUnparsedDefaultArg() &&
        !param->hasUninstantiatedDefaultArg())
      collector.TraverseStmt(
          const_cast<Expr *>(param->getDefaultArg()));
  if (const auto *ctor = dyn_cast<CXXConstructorDecl>(definition))
    for (const CXXCtorInitializer *init : ctor->inits())
      if (init->getInit())
        collector.TraverseStmt(const_cast<Expr *>(init->getInit()));
  if (Stmt *body = definition->getBody())
    collector.TraverseStmt(body);

  const auto *canonical = cast<TagDecl>(record->getCanonicalDecl());
  for (unsigned index = 0; index < collector.Tags.size(); ++index)
    if (collector.Tags[index] == canonical)
      return index;
  return std::nullopt;
}

// True when any type the ABI must spell for this function is an unnamed
// type. Those spellings come from the mangler's counter, not from the source.
bool typeEmbedsUnnamedTypeCounter(QualType type, unsigned depth = 0) {
  if (type.isNull() || depth > 8)
    return false;
  const clang::Type *canonical = type.getCanonicalType().getTypePtrOrNull();
  if (!canonical)
    return false;
  if (const auto *tag = canonical->getAsTagDecl()) {
    if (!tag->getIdentifier() && !tag->getTypedefNameForAnonDecl()) {
      // A closure with a nonzero lambda mangling number mangles as
      // Ul...E<n>_ from a source-stable per-context counter. Only unnamed
      // types without such a number fall back to the mangler-instance
      // $_N/UtN_ ids.
      const auto *closure = dyn_cast<CXXRecordDecl>(tag);
      if (!closure || !closure->isLambda() ||
          closure->getLambdaManglingNumber() == 0)
        return true;
      // A numbered closure still spells its lexical owner chain in the
      // mangled name (Z<owner>E...Ul...E<n>_). Any enclosing closure without
      // a mangling number, or an enclosing unnamed non-closure record,
      // reintroduces the mangler-instance $_N/UtN_ id into that owner
      // spelling, so the whole name remains request-order state.
      for (const DeclContext *ctx = closure->getDeclContext(); ctx;
           ctx = ctx->getParent()) {
        const auto *owner = dyn_cast<CXXRecordDecl>(ctx);
        if (!owner)
          continue;
        if (owner->isLambda()) {
          if (owner->getLambdaManglingNumber() == 0)
            return true;
          continue;
        }
        if (!owner->getIdentifier() && !owner->getTypedefNameForAnonDecl())
          return true;
      }
    }
    // A named specialization still spells every template argument, so a
    // closure or unnamed enum anywhere in the argument tree reaches the
    // mangled name (tuple<..., $_0> and friends).
    if (const auto *specialization =
            dyn_cast<ClassTemplateSpecializationDecl>(tag)) {
      auto argumentEmbeds = [&](const TemplateArgument &argument,
                                auto &self) -> bool {
        switch (argument.getKind()) {
        case TemplateArgument::Type:
          return typeEmbedsUnnamedTypeCounter(argument.getAsType(), depth + 1);
        case TemplateArgument::Integral:
          return typeEmbedsUnnamedTypeCounter(argument.getIntegralType(),
                                              depth + 1);
        case TemplateArgument::Pack:
          for (const TemplateArgument &element : argument.pack_elements())
            if (self(element, self))
              return true;
          return false;
        default:
          return false;
        }
      };
      for (const TemplateArgument &argument :
           specialization->getTemplateArgs().asArray())
        if (argumentEmbeds(argument, argumentEmbeds))
          return true;
    }
    return false;
  }
  if (const auto *pointer = canonical->getAs<clang::PointerType>())
    return typeEmbedsUnnamedTypeCounter(pointer->getPointeeType(), depth + 1);
  if (const auto *reference = canonical->getAs<ReferenceType>())
    return typeEmbedsUnnamedTypeCounter(reference->getPointeeType(),
                                        depth + 1);
  if (const auto *array =
          canonical->getAsArrayTypeUnsafe())
    return typeEmbedsUnnamedTypeCounter(array->getElementType(), depth + 1);
  if (const auto *memberPointer =
          canonical->getAs<clang::MemberPointerType>()) {
    if (const CXXRecordDecl *owner =
            memberPointer->getMostRecentCXXRecordDecl()) {
      const QualType ownerType =
          owner->getASTContext().getCanonicalTagType(owner);
      if (typeEmbedsUnnamedTypeCounter(ownerType, depth + 1))
        return true;
    }
    return typeEmbedsUnnamedTypeCounter(memberPointer->getPointeeType(),
                                        depth + 1);
  }
  if (const auto *function = canonical->getAs<clang::FunctionProtoType>()) {
    if (typeEmbedsUnnamedTypeCounter(function->getReturnType(), depth + 1))
      return true;
    for (QualType parameter : function->param_types())
      if (typeEmbedsUnnamedTypeCounter(parameter, depth + 1))
        return true;
    return false;
  }
  if (const auto *function =
          canonical->getAs<clang::FunctionNoProtoType>())
    return typeEmbedsUnnamedTypeCounter(function->getReturnType(), depth + 1);
  return false;
}

bool functionManglingEmbedsUnnamedTypeCounter(const FunctionDecl *function) {
  if (!function)
    return false;
  if (const TemplateArgumentList *arguments =
          function->getTemplateSpecializationArgs()) {
    for (const TemplateArgument &argument : arguments->asArray()) {
      if (argument.getKind() == TemplateArgument::Type &&
          typeEmbedsUnnamedTypeCounter(argument.getAsType()))
        return true;
      if (argument.getKind() == TemplateArgument::Integral &&
          typeEmbedsUnnamedTypeCounter(argument.getIntegralType()))
        return true;
    }
  }
  for (const ParmVarDecl *parameter : function->parameters())
    if (parameter && typeEmbedsUnnamedTypeCounter(parameter->getType()))
      return true;
  if (const auto *method = dyn_cast<CXXMethodDecl>(function))
    if (const CXXRecordDecl *parent = method->getParent())
      if (typeEmbedsUnnamedTypeCounter(
              method->getASTContext().getCanonicalTagType(parent)))
        return true;
  return false;
}

// Deterministic anchor for the owning function. Constructors and destructors
// need an explicit structor variant to have one mangling.
//
// A mangled name is only usable as an anchor while it is a pure function of
// the source. Clang spells an unnamed type by its position in the mangler's
// own request sequence ($_0, $_1, Ut0_, ...), so a specialization on an
// unnamed type mangles differently depending on what was mangled before it,
// and two readers of the same source disagree. Those owners anchor on their
// USR instead, which names the unnamed type through its first enumerator or
// declaration and is therefore recomputable by any reader.
std::string mangledFunctionAnchor(clang::MangleContext &mangleContext,
                                  const FunctionDecl *function) {
  if (!function)
    return {};
  // A declaration the ABI never mangles (C linkage, main, ...) must not
  // reach MangleContext::mangleName: it asserts. Its USR is still a unique,
  // source-derived anchor, so closures inside such functions keep their
  // lexical-ordinal identity instead of failing closed.
  if (!mangleContext.shouldMangleDeclName(function) ||
      functionManglingEmbedsUnnamedTypeCounter(function)) {
    llvm::SmallString<256> usr;
    if (clang::index::generateUSRForDecl(function, usr) || usr.empty())
      return {};
    return ("usr:" + usr).str();
  }
  GlobalDecl target;
  if (const auto *ctor = dyn_cast<CXXConstructorDecl>(function))
    target = GlobalDecl(ctor, Ctor_Complete);
  else if (const auto *dtor = dyn_cast<CXXDestructorDecl>(function))
    target = GlobalDecl(dtor, Dtor_Complete);
  else
    target = GlobalDecl(function);
  std::string anchor;
  llvm::raw_string_ostream stream(anchor);
  mangleContext.mangleName(target, stream);
  stream.flush();
  return anchor;
}
std::optional<std::string>
anonymousRecordSourceDiscriminator(CIRGenModule &cgm,
                                   const RecordDecl *record) {
  if (!record || record->getIdentifier())
    return std::string();
  const SourceLocation location = record->getLocation();
  if (location.isInvalid())
    return std::nullopt;
  const SourceManager &sourceManager = cgm.getASTContext().getSourceManager();
  const auto expansion = stableSourceRecordLocation(
      sourceManager, sourceManager.getExpansionLoc(location));
  const auto spelling = stableSourceRecordLocation(
      sourceManager, sourceManager.getSpellingLoc(location));
  const auto tagKind = stableSourceRecordTagKind(record);
  if (!expansion || !spelling || !tagKind)
    return std::nullopt;
  std::ostringstream payload;
  payload << expansion->file.size() << ':' << expansion->file << ':'
          << expansion->offset << ':' << expansion->line << ':'
          << expansion->column << ':' << spelling->file.size() << ':'
          << spelling->file << ':' << spelling->offset << ':' << spelling->line
          << ':' << spelling->column << ':' << tagKind->str();
  return sha256Hex({"anonymous-record-source-v1", payload.str()});
}
void collectTemplateArgumentRecordOwners(
    QualType type, std::vector<const RecordDecl *> &owners) {
  if (type.isNull())
    return;
  type = type.getCanonicalType();
  if (const auto *recordType = type->getAs<RecordType>()) {
    owners.push_back(recordType->getDecl());
    return;
  }
  if (const auto *memberPointer = type->getAs<MemberPointerType>()) {
    if (const clang::Type *classType =
            memberPointer->getQualifier().getAsType())
      collectTemplateArgumentRecordOwners(QualType(classType, 0), owners);
    collectTemplateArgumentRecordOwners(memberPointer->getPointeeType(), owners);
    return;
  }
  if (const auto *function = type->getAs<FunctionProtoType>()) {
    collectTemplateArgumentRecordOwners(function->getReturnType(), owners);
    for (QualType parameter : function->param_types())
      collectTemplateArgumentRecordOwners(parameter, owners);
    return;
  }
  if (const auto *function = type->getAs<FunctionNoProtoType>()) {
    collectTemplateArgumentRecordOwners(function->getReturnType(), owners);
    return;
  }
  if (const auto *array = type->getAsArrayTypeUnsafe()) {
    collectTemplateArgumentRecordOwners(array->getElementType(), owners);
    return;
  }
  if (const auto *atomic = type->getAs<AtomicType>()) {
    collectTemplateArgumentRecordOwners(atomic->getValueType(), owners);
    return;
  }
  if (const auto *pack = type->getAs<PackExpansionType>()) {
    collectTemplateArgumentRecordOwners(pack->getPattern(), owners);
    return;
  }
  const QualType pointee = type->getPointeeType();
  if (!pointee.isNull())
    collectTemplateArgumentRecordOwners(pointee, owners);
}
const CXXRecordDecl *stableSpecializationODROwner(
    const CXXRecordDecl *record,
    const ClassTemplateSpecializationDecl *specialization) {
  if (!record || !specialization)
    return record;
  // An instantiated specialization's selected partial pattern and
  // completeness can change during lazy instantiation. Exact TemplateArgs and
  // recursively owned argument identities already distinguish it. Only
  // source-owned specialization declarations contribute their parsed ODR
  // fact: explicit concrete specializations and partial-specialization
  // patterns. Primary template declarations continue through the record path.
  if (specialization->getSpecializationKind() !=
          TSK_ExplicitSpecialization &&
      !isa<ClassTemplatePartialSpecializationDecl>(specialization))
    return nullptr;
  return record->getDefinition();
}


std::optional<std::string>
recordDeclIdentityImpl(CIRGenModule &cgm, const RecordDecl *decl,
                       llvm::DenseSet<const RecordDecl *> &inProgress) {
  if (!decl)
    return std::nullopt;
  const RecordDecl *definition = decl->getDefinition();
  const RecordDecl *record = definition ? definition : decl;
  // A concrete specialization's definition owns its exact substituted
  // TemplateArgument sequence. Canonicalizing through a retained partial-
  // specialization pattern erases distinct packs before identity hashing.
  if (!isa<ClassTemplateSpecializationDecl>(record)) {
    if (const auto *canonical =
            dyn_cast_or_null<RecordDecl>(record->getCanonicalDecl()))
      record = canonical;
    if (const RecordDecl *canonicalDefinition = record->getDefinition())
      record = canonicalDefinition;
  }
  if (!inProgress.insert(record).second)
    return std::nullopt;
  auto eraseRecord = llvm::make_scope_exit([&] { inProgress.erase(record); });

  // Clang's source and layout views can disagree on whether an injected
  // anonymous aggregate has a USR. Its containing record, exact injected-field
  // ordinal, and source location provide one identity for both views.
  if (!record->getIdentifier()) {
    const DeclContext *context = record->getDeclContext();
    const auto *parent =
        context ? dyn_cast<RecordDecl>(Decl::castFromDeclContext(context))
                : nullptr;
    std::optional<unsigned> injectedFieldIndex;
    if (parent) {
      for (const FieldDecl *field : parent->fields()) {
        const auto *fieldRecord =
            field->getType()->getAsCanonical<RecordType>();
        if (fieldRecord && fieldRecord->getDecl()->getCanonicalDecl() ==
                               record->getCanonicalDecl()) {
          injectedFieldIndex = field->getFieldIndex();
          break;
        }
      }
    }
    auto parentIdentity =
        parent ? recordDeclIdentityImpl(cgm, parent, inProgress) : std::nullopt;
    auto sourceDiscriminator = anonymousRecordSourceDiscriminator(cgm, record);
    if (parentIdentity && injectedFieldIndex && sourceDiscriminator) {
      return "clang-anonymous-record:v1:" +
             std::to_string(parentIdentity->size()) + ":" + *parentIdentity +
             ":" + std::to_string(*injectedFieldIndex) + ":" +
             *sourceDiscriminator;
    }
  }
  llvm::SmallString<256> usr;
  if (!clang::index::generateUSRForDecl(record, usr)) {
    std::string identity = usr.str().str();
    const auto *cxxRecord = dyn_cast<CXXRecordDecl>(record);
    const auto *concreteSpecialization =
        dyn_cast_or_null<ClassTemplateSpecializationDecl>(cxxRecord);
    // Referenced-only concrete specializations can remain incomplete while
    // still owning an exact substituted TemplateArgument sequence.
    const bool specializationBearing =
        concreteSpecialization ||
        (cxxRecord && cxxRecord->isCompleteDefinition() &&
         cxxRecord->getDescribedClassTemplate() != nullptr);
    if (specializationBearing) {
      ODRHash argumentHash;
      if (concreteSpecialization) {
        const auto *specialization = concreteSpecialization;
        for (const TemplateArgument &argument :
             specialization->getTemplateArgs().asArray())
          argumentHash.AddTemplateArgument(argument);
        std::ostringstream exactArgumentOwners;
        bool hasExactArgumentOwner = false;
        bool exactArgumentOwnersComplete = true;
        std::size_t argumentIndex = 0;
        // A pack argument names its element types: a variadic trait such as
        // std::conjunction<B...> distinguishes sibling closures only through
        // the pack, so owner identities come from the flattened leaf
        // sequence. Leaves are indexed in visit order; a sequence without
        // packs enumerates exactly as before.
        auto visitArgument = [&](const TemplateArgument &argument,
                                 auto &self) -> void {
          if (argument.getKind() == TemplateArgument::Pack) {
            for (const TemplateArgument &element : argument.pack_elements())
              self(element, self);
            return;
          }
          if (argument.getKind() == TemplateArgument::Type) {
            std::vector<const RecordDecl *> argumentRecords;
            collectTemplateArgumentRecordOwners(argument.getAsType(),
                                                argumentRecords);
            for (std::size_t ownerIndex = 0;
                 ownerIndex < argumentRecords.size(); ++ownerIndex) {
              const RecordDecl *argumentRecord = argumentRecords[ownerIndex];
              // Identity observation must not instantiate a referenced-only
              // argument specialization: doing so conditionally adds its ODR
              // suffix and makes the enclosing owner depend on CIR lowering
              // order rather than the compiler TemplateArgument graph.
              auto argumentOwner =
                  recordDeclIdentityImpl(cgm, argumentRecord, inProgress);
              if (!argumentOwner) {
                exactArgumentOwnersComplete = false;
                continue;
              }
              hasExactArgumentOwner = true;
              exactArgumentOwners << argumentIndex;
              if (ownerIndex != 0)
                exactArgumentOwners << "." << ownerIndex;
              exactArgumentOwners << ':' << argumentOwner->size() << ':'
                                  << *argumentOwner << ';';
            }
          }
          ++argumentIndex;
        };
        for (const TemplateArgument &argument :
             specialization->getTemplateArgs().asArray())
          visitArgument(argument, visitArgument);
        if (!exactArgumentOwnersComplete) {
          identity.clear();
        } else if (hasExactArgumentOwner) {
          const std::string owners = exactArgumentOwners.str();
          identity += "#argowners:" +
                      sha256Hex({"record-template-argument-owners-v1", owners});
        }
      } else {
        argumentHash.AddQualType(
            cgm.getASTContext().getCanonicalTagType(cxxRecord));
      }
      if (!identity.empty()) {
        std::ostringstream suffix;
        const CXXRecordDecl *odrOwner =
            stableSpecializationODROwner(cxxRecord,
                                         concreteSpecialization);
        if (odrOwner && odrOwner->isCompleteDefinition()) {
          suffix << "#odr:" << std::hex << std::nouppercase
                 << std::setfill('0') << std::setw(8)
                 << odrOwner->getODRHash();
        }
        suffix << "#args:" << std::hex << std::nouppercase
               << std::setfill('0') << std::setw(8)
               << argumentHash.CalculateHash();
        identity += suffix.str();
      }
    }
    if (!identity.empty() && !record->getIdentifier()) {
      auto sourceDiscriminator =
          anonymousRecordSourceDiscriminator(cgm, record);
      if (!sourceDiscriminator) {
        identity.clear();
      } else {
        identity += "#anonymous-source:" + *sourceDiscriminator;
      }
    }
    if (!identity.empty() && !record->getIdentifier()) {
      // A location discriminator cannot separate sibling closures spelled by
      // one macro argument that is evaluated twice, and Clang's lambda USR
      // can erase the distinguishing template argument of the enclosing
      // specialization. A function-local anonymous record therefore also
      // binds the mangled name of its enclosing function plus its lexical
      // ordinal among that function's local unnamed tags. The ordinal walk
      // descends into rewritten default-argument initializers because those
      // materialize fresh per-callsite closures the mangler's own counters
      // number in request order, which is not stable across mangler
      // instances.
      const auto *localCxxRecord = dyn_cast<CXXRecordDecl>(record);
      const clang::FunctionDecl *owningFunction = nullptr;
      for (const DeclContext *context = record->getDeclContext(); context;
           context = context->getParent()) {
        if (const auto *function = dyn_cast<FunctionDecl>(context)) {
          owningFunction = function;
          break;
        }
      }
      if (localCxxRecord && owningFunction &&
          !owningFunction->isTemplated()) {
        const std::optional<unsigned> ordinal =
            functionLocalUnnamedTagOrdinal(owningFunction, record);
        std::string anchor =
            mangledFunctionAnchor(cgm.getIdentityMangleContext(),
                                  owningFunction);
        if (!ordinal.has_value() || anchor.empty()) {
          if (getenv("AENEAS_FNLOCAL_TRACE")) {
            llvm::errs() << "AENEAS_FNLOCAL cleared record="
                         << record->getQualifiedNameAsString() << " owner="
                         << owningFunction->getQualifiedNameAsString()
                         << " owner_has_def="
                         << (owningFunction->getDefinition() != nullptr)
                         << " ordinal=" << (ordinal ? int(*ordinal) : -1)
                         << " anchor_empty=" << anchor.empty() << " loc="
                         << record->getLocation().printToString(
                                cgm.getASTContext().getSourceManager())
                         << "\n";
          }
          identity.clear();
        } else {
          identity += "#fnlocal:" +
                      sha256Hex({"record-function-local-owner-v2", anchor,
                                 ":", std::to_string(*ordinal)});
        }
      }
    }
    if (!identity.empty() && record->getIdentifier()) {
      const FunctionDecl *owningFunction = nullptr;
      for (const DeclContext *context = record->getDeclContext(); context;
           context = context->getParent()) {
        if (const auto *function = dyn_cast<FunctionDecl>(context)) {
          owningFunction = function;
          break;
        }
      }
      if (owningFunction) {
        const std::string anchor =
            mangledFunctionAnchor(cgm.getIdentityMangleContext(),
                                  owningFunction);
        if (anchor.empty()) {
          identity.clear();
        } else {
          identity += "#fnowner:" +
                      sha256Hex(
                          {"record-exact-function-owner-v1", anchor});
        }
      }
    }
    if (!identity.empty()) {
      const DeclContext *context = record->getDeclContext();
      const auto *parent =
          context ? dyn_cast<CXXRecordDecl>(Decl::castFromDeclContext(context))
                  : nullptr;
      if (parent) {
        if (const CXXRecordDecl *parentDefinition = parent->getDefinition())
          parent = parentDefinition;
        const bool specializationParent =
            parent->isCompleteDefinition() &&
            (isa<ClassTemplateSpecializationDecl>(parent) ||
             parent->getDescribedClassTemplate() != nullptr);
        if (specializationParent) {
          auto parentIdentity = recordDeclIdentityImpl(cgm, parent, inProgress);
          if (!parentIdentity) {
            identity.clear();
          } else {
            identity += "#owner:" + sha256Hex({"record-specialization-owner-v1",
                                               *parentIdentity});
          }
        }
      }
    }
    if (!identity.empty() && record->getIdentifier() &&
        needsLayoutSourceDiscriminator(record)) {
      const std::optional<std::string> source =
          declarationLayoutSourcePayload(cgm, record);
      if (!source)
        return std::nullopt;
      identity += "#decl." + hex64(fnv1a64(*source));
    }
    if (!identity.empty())
      return identity;
  }

  // Local records without a direct Clang USR need an identity that survives
  // ABI lambda discriminator changes. A nonmacro definition uses v1 source
  // provenance; a macro definition uses v2's expansion-and-spelling pair so
  // repeated macro expansions cannot collide. Both variants require the exact
  // enclosing function-template pattern USR and otherwise fail closed to RTTI.
  if (auto sourceIdentity = localRecordSourceIdentity(cgm, record))
    return sourceIdentity;
  if (auto macroSourceIdentity = localMacroRecordSourceIdentity(cgm, record))
    return macroSourceIdentity;
  const auto *cxxRecord = dyn_cast<CXXRecordDecl>(record);
  if (!cxxRecord)
    return std::nullopt;

  std::string rttiName;
  llvm::raw_string_ostream rttiNameStream(rttiName);
  QualType canonicalType = cgm.getASTContext().getCanonicalTagType(cxxRecord);
  cgm.getCXXABI().getMangleContext().mangleCXXRTTIName(canonicalType,
                                                       rttiNameStream);
  rttiNameStream.flush();
  if (rttiName.empty())
    return std::nullopt;
  return "cxx-rtti-name:" + rttiName;
}

} // namespace

std::optional<unsigned>
clang::CIRGen::functionLocalUnnamedTagLexicalOrdinal(
    const clang::FunctionDecl *function, const clang::RecordDecl *record) {
  if (!function || !record)
    return std::nullopt;
  return functionLocalUnnamedTagOrdinal(function, record);
}

std::optional<std::string>
clang::CIRGen::recordDeclIdentity(CIRGenModule &cgm, const RecordDecl *decl) {
  llvm::DenseSet<const RecordDecl *> inProgress;
  return recordDeclIdentityImpl(cgm, decl, inProgress);
}

std::optional<std::string>
clang::CIRGen::fieldDeclIdentity(CIRGenModule &cgm, const FieldDecl *decl) {
  if (!decl)
    return std::nullopt;
  mlir::StringAttr ownerAttr = cgm.exactRecordUSRAttr(decl->getParent());
  std::optional<std::string> ownerID =
      ownerAttr ? std::optional<std::string>(ownerAttr.getValue().str())
                : std::nullopt;
  llvm::SmallString<256> memberUSR;
  if (!ownerID.has_value() || ownerID->empty())
    return std::nullopt;
  if (clang::index::generateUSRForDecl(decl, memberUSR) || memberUSR.empty()) {
    return "clang-field-ordinal:" + std::to_string(ownerID->size()) + ":" +
           *ownerID + ":" + std::to_string(decl->getFieldIndex());
  }
  return "clang-field:" + std::to_string(ownerID->size()) + ":" + *ownerID +
         ":" + memberUSR.str().str();
}

CIRRecordLowering::CIRRecordLowering(CIRGenTypes &cirGenTypes,
                                     const RecordDecl *recordDecl, bool packed)
    : cirGenTypes{cirGenTypes}, builder{cirGenTypes.getBuilder()},
      astContext{cirGenTypes.getASTContext()}, recordDecl{recordDecl},
      cxxRecordDecl{llvm::dyn_cast<CXXRecordDecl>(recordDecl)},
      astRecordLayout{
          cirGenTypes.getASTContext().getASTRecordLayout(recordDecl)},
      dataLayout{cirGenTypes.getCGModule().getModule()},
      zeroInitializable{true}, zeroInitializableAsBase{true}, packed{packed},
      padded{false} {}

void CIRRecordLowering::setBitFieldInfo(const FieldDecl *fd,
                                        CharUnits startOffset,
                                        mlir::Type storageType) {
  CIRGenBitFieldInfo &info = bitFields[fd->getCanonicalDecl()];
  info.isSigned = fd->getType()->isSignedIntegerOrEnumerationType();
  info.offset =
      (unsigned)(getFieldBitOffset(fd) - astContext.toBits(startOffset));
  info.size = fd->getBitWidthValue();
  info.storageSize = getSizeInBits(storageType).getQuantity();
  info.storageOffset = startOffset;
  info.storageType = storageType;
  info.name = fd->getName();

  if (info.size > info.storageSize)
    info.size = info.storageSize;
  // Reverse the bit offsets for big endian machines. Since bitfields are laid
  // out as packed bits within an integer-sized unit, we can imagine the bits
  // counting from the most-significant-bit instead of the
  // least-significant-bit.
  if (dataLayout.isBigEndian())
    info.offset = info.storageSize - (info.offset + info.size);

  info.volatileStorageSize = 0;
  info.volatileOffset = 0;
  info.volatileStorageOffset = CharUnits::Zero();
}

void CIRRecordLowering::lower(bool nonVirtualBaseType) {
  if (recordDecl->isUnion()) {
    lowerUnion();
    computeVolatileBitfields();
    return;
  }

  CharUnits size = nonVirtualBaseType ? astRecordLayout.getNonVirtualSize()
                                      : astRecordLayout.getSize();

  accumulateFields();

  if (cxxRecordDecl) {
    accumulateVPtrs();
    accumulateBases();
    if (members.empty()) {
      appendPaddingBytes(size);
      computeVolatileBitfields();
      return;
    }
    if (!nonVirtualBaseType)
      accumulateVBases();
  }

  llvm::stable_sort(members);
  // TODO: Verify bitfield clipping
  assert(!cir::MissingFeatures::checkBitfieldClipping());

  members.push_back(makeStorageInfo(size, getUIntNType(8)));
  determinePacked(nonVirtualBaseType);
  insertPadding();
  members.pop_back();

  calculateZeroInit();
  fillOutputFields();
  computeVolatileBitfields();
}

void CIRRecordLowering::fillOutputFields() {
  for (const MemberInfo &member : members) {
    if (member.data)
      fieldTypes.push_back(member.data);
    if (member.kind == MemberInfo::InfoKind::Field) {
      if (member.fieldDecl)
        fieldIdxMap[member.fieldDecl->getCanonicalDecl()] =
            fieldTypes.size() - 1;
      // A field without storage must be a bitfield.
      if (!member.data) {
        assert(member.fieldDecl &&
               "member.data is a nullptr so member.fieldDecl should not be");
        setBitFieldInfo(member.fieldDecl, member.offset, fieldTypes.back());
      }
    } else if (member.kind == MemberInfo::InfoKind::Base) {
      nonVirtualBases[member.cxxRecordDecl] = fieldTypes.size() - 1;
    } else if (member.kind == MemberInfo::InfoKind::VBase) {
      virtualBases[member.cxxRecordDecl] = fieldTypes.size() - 1;
    }
  }
}

RecordDecl::field_iterator
CIRRecordLowering::accumulateBitFields(RecordDecl::field_iterator field,
                                       RecordDecl::field_iterator fieldEnd) {
  if (isDiscreteBitFieldABI()) {
    // run stores the first element of the current run of bitfields. fieldEnd is
    // used as a special value to note that we don't have a current run. A
    // bitfield run is a contiguous collection of bitfields that can be stored
    // in the same storage block. Zero-sized bitfields and bitfields that would
    // cross an alignment boundary break a run and start a new one.
    RecordDecl::field_iterator run = fieldEnd;
    // tail is the offset of the first bit off the end of the current run. It's
    // used to determine if the ASTRecordLayout is treating these two bitfields
    // as contiguous. StartBitOffset is offset of the beginning of the Run.
    uint64_t startBitOffset, tail = 0;
    for (; field != fieldEnd && field->isBitField(); ++field) {
      // Zero-width bitfields end runs.
      if (field->isZeroLengthBitField()) {
        run = fieldEnd;
        continue;
      }
      uint64_t bitOffset = getFieldBitOffset(*field);
      mlir::Type type = cirGenTypes.convertTypeForMem(field->getType());
      // If we don't have a run yet, or don't live within the previous run's
      // allocated storage then we allocate some storage and start a new run.
      if (run == fieldEnd || bitOffset >= tail) {
        run = field;
        startBitOffset = bitOffset;
        tail = startBitOffset + dataLayout.getTypeAllocSizeInBits(type);
        // Add the storage member to the record.  This must be added to the
        // record before the bitfield members so that it gets laid out before
        // the bitfields it contains get laid out.
        members.push_back(
            makeStorageInfo(bitsToCharUnits(startBitOffset), type));
      }
      // Bitfields get the offset of their storage but come afterward and remain
      // there after a stable sort.
      members.push_back(MemberInfo(bitsToCharUnits(startBitOffset),
                                   MemberInfo::InfoKind::Field, nullptr,
                                   *field));
    }
    return field;
  }

  CharUnits regSize =
      bitsToCharUnits(astContext.getTargetInfo().getRegisterWidth());
  unsigned charBits = astContext.getCharWidth();

  // Data about the start of the span we're accumulating to create an access
  // unit from. 'Begin' is the first bitfield of the span. If 'begin' is
  // 'fieldEnd', we've not got a current span. The span starts at the
  // 'beginOffset' character boundary. 'bitSizeSinceBegin' is the size (in bits)
  // of the span -- this might include padding when we've advanced to a
  // subsequent bitfield run.
  RecordDecl::field_iterator begin = fieldEnd;
  CharUnits beginOffset;
  uint64_t bitSizeSinceBegin;

  // The (non-inclusive) end of the largest acceptable access unit we've found
  // since 'begin'. If this is 'begin', we're gathering the initial set of
  // bitfields of a new span. 'bestEndOffset' is the end of that acceptable
  // access unit -- it might extend beyond the last character of the bitfield
  // run, using available padding characters.
  RecordDecl::field_iterator bestEnd = begin;
  CharUnits bestEndOffset;
  bool bestClipped; // Whether the representation must be in a byte array.

  for (;;) {
    // atAlignedBoundary is true if 'field' is the (potential) start of a new
    // span (or the end of the bitfields). When true, limitOffset is the
    // character offset of that span and barrier indicates whether the new
    // span cannot be merged into the current one.
    bool atAlignedBoundary = false;
    bool barrier = false; // a barrier can be a zero Bit Width or non bit member
    if (field != fieldEnd && field->isBitField()) {
      uint64_t bitOffset = getFieldBitOffset(*field);
      if (begin == fieldEnd) {
        // Beginning a new span.
        begin = field;
        bestEnd = begin;

        assert((bitOffset % charBits) == 0 && "Not at start of char");
        beginOffset = bitsToCharUnits(bitOffset);
        bitSizeSinceBegin = 0;
      } else if ((bitOffset % charBits) != 0) {
        // Bitfield occupies the same character as previous bitfield, it must be
        // part of the same span. This can include zero-length bitfields, should
        // the target not align them to character boundaries. Such non-alignment
        // is at variance with the standards, which require zero-length
        // bitfields be a barrier between access units. But of course we can't
        // achieve that in the middle of a character.
        assert(bitOffset ==
                   astContext.toBits(beginOffset) + bitSizeSinceBegin &&
               "Concatenating non-contiguous bitfields");
      } else {
        // Bitfield potentially begins a new span. This includes zero-length
        // bitfields on non-aligning targets that lie at character boundaries
        // (those are barriers to merging).
        if (field->isZeroLengthBitField())
          barrier = true;
        atAlignedBoundary = true;
      }
    } else {
      // We've reached the end of the bitfield run. Either we're done, or this
      // is a barrier for the current span.
      if (begin == fieldEnd)
        break;

      barrier = true;
      atAlignedBoundary = true;
    }

    // 'installBest' indicates whether we should create an access unit for the
    // current best span: fields ['begin', 'bestEnd') occupying characters
    // ['beginOffset', 'bestEndOffset').
    bool installBest = false;
    if (atAlignedBoundary) {
      // 'field' is the start of a new span or the end of the bitfields. The
      // just-seen span now extends to 'bitSizeSinceBegin'.

      // Determine if we can accumulate that just-seen span into the current
      // accumulation.
      CharUnits accessSize = bitsToCharUnits(bitSizeSinceBegin + charBits - 1);
      if (bestEnd == begin) {
        // This is the initial run at the start of a new span. By definition,
        // this is the best seen so far.
        bestEnd = field;
        bestEndOffset = beginOffset + accessSize;
        // Assume clipped until proven not below.
        bestClipped = true;
        if (!bitSizeSinceBegin)
          // A zero-sized initial span -- this will install nothing and reset
          // for another.
          installBest = true;
      } else if (accessSize > regSize) {
        // Accumulating the just-seen span would create a multi-register access
        // unit, which would increase register pressure.
        installBest = true;
      }

      if (!installBest) {
        // Determine if accumulating the just-seen span will create an expensive
        // access unit or not.
        mlir::Type type = getUIntNType(astContext.toBits(accessSize));
        if (!astContext.getTargetInfo().hasCheapUnalignedBitFieldAccess())
          cirGenTypes.getCGModule().errorNYI(
              field->getSourceRange(), "NYI CheapUnalignedBitFieldAccess");

        if (!installBest) {
          // Find the next used storage offset to determine what the limit of
          // the current span is. That's either the offset of the next field
          // with storage (which might be field itself) or the end of the
          // non-reusable tail padding.
          CharUnits limitOffset;
          for (auto probe = field; probe != fieldEnd; ++probe)
            if (!isEmptyFieldForLayout(astContext, *probe)) {
              // A member with storage sets the limit.
              assert((getFieldBitOffset(*probe) % charBits) == 0 &&
                     "Next storage is not byte-aligned");
              limitOffset = bitsToCharUnits(getFieldBitOffset(*probe));
              goto FoundLimit;
            }
          limitOffset = cxxRecordDecl ? astRecordLayout.getNonVirtualSize()
                                      : astRecordLayout.getDataSize();

        FoundLimit:
          CharUnits typeSize = getSize(type);
          if (beginOffset + typeSize <= limitOffset) {
            // There is space before limitOffset to create a naturally-sized
            // access unit.
            bestEndOffset = beginOffset + typeSize;
            bestEnd = field;
            bestClipped = false;
          }
          if (barrier) {
            // The next field is a barrier that we cannot merge across.
            installBest = true;
          } else if (cirGenTypes.getCGModule()
                         .getCodeGenOpts()
                         .FineGrainedBitfieldAccesses) {
            installBest = true;
          } else {
            // Otherwise, we're not installing. Update the bit size
            // of the current span to go all the way to limitOffset, which is
            // the (aligned) offset of next bitfield to consider.
            bitSizeSinceBegin = astContext.toBits(limitOffset - beginOffset);
          }
        }
      }
    }

    if (installBest) {
      assert((field == fieldEnd || !field->isBitField() ||
              (getFieldBitOffset(*field) % charBits) == 0) &&
             "Installing but not at an aligned bitfield or limit");
      CharUnits accessSize = bestEndOffset - beginOffset;
      if (!accessSize.isZero()) {
        // Add the storage member for the access unit to the record. The
        // bitfields get the offset of their storage but come afterward and
        // remain there after a stable sort.
        mlir::Type type;
        if (bestClipped) {
          assert(getSize(getUIntNType(astContext.toBits(accessSize))) >
                     accessSize &&
                 "Clipped access need not be clipped");
          type = getByteArrayType(accessSize);
        } else {
          type = getUIntNType(astContext.toBits(accessSize));
          assert(getSize(type) == accessSize &&
                 "Unclipped access must be clipped");
        }
        members.push_back(makeStorageInfo(beginOffset, type));
        for (; begin != bestEnd; ++begin)
          if (!begin->isZeroLengthBitField())
            members.push_back(MemberInfo(
                beginOffset, MemberInfo::InfoKind::Field, nullptr, *begin));
      }
      // Reset to start a new span.
      field = bestEnd;
      begin = fieldEnd;
    } else {
      assert(field != fieldEnd && field->isBitField() &&
             "Accumulating past end of bitfields");
      assert(!barrier && "Accumulating across barrier");
      // Accumulate this bitfield into the current (potential) span.
      bitSizeSinceBegin += field->getBitWidthValue();
      ++field;
    }
  }

  return field;
}

void CIRRecordLowering::accumulateFields() {
  for (RecordDecl::field_iterator field = recordDecl->field_begin(),
                                  fieldEnd = recordDecl->field_end();
       field != fieldEnd;) {
    if (field->isBitField()) {
      field = accumulateBitFields(field, fieldEnd);
      assert((field == fieldEnd || !field->isBitField()) &&
             "Failed to accumulate all the bitfields");
    } else if (isEmptyFieldForLayout(astContext, *field)) {
      // TODO(cir): do we want to do anything special about zero size members?
      assert(!cir::MissingFeatures::zeroSizeRecordMembers());
      ++field;
    } else {
      // Use base subobject layout for potentially-overlapping fields,
      // as it is done in RecordLayoutBuilder.
      members.push_back(MemberInfo(
          bitsToCharUnits(getFieldBitOffset(*field)),
          MemberInfo::InfoKind::Field,
          field->isPotentiallyOverlapping()
              ? getStorageType(field->getType()->getAsCXXRecordDecl())
              : getStorageType(*field),
          *field));
      ++field;
    }
  }
}

void CIRRecordLowering::calculateZeroInit() {
  for (const MemberInfo &member : members) {
    if (member.kind == MemberInfo::InfoKind::Field) {
      if (!member.fieldDecl || isZeroInitializable(member.fieldDecl))
        continue;
      zeroInitializable = zeroInitializableAsBase = false;
      return;
    } else if (member.kind == MemberInfo::InfoKind::Base ||
               member.kind == MemberInfo::InfoKind::VBase) {
      if (isZeroInitializable(member.cxxRecordDecl))
        continue;
      zeroInitializable = false;
      if (member.kind == MemberInfo::InfoKind::Base)
        zeroInitializableAsBase = false;
    }
  }
}

void CIRRecordLowering::determinePacked(bool nvBaseType) {
  if (packed)
    return;
  CharUnits alignment = CharUnits::One();
  CharUnits nvAlignment = CharUnits::One();
  CharUnits nvSize = !nvBaseType && cxxRecordDecl
                         ? astRecordLayout.getNonVirtualSize()
                         : CharUnits::Zero();

  for (const MemberInfo &member : members) {
    if (!member.data)
      continue;
    // If any member falls at an offset that it not a multiple of its alignment,
    // then the entire record must be packed.
    if (!member.offset.isMultipleOf(getAlignment(member.data)))
      packed = true;
    if (member.offset < nvSize)
      nvAlignment = std::max(nvAlignment, getAlignment(member.data));
    alignment = std::max(alignment, getAlignment(member.data));
  }
  // If the size of the record (the capstone's offset) is not a multiple of the
  // record's alignment, it must be packed.
  if (!members.back().offset.isMultipleOf(alignment))
    packed = true;
  // If the non-virtual sub-object is not a multiple of the non-virtual
  // sub-object's alignment, it must be packed.  We cannot have a packed
  // non-virtual sub-object and an unpacked complete object or vise versa.
  if (!nvSize.isMultipleOf(nvAlignment))
    packed = true;
  // Update the alignment of the sentinel.
  if (!packed)
    members.back().data = getUIntNType(astContext.toBits(alignment));
}

void CIRRecordLowering::insertPadding() {
  std::vector<std::pair<CharUnits, CharUnits>> padding;
  CharUnits size = CharUnits::Zero();
  for (const MemberInfo &member : members) {
    if (!member.data)
      continue;
    CharUnits offset = member.offset;
    assert(offset >= size);
    // Insert padding if we need to.
    if (offset !=
        size.alignTo(packed ? CharUnits::One() : getAlignment(member.data)))
      padding.push_back(std::make_pair(size, offset - size));
    size = offset + getSize(member.data);
  }
  if (padding.empty())
    return;
  padded = true;
  // Add the padding to the Members list and sort it.
  for (const std::pair<CharUnits, CharUnits> &paddingPair : padding)
    members.push_back(makeStorageInfo(paddingPair.first,
                                      getByteArrayType(paddingPair.second)));
  llvm::stable_sort(members);
}

static cir::ArgPassingKind
convertRecordArgPassingKind(RecordArgPassingKind kind) {
  switch (kind) {
  case RecordArgPassingKind::CanPassInRegs:
    return cir::ArgPassingKind::CanPassInRegs;
  case RecordArgPassingKind::CannotPassInRegs:
    return cir::ArgPassingKind::CannotPassInRegs;
  case RecordArgPassingKind::CanNeverPassInRegs:
    return cir::ArgPassingKind::CanNeverPassInRegs;
  }
  llvm_unreachable("unknown RecordArgPassingKind");
}

std::unique_ptr<CIRGenRecordLayout>
CIRGenTypes::computeRecordLayout(const RecordDecl *rd, cir::RecordType *ty) {
  CIRRecordLowering lowering(*this, rd, /*packed=*/false);
  assert(ty->isIncomplete() && "recomputing record layout?");
  lowering.lower(/*nonVirtualBaseType=*/false);

  // If we're in C++, compute the base subobject type. For C++ records the base
  // subobject type is always set (matching classic CodeGen). For unions and
  // final classes the base subobject and complete object types are identical
  // (no tail padding can be reused), so baseTy points at the same record as
  // ty. We must still populate baseTy in those cases because callers such as
  // getStorageType(const CXXRecordDecl *) used to lay out potentially-
  // overlapping ([[no_unique_address]]) fields read it unconditionally; a
  // null baseTy would otherwise propagate as a null mlir::Type into the
  // members vector and trip the !empty() assertion in fillOutputFields.
  cir::RecordType baseTy;
  if (llvm::isa<CXXRecordDecl>(rd)) {
    baseTy = *ty;
    if (!rd->isUnion() && !rd->hasAttr<FinalAttr>() &&
        lowering.astRecordLayout.getNonVirtualSize() !=
            lowering.astRecordLayout.getSize()) {
      CIRRecordLowering baseLowering(*this, rd, /*Packed=*/lowering.packed);
      baseLowering.lower(/*NonVirtualBaseType=*/true);
      std::string baseIdentifier = getRecordTypeName(rd, ".base");
      baseTy = builder.getCompleteNamedRecordType(
          baseLowering.fieldTypes, baseLowering.packed, baseLowering.padded,
          baseIdentifier);
      // TODO(cir): add something like addRecordTypeName

      // BaseTy and Ty must agree on their packedness for getCIRFieldNo to work
      // on both of them with the same index.
      assert(lowering.packed == baseLowering.packed &&
             "Non-virtual and complete types must agree on packedness");
    }
  }

  // Fill in the record *after* computing the base type.  Filling in the body
  // signifies that the type is no longer opaque and record layout is complete,
  // but we may need to recursively layout rd while laying D out as a base type.
  assert(!cir::MissingFeatures::astRecordDeclAttr());
  ty->complete(lowering.fieldTypes, lowering.packed, lowering.padded,
               lowering.unionPadding);

  // Queue ABI metadata for the module-level cir.record_layouts attribute.
  if (ty->getName()) {
    mlir::MLIRContext *mlirCtx = ty->getContext();
    cir::ArgPassingKind apk =
        convertRecordArgPassingKind(rd->getArgPassingRestrictions());

    bool hasTrivialDestructor = true;
    if (auto *cxxRD = dyn_cast<CXXRecordDecl>(rd))
      hasTrivialDestructor = cxxRD->hasTrivialDestructor();
    const auto &astLayout = astContext.getASTRecordLayout(rd);
    uint64_t recordAlignInBytes = astLayout.getAlignment().getQuantity();

    // The record's exact Clang identity is recorded once, when the named CIR
    // record type is created (CIRGenTypes::convertRecordDeclType), so records
    // that only ever appear as incomplete pointees still carry it; only the
    // layout-derived facts are queued here.
    // The projected object schema omits base subobjects only when the base
    // itself projects nothing: no fields anywhere in its base chain and no
    // vptr. A base that is merely empty FOR LAYOUT (its storage collapses to
    // padding because every field has an empty record type) still projects
    // those addressable member fields, and consumers that enumerate the
    // exact AST schema will see them; marking such a record decidably empty
    // would contradict its own exact field-bearing schema.
    auto recordProjectsEmptySchema = [](const RecordDecl *record,
                                        auto &&self) -> bool {
      const RecordDecl *definition = record ? record->getDefinition() : nullptr;
      if (!definition || !definition->field_empty())
        return false;
      const auto *cxx = dyn_cast<CXXRecordDecl>(definition);
      if (!cxx)
        return true;
      if (cxx->isDynamicClass())
        return false;
      for (const CXXBaseSpecifier &base : cxx->bases()) {
        const CXXRecordDecl *baseRecord = base.getType()->getAsCXXRecordDecl();
        if (!baseRecord || !self(baseRecord, self))
          return false;
      }
      return true;
    };
    const bool hasEmptyProjectedSchema =
        recordProjectsEmptySchema(rd, recordProjectsEmptySchema);
    if (hasEmptyProjectedSchema)
      cgm.addEmptyRecordSchema(ty->getName());

    cgm.addRecordLayout(ty->getName(), cir::RecordLayoutAttr::get(
                                           mlirCtx, apk, hasTrivialDestructor,
                                           recordAlignInBytes));
  }

  auto rl = std::make_unique<CIRGenRecordLayout>(
      ty ? *ty : cir::RecordType{}, baseTy ? baseTy : cir::RecordType{},
      (bool)lowering.zeroInitializable, (bool)lowering.zeroInitializableAsBase);

  rl->nonVirtualBases.swap(lowering.nonVirtualBases);
  rl->completeObjectVirtualBases.swap(lowering.virtualBases);

  // Add all the field numbers.
  rl->fieldIdxMap.swap(lowering.fieldIdxMap);

  rl->bitFields.swap(lowering.bitFields);

  // Dump the layout, if requested.
  if (getASTContext().getLangOpts().DumpRecordLayouts) {
    llvm::outs() << "\n*** Dumping CIRgen Record Layout\n";
    llvm::outs() << "Record: ";
    rd->dump(llvm::outs());
    llvm::outs() << "\nLayout: ";
    rl->print(llvm::outs());
  }

  // TODO: implement verification
  return rl;
}

void CIRGenRecordLayout::print(raw_ostream &os) const {
  os << "<CIRecordLayout\n";
  os << "   CIR Type:" << completeObjectType << "\n";
  if (baseSubobjectType)
    os << "   NonVirtualBaseCIRType:" << baseSubobjectType << "\n";
  os << "   IsZeroInitializable:" << zeroInitializable << "\n";
  os << "   BitFields:[\n";
  std::vector<std::pair<unsigned, const CIRGenBitFieldInfo *>> bitInfo;
  for (auto &[decl, info] : bitFields) {
    const RecordDecl *rd = decl->getParent();
    unsigned index = 0;
    for (RecordDecl::field_iterator it = rd->field_begin(); *it != decl; ++it)
      ++index;
    bitInfo.push_back(std::make_pair(index, &info));
  }
  llvm::array_pod_sort(bitInfo.begin(), bitInfo.end());
  for (std::pair<unsigned, const CIRGenBitFieldInfo *> &info : bitInfo) {
    os.indent(4);
    info.second->print(os);
    os << "\n";
  }
  os << "   ]>\n";
}

void CIRGenBitFieldInfo::print(raw_ostream &os) const {
  os << "<CIRBitFieldInfo" << " name:" << name << " offset:" << offset
     << " size:" << size << " isSigned:" << isSigned
     << " storageSize:" << storageSize
     << " storageOffset:" << storageOffset.getQuantity()
     << " volatileOffset:" << volatileOffset
     << " volatileStorageSize:" << volatileStorageSize
     << " volatileStorageOffset:" << volatileStorageOffset.getQuantity() << ">";
}

void CIRGenRecordLayout::dump() const { print(llvm::errs()); }

void CIRGenBitFieldInfo::dump() const { print(llvm::errs()); }

void CIRRecordLowering::lowerUnion() {
  CharUnits layoutSize = astRecordLayout.getSize();
  mlir::Type storageType = nullptr;
  bool seenNamedMember = false;

  // Iterate through the fields setting bitFieldInfo and the Fields array. Also
  // locate the "most appropriate" storage type.
  for (const FieldDecl *field : recordDecl->fields()) {
    mlir::Type fieldType;
    if (field->isBitField()) {
      if (field->isZeroLengthBitField())
        continue;
      fieldType = getBitfieldStorageType(field->getBitWidthValue());
      setBitFieldInfo(field, CharUnits::Zero(), fieldType);
    } else {
      fieldType = getStorageType(field);
    }

    // Record the member's index in the CIR union layout. This can differ from
    // FieldDecl::getFieldIndex() when an earlier field has no CIR storage.
    fieldIdxMap[field->getCanonicalDecl()] = fieldTypes.size();
    // Keep every source member in the CIR union schema even when this field
    // proves that the union is not zero-initializable. That property is
    // independent of both the source-member schema and the physical storage
    // type selected below.
    fieldTypes.push_back(fieldType);

    // Compute zero-initializable status. A pointer-to-data-member can have a
    // nonzero null representation, so a union containing one may not be zero
    // initializable.
    if (!seenNamedMember) {
      seenNamedMember = field->getIdentifier();
      if (!seenNamedMember)
        if (const RecordDecl *fieldRD = field->getType()->getAsRecordDecl())
          seenNamedMember = fieldRD->findFirstNamedDataMember();
      if (seenNamedMember && !isZeroInitializable(field))
        zeroInitializable = zeroInitializableAsBase = false;
    }

    // Conditionally update our storage type if we've got a new "better" one.
    if (!storageType || getAlignment(fieldType) > getAlignment(storageType) ||
        (getAlignment(fieldType) == getAlignment(storageType) &&
         getSize(fieldType) > getSize(storageType)))
      storageType = fieldType;
  }

  if (!storageType) {
    appendPaddingBytes(layoutSize);
    return;
  }

  if (layoutSize < getSize(storageType))
    storageType = getByteArrayType(layoutSize);
  else
    appendPaddingBytes(layoutSize - getSize(storageType));

  // Set packed if we need it.
  if (!layoutSize.isMultipleOf(getAlignment(storageType)))
    packed = true;
}

bool CIRRecordLowering::hasOwnStorage(const CXXRecordDecl *decl,
                                      const CXXRecordDecl *query) {
  const ASTRecordLayout &declLayout = astContext.getASTRecordLayout(decl);
  if (declLayout.isPrimaryBaseVirtual() && declLayout.getPrimaryBase() == query)
    return false;
  for (const auto &base : decl->bases())
    if (!hasOwnStorage(base.getType()->getAsCXXRecordDecl(), query))
      return false;
  return true;
}

/// The AAPCS that defines that, when possible, bit-fields should
/// be accessed using containers of the declared type width:
/// When a volatile bit-field is read, and its container does not overlap with
/// any non-bit-field member or any zero length bit-field member, its container
/// must be read exactly once using the access width appropriate to the type of
/// the container. When a volatile bit-field is written, and its container does
/// not overlap with any non-bit-field member or any zero-length bit-field
/// member, its container must be read exactly once and written exactly once
/// using the access width appropriate to the type of the container. The two
/// accesses are not atomic.
///
/// Enforcing the width restriction can be disabled using
/// -fno-aapcs-bitfield-width.
void CIRRecordLowering::computeVolatileBitfields() {
  if (!isAAPCS() ||
      !cirGenTypes.getCGModule().getCodeGenOpts().AAPCSBitfieldWidth)
    return;

  for (auto &[field, info] : bitFields) {
    mlir::Type resLTy = cirGenTypes.convertTypeForMem(field->getType());

    if (astContext.toBits(astRecordLayout.getAlignment()) <
        getSizeInBits(resLTy).getQuantity())
      continue;

    // CIRRecordLowering::setBitFieldInfo() pre-adjusts the bit-field offsets
    // for big-endian targets, but it assumes a container of width
    // info.storageSize. Since AAPCS uses a different container size (width
    // of the type), we first undo that calculation here and redo it once
    // the bit-field offset within the new container is calculated.
    const unsigned oldOffset =
        isBigEndian() ? info.storageSize - (info.offset + info.size)
                      : info.offset;
    // Offset to the bit-field from the beginning of the struct.
    const unsigned absoluteOffset =
        astContext.toBits(info.storageOffset) + oldOffset;

    // Container size is the width of the bit-field type.
    const unsigned storageSize = getSizeInBits(resLTy).getQuantity();
    // Nothing to do if the access uses the desired
    // container width and is naturally aligned.
    if (info.storageSize == storageSize && (oldOffset % storageSize == 0))
      continue;

    // Offset within the container.
    unsigned offset = absoluteOffset & (storageSize - 1);
    // Bail out if an aligned load of the container cannot cover the entire
    // bit-field. This can happen for example, if the bit-field is part of a
    // packed struct. AAPCS does not define access rules for such cases, we let
    // clang to follow its own rules.
    if (offset + info.size > storageSize)
      continue;

    // Re-adjust offsets for big-endian targets.
    if (isBigEndian())
      offset = storageSize - (offset + info.size);

    const CharUnits storageOffset =
        astContext.toCharUnitsFromBits(absoluteOffset & ~(storageSize - 1));
    const CharUnits end = storageOffset +
                          astContext.toCharUnitsFromBits(storageSize) -
                          CharUnits::One();

    const ASTRecordLayout &layout =
        astContext.getASTRecordLayout(field->getParent());
    // If we access outside memory outside the record, than bail out.
    const CharUnits recordSize = layout.getSize();
    if (end >= recordSize)
      continue;

    // Bail out if performing this load would access non-bit-fields members.
    bool conflict = false;
    for (const auto *f : recordDecl->fields()) {
      // Allow sized bit-fields overlaps.
      if (f->isBitField() && !f->isZeroLengthBitField())
        continue;

      const CharUnits fOffset = astContext.toCharUnitsFromBits(
          layout.getFieldOffset(f->getFieldIndex()));

      // As C11 defines, a zero sized bit-field defines a barrier, so
      // fields after and before it should be race condition free.
      // The AAPCS acknowledges it and imposes no restritions when the
      // natural container overlaps a zero-length bit-field.
      if (f->isZeroLengthBitField()) {
        if (end > fOffset && storageOffset < fOffset) {
          conflict = true;
          break;
        }
      }

      const CharUnits fEnd =
          fOffset +
          astContext.toCharUnitsFromBits(
              getSizeInBits(cirGenTypes.convertTypeForMem(f->getType()))
                  .getQuantity()) -
          CharUnits::One();
      // If no overlap, continue.
      if (end < fOffset || fEnd < storageOffset)
        continue;

      // The desired load overlaps a non-bit-field member, bail out.
      conflict = true;
      break;
    }

    if (conflict)
      continue;
    // Write the new bit-field access parameters.
    // As the storage offset now is defined as the number of elements from the
    // start of the structure, we should divide the Offset by the element size.
    info.volatileStorageOffset =
        storageOffset /
        astContext.toCharUnitsFromBits(storageSize).getQuantity();
    info.volatileStorageSize = storageSize;
    info.volatileOffset = offset;
  }
}

void CIRRecordLowering::accumulateBases() {
  // If we've got a primary virtual base, we need to add it with the bases.
  if (astRecordLayout.isPrimaryBaseVirtual()) {
    const CXXRecordDecl *baseDecl = astRecordLayout.getPrimaryBase();
    members.push_back(MemberInfo(CharUnits::Zero(), MemberInfo::InfoKind::Base,
                                 getStorageType(baseDecl), baseDecl));
  }

  // Accumulate the non-virtual bases.
  for (const auto &base : cxxRecordDecl->bases()) {
    if (base.isVirtual())
      continue;
    // Bases can be zero-sized even if not technically empty if they
    // contain only a trailing array member.
    const CXXRecordDecl *baseDecl = base.getType()->getAsCXXRecordDecl();
    if (!baseDecl->isEmpty() &&
        !astContext.getASTRecordLayout(baseDecl).getNonVirtualSize().isZero()) {
      members.push_back(MemberInfo(astRecordLayout.getBaseClassOffset(baseDecl),
                                   MemberInfo::InfoKind::Base,
                                   getStorageType(baseDecl), baseDecl));
    }
  }
}

void CIRRecordLowering::accumulateVBases() {
  for (const auto &base : cxxRecordDecl->vbases()) {
    const CXXRecordDecl *baseDecl = base.getType()->getAsCXXRecordDecl();
    if (isEmptyRecordForLayout(astContext, base.getType()))
      continue;
    CharUnits offset = astRecordLayout.getVBaseClassOffset(baseDecl);
    // If the vbase is a primary virtual base of some base, then it doesn't
    // get its own storage location but instead lives inside of that base.
    if (isOverlappingVBaseABI() && astContext.isNearlyEmpty(baseDecl) &&
        !hasOwnStorage(cxxRecordDecl, baseDecl)) {
      members.push_back(
          MemberInfo(offset, MemberInfo::InfoKind::VBase, nullptr, baseDecl));
      continue;
    }
    // If we've got a vtordisp, add it as a storage type.
    if (astRecordLayout.getVBaseOffsetsMap()
            .find(baseDecl)
            ->second.hasVtorDisp())
      members.push_back(makeStorageInfo(offset - CharUnits::fromQuantity(4),
                                        getUIntNType(32)));
    members.push_back(MemberInfo(offset, MemberInfo::InfoKind::VBase,
                                 getStorageType(baseDecl), baseDecl));
  }
}

void CIRRecordLowering::accumulateVPtrs() {
  if (astRecordLayout.hasOwnVFPtr())
    members.push_back(MemberInfo(CharUnits::Zero(), MemberInfo::InfoKind::VFPtr,
                                 getVFPtrType()));

  if (astRecordLayout.hasOwnVBPtr())
    cirGenTypes.getCGModule().errorNYI(recordDecl->getSourceRange(),
                                       "accumulateVPtrs: hasOwnVBPtr");
}

mlir::Type CIRRecordLowering::getVFPtrType() {
  return cir::VPtrType::get(builder.getContext());
}
