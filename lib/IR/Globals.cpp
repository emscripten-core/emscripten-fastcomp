//===-- Globals.cpp - Implement the GlobalValue & GlobalVariable class ----===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the GlobalValue & GlobalVariable classes for the IR
// library.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/GlobalValue.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/GlobalAlias.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/LeakDetector.h"
using namespace llvm;

//===----------------------------------------------------------------------===//
//                            GlobalValue Class
//===----------------------------------------------------------------------===//

bool GlobalValue::isMaterializable() const {
  return getParent() && getParent()->isMaterializable(this);
}
bool GlobalValue::isDematerializable() const {
  return getParent() && getParent()->isDematerializable(this);
}
bool GlobalValue::Materialize(std::string *ErrInfo) {
  return getParent()->Materialize(this, ErrInfo);
}
void GlobalValue::Dematerialize() {
  getParent()->Dematerialize(this);
}

/// Override destroyConstant to make sure it doesn't get called on
/// GlobalValue's because they shouldn't be treated like other constants.
void GlobalValue::destroyConstant() {
  llvm_unreachable("You can't GV->destroyConstant()!");
}

// @LOCALMOD-BEGIN

// Extract the version information from GV.
static void ExtractVersion(const GlobalValue *GV,
                           StringRef *Name,
                           StringRef *Ver,
                           bool *IsDefault) {
  // The version information is stored in the GlobalValue's name, e.g.:
  //
  //     GV Name      Name  Ver  IsDefault
  //    ------------------------------------
  //     foo@@V1 -->  foo   V1     true
  //     bar@V2  -->  bar   V2     false
  //     baz     -->  baz          false

  StringRef GVName = GV->getName();
  size_t atpos = GVName.find("@");
  if (atpos == StringRef::npos) {
    *Name = GVName;
    *Ver = "";
    *IsDefault = false;
    return;
  }
  *Name = GVName.substr(0, atpos);
  ++atpos;
  if (atpos < GVName.size() && GVName[atpos] == '@') {
    *IsDefault = true;
    ++atpos;
  } else {
    *IsDefault = false;
  }
  *Ver = GVName.substr(atpos);
}

// Set the version information on GV.
static void SetVersion(Module *M,
                       GlobalValue *GV,
                       StringRef Ver,
                       bool IsDefault) {
  StringRef Name;
  StringRef PrevVersion;
  bool PrevIsDefault;
  ExtractVersion(GV, &Name, &PrevVersion, &PrevIsDefault);

  // If this symbol already has a version, make sure it matches.
  if (!PrevVersion.empty()) {
    if (!PrevVersion.equals(Ver) || PrevIsDefault != IsDefault) {
      llvm_unreachable("Trying to override symbol version info!");
    }
    return;
  }
  // If there's no version to set, there's nothing to do.
  if (Ver.empty())
    return;

  // Make sure the versioned symbol name doesn't already exist.
  std::string NewName = Name.str() + (IsDefault ? "@@" : "@") + Ver.str();
  if (M->getNamedValue(NewName)) {
    // It may make sense to do this as long as one of the globals being
    // merged is only a declaration. But since this situation seems to be
    // a corner case, for now it is unimplemented.
    llvm_unreachable("Merging unversioned global into "
                     "existing versioned global is unimplemented");
  }
  GV->setName(NewName);
}

StringRef GlobalValue::getUnversionedName() const {
  StringRef Name;
  StringRef Ver;
  bool IsDefaultVersion;
  ExtractVersion(this, &Name, &Ver, &IsDefaultVersion);
  return Name;
}

StringRef GlobalValue::getVersion() const {
  StringRef Name;
  StringRef Ver;
  bool IsDefaultVersion;
  ExtractVersion(this, &Name, &Ver, &IsDefaultVersion);
  return Ver;
}

bool GlobalValue::isDefaultVersion() const {
  StringRef Name;
  StringRef Ver;
  bool IsDefaultVersion;
  ExtractVersion(this, &Name, &Ver, &IsDefaultVersion);
  // It is an error to call this function on an unversioned symbol.
  assert(!Ver.empty());
  return IsDefaultVersion;
}

void GlobalValue::setVersionDef(StringRef Version, bool IsDefault) {
  // This call only makes sense for definitions.
  assert(!isDeclaration());
  SetVersion(Parent, this, Version, IsDefault);
}

void GlobalValue::setNeeded(StringRef Version, StringRef DynFile) {
  // This call makes sense on declarations or
  // available-externally definitions.
  // TODO(pdox): If this is a definition, should we turn it
  //             into a declaration here?
  assert(isDeclaration() || hasAvailableExternallyLinkage());
  SetVersion(Parent, this, Version, false);
  Parent->addNeededRecord(DynFile, this);
}
// @LOCALMOD-END

/// copyAttributesFrom - copy all additional attributes (those not needed to
/// create a GlobalValue) from the GlobalValue Src to this one.
void GlobalValue::copyAttributesFrom(const GlobalValue *Src) {
  setAlignment(Src->getAlignment());
  setSection(Src->getSection());
  setVisibility(Src->getVisibility());
  setUnnamedAddr(Src->hasUnnamedAddr());
}

void GlobalValue::setAlignment(unsigned Align) {
  assert((Align & (Align-1)) == 0 && "Alignment is not a power of 2!");
  assert(Align <= MaximumAlignment &&
         "Alignment is greater than MaximumAlignment!");
  Alignment = Log2_32(Align) + 1;
  assert(getAlignment() == Align && "Alignment representation error!");
}

bool GlobalValue::isDeclaration() const {
  // Globals are definitions if they have an initializer.
  if (const GlobalVariable *GV = dyn_cast<GlobalVariable>(this))
    return GV->getNumOperands() == 0;

  // Functions are definitions if they have a body.
  if (const Function *F = dyn_cast<Function>(this))
    return F->empty();

  // Aliases are always definitions.
  assert(isa<GlobalAlias>(this));
  return false;
}
  
//===----------------------------------------------------------------------===//
// GlobalVariable Implementation
//===----------------------------------------------------------------------===//

GlobalVariable::GlobalVariable(Type *Ty, bool constant, LinkageTypes Link,
                               Constant *InitVal,
                               const Twine &Name, ThreadLocalMode TLMode,
                               unsigned AddressSpace,
                               bool isExternallyInitialized)
  : GlobalValue(PointerType::get(Ty, AddressSpace),
                Value::GlobalVariableVal,
                OperandTraits<GlobalVariable>::op_begin(this),
                InitVal != 0, Link, Name),
    isConstantGlobal(constant), threadLocalMode(TLMode),
    isExternallyInitializedConstant(isExternallyInitialized) {
  if (InitVal) {
    assert(InitVal->getType() == Ty &&
           "Initializer should be the same type as the GlobalVariable!");
    Op<0>() = InitVal;
  }

  LeakDetector::addGarbageObject(this);
}

GlobalVariable::GlobalVariable(Module &M, Type *Ty, bool constant,
                               LinkageTypes Link, Constant *InitVal,
                               const Twine &Name,
                               GlobalVariable *Before, ThreadLocalMode TLMode,
                               unsigned AddressSpace,
                               bool isExternallyInitialized)
  : GlobalValue(PointerType::get(Ty, AddressSpace),
                Value::GlobalVariableVal,
                OperandTraits<GlobalVariable>::op_begin(this),
                InitVal != 0, Link, Name),
    isConstantGlobal(constant), threadLocalMode(TLMode),
    isExternallyInitializedConstant(isExternallyInitialized) {
  if (InitVal) {
    assert(InitVal->getType() == Ty &&
           "Initializer should be the same type as the GlobalVariable!");
    Op<0>() = InitVal;
  }
  
  LeakDetector::addGarbageObject(this);
  
  if (Before)
    Before->getParent()->getGlobalList().insert(Before, this);
  else
    M.getGlobalList().push_back(this);
}

void GlobalVariable::setParent(Module *parent) {
  if (getParent())
    LeakDetector::addGarbageObject(this);
  Parent = parent;
  if (getParent())
    LeakDetector::removeGarbageObject(this);
}

void GlobalVariable::removeFromParent() {
  getParent()->getGlobalList().remove(this);
}

void GlobalVariable::eraseFromParent() {
  getParent()->getGlobalList().erase(this);
}

void GlobalVariable::replaceUsesOfWithOnConstant(Value *From, Value *To,
                                                 Use *U) {
  // If you call this, then you better know this GVar has a constant
  // initializer worth replacing. Enforce that here.
  assert(getNumOperands() == 1 &&
         "Attempt to replace uses of Constants on a GVar with no initializer");

  // And, since you know it has an initializer, the From value better be
  // the initializer :)
  assert(getOperand(0) == From &&
         "Attempt to replace wrong constant initializer in GVar");

  // And, you better have a constant for the replacement value
  assert(isa<Constant>(To) &&
         "Attempt to replace GVar initializer with non-constant");

  // Okay, preconditions out of the way, replace the constant initializer.
  this->setOperand(0, cast<Constant>(To));
}

void GlobalVariable::setInitializer(Constant *InitVal) {
  if (InitVal == 0) {
    if (hasInitializer()) {
      Op<0>().set(0);
      NumOperands = 0;
    }
  } else {
    assert(InitVal->getType() == getType()->getElementType() &&
           "Initializer type must match GlobalVariable type");
    if (!hasInitializer())
      NumOperands = 1;
    Op<0>().set(InitVal);
  }
}

/// copyAttributesFrom - copy all additional attributes (those not needed to
/// create a GlobalVariable) from the GlobalVariable Src to this one.
void GlobalVariable::copyAttributesFrom(const GlobalValue *Src) {
  assert(isa<GlobalVariable>(Src) && "Expected a GlobalVariable!");
  GlobalValue::copyAttributesFrom(Src);
  const GlobalVariable *SrcVar = cast<GlobalVariable>(Src);
  setThreadLocal(SrcVar->isThreadLocal());
}


//===----------------------------------------------------------------------===//
// GlobalAlias Implementation
//===----------------------------------------------------------------------===//

GlobalAlias::GlobalAlias(Type *Ty, LinkageTypes Link,
                         const Twine &Name, Constant* aliasee,
                         Module *ParentModule)
  : GlobalValue(Ty, Value::GlobalAliasVal, &Op<0>(), 1, Link, Name) {
  LeakDetector::addGarbageObject(this);

  if (aliasee)
    assert(aliasee->getType() == Ty && "Alias and aliasee types should match!");
  Op<0>() = aliasee;

  if (ParentModule)
    ParentModule->getAliasList().push_back(this);
}

void GlobalAlias::setParent(Module *parent) {
  if (getParent())
    LeakDetector::addGarbageObject(this);
  Parent = parent;
  if (getParent())
    LeakDetector::removeGarbageObject(this);
}

void GlobalAlias::removeFromParent() {
  getParent()->getAliasList().remove(this);
}

void GlobalAlias::eraseFromParent() {
  getParent()->getAliasList().erase(this);
}

void GlobalAlias::setAliasee(Constant *Aliasee) {
  assert((!Aliasee || Aliasee->getType() == getType()) &&
         "Alias and aliasee types should match!");
  
  setOperand(0, Aliasee);
}

const GlobalValue *GlobalAlias::getAliasedGlobal() const {
  const Constant *C = getAliasee();
  if (C == 0) return 0;
  
  if (const GlobalValue *GV = dyn_cast<GlobalValue>(C))
    return GV;

  const ConstantExpr *CE = cast<ConstantExpr>(C);
  assert((CE->getOpcode() == Instruction::BitCast || 
          CE->getOpcode() == Instruction::GetElementPtr) &&
         "Unsupported aliasee");
  
  return cast<GlobalValue>(CE->getOperand(0));
}

const GlobalValue *GlobalAlias::resolveAliasedGlobal(bool stopOnWeak) const {
  SmallPtrSet<const GlobalValue*, 3> Visited;

  // Check if we need to stop early.
  if (stopOnWeak && mayBeOverridden())
    return this;

  const GlobalValue *GV = getAliasedGlobal();
  Visited.insert(GV);

  // Iterate over aliasing chain, stopping on weak alias if necessary.
  while (const GlobalAlias *GA = dyn_cast<GlobalAlias>(GV)) {
    if (stopOnWeak && GA->mayBeOverridden())
      break;

    GV = GA->getAliasedGlobal();

    if (!Visited.insert(GV))
      return 0;
  }

  return GV;
}
