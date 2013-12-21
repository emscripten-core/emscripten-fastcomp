//===-- Module.cpp - Implement the Module class ---------------------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// This file implements the Module class for the IR library.
//
//===----------------------------------------------------------------------===//

#include "llvm/IR/Module.h"
#include "SymbolTableListTraitsImpl.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/GVMaterializer.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/LeakDetector.h"
#include "llvm/Support/ErrorHandling.h" // @LOCALMOD
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <cstdarg>
#include <cstdlib>
using namespace llvm;

//===----------------------------------------------------------------------===//
// Methods to implement the globals and functions lists.
//

// Explicit instantiations of SymbolTableListTraits since some of the methods
// are not in the public header file.
template class llvm::SymbolTableListTraits<Function, Module>;
template class llvm::SymbolTableListTraits<GlobalVariable, Module>;
template class llvm::SymbolTableListTraits<GlobalAlias, Module>;

//===----------------------------------------------------------------------===//
// Primitive Module methods.
//

Module::Module(StringRef MID, LLVMContext& C)
  : Context(C), Materializer(NULL), ModuleID(MID) {
  ValSymTab = new ValueSymbolTable();
  NamedMDSymTab = new StringMap<NamedMDNode *>();
  Context.addModule(this);
}

Module::~Module() {
  Context.removeModule(this);
  dropAllReferences();
  GlobalList.clear();
  FunctionList.clear();
  AliasList.clear();
  LibraryList.clear(); // @LOCALMOD
  NamedMDList.clear();
  delete ValSymTab;
  delete static_cast<StringMap<NamedMDNode *> *>(NamedMDSymTab);
}

/// Target endian information.
Module::Endianness Module::getEndianness() const {
  StringRef temp = DataLayout;
  Module::Endianness ret = AnyEndianness;

  while (!temp.empty()) {
    std::pair<StringRef, StringRef> P = getToken(temp, "-");

    StringRef token = P.first;
    temp = P.second;

    if (token[0] == 'e') {
      ret = LittleEndian;
    } else if (token[0] == 'E') {
      ret = BigEndian;
    }
  }

  return ret;
}

/// Target Pointer Size information.
Module::PointerSize Module::getPointerSize() const {
  StringRef temp = DataLayout;
  Module::PointerSize ret = AnyPointerSize;

  while (!temp.empty()) {
    std::pair<StringRef, StringRef> TmpP = getToken(temp, "-");
    temp = TmpP.second;
    TmpP = getToken(TmpP.first, ":");
    StringRef token = TmpP.second, signalToken = TmpP.first;

    if (signalToken[0] == 'p') {
      int size = 0;
      getToken(token, ":").first.getAsInteger(10, size);
      if (size == 32)
        ret = Pointer32;
      else if (size == 64)
        ret = Pointer64;
    }
  }

  return ret;
}

/// getNamedValue - Return the first global value in the module with
/// the specified name, of arbitrary type.  This method returns null
/// if a global with the specified name is not found.
GlobalValue *Module::getNamedValue(StringRef Name) const {
  return cast_or_null<GlobalValue>(getValueSymbolTable().lookup(Name));
}

/// getMDKindID - Return a unique non-zero ID for the specified metadata kind.
/// This ID is uniqued across modules in the current LLVMContext.
unsigned Module::getMDKindID(StringRef Name) const {
  return Context.getMDKindID(Name);
}

/// getMDKindNames - Populate client supplied SmallVector with the name for
/// custom metadata IDs registered in this LLVMContext.   ID #0 is not used,
/// so it is filled in as an empty string.
void Module::getMDKindNames(SmallVectorImpl<StringRef> &Result) const {
  return Context.getMDKindNames(Result);
}


//===----------------------------------------------------------------------===//
// Methods for easy access to the functions in the module.
//

// getOrInsertFunction - Look up the specified function in the module symbol
// table.  If it does not exist, add a prototype for the function and return
// it.  This is nice because it allows most passes to get away with not handling
// the symbol table directly for this common task.
//
Constant *Module::getOrInsertFunction(StringRef Name,
                                      FunctionType *Ty,
                                      AttributeSet AttributeList) {
  // See if we have a definition for the specified function already.
  GlobalValue *F = getNamedValue(Name);
  if (F == 0) {
    // Nope, add it
    Function *New = Function::Create(Ty, GlobalVariable::ExternalLinkage, Name);
    if (!New->isIntrinsic())       // Intrinsics get attrs set on construction
      New->setAttributes(AttributeList);
    FunctionList.push_back(New);
    return New;                    // Return the new prototype.
  }

  // Okay, the function exists.  Does it have externally visible linkage?
  if (F->hasLocalLinkage()) {
    // Clear the function's name.
    F->setName("");
    // Retry, now there won't be a conflict.
    Constant *NewF = getOrInsertFunction(Name, Ty);
    F->setName(Name);
    return NewF;
  }

  // If the function exists but has the wrong type, return a bitcast to the
  // right type.
  if (F->getType() != PointerType::getUnqual(Ty))
    return ConstantExpr::getBitCast(F, PointerType::getUnqual(Ty));

  // Otherwise, we just found the existing function or a prototype.
  return F;
}

Constant *Module::getOrInsertTargetIntrinsic(StringRef Name,
                                             FunctionType *Ty,
                                             AttributeSet AttributeList) {
  // See if we have a definition for the specified function already.
  GlobalValue *F = getNamedValue(Name);
  if (F == 0) {
    // Nope, add it
    Function *New = Function::Create(Ty, GlobalVariable::ExternalLinkage, Name);
    New->setAttributes(AttributeList);
    FunctionList.push_back(New);
    return New; // Return the new prototype.
  }

  // Otherwise, we just found the existing function or a prototype.
  return F;
}

Constant *Module::getOrInsertFunction(StringRef Name,
                                      FunctionType *Ty) {
  return getOrInsertFunction(Name, Ty, AttributeSet());
}

// getOrInsertFunction - Look up the specified function in the module symbol
// table.  If it does not exist, add a prototype for the function and return it.
// This version of the method takes a null terminated list of function
// arguments, which makes it easier for clients to use.
//
Constant *Module::getOrInsertFunction(StringRef Name,
                                      AttributeSet AttributeList,
                                      Type *RetTy, ...) {
  va_list Args;
  va_start(Args, RetTy);

  // Build the list of argument types...
  std::vector<Type*> ArgTys;
  while (Type *ArgTy = va_arg(Args, Type*))
    ArgTys.push_back(ArgTy);

  va_end(Args);

  // Build the function type and chain to the other getOrInsertFunction...
  return getOrInsertFunction(Name,
                             FunctionType::get(RetTy, ArgTys, false),
                             AttributeList);
}

Constant *Module::getOrInsertFunction(StringRef Name,
                                      Type *RetTy, ...) {
  va_list Args;
  va_start(Args, RetTy);

  // Build the list of argument types...
  std::vector<Type*> ArgTys;
  while (Type *ArgTy = va_arg(Args, Type*))
    ArgTys.push_back(ArgTy);

  va_end(Args);

  // Build the function type and chain to the other getOrInsertFunction...
  return getOrInsertFunction(Name,
                             FunctionType::get(RetTy, ArgTys, false),
                             AttributeSet());
}

// getFunction - Look up the specified function in the module symbol table.
// If it does not exist, return null.
//
Function *Module::getFunction(StringRef Name) const {
  return dyn_cast_or_null<Function>(getNamedValue(Name));
}

//===----------------------------------------------------------------------===//
// Methods for easy access to the global variables in the module.
//

/// getGlobalVariable - Look up the specified global variable in the module
/// symbol table.  If it does not exist, return null.  The type argument
/// should be the underlying type of the global, i.e., it should not have
/// the top-level PointerType, which represents the address of the global.
/// If AllowLocal is set to true, this function will return types that
/// have an local. By default, these types are not returned.
///
GlobalVariable *Module::getGlobalVariable(StringRef Name,
                                          bool AllowLocal) const {
  if (GlobalVariable *Result =
      dyn_cast_or_null<GlobalVariable>(getNamedValue(Name)))
    if (AllowLocal || !Result->hasLocalLinkage())
      return Result;
  return 0;
}

/// getOrInsertGlobal - Look up the specified global in the module symbol table.
///   1. If it does not exist, add a declaration of the global and return it.
///   2. Else, the global exists but has the wrong type: return the function
///      with a constantexpr cast to the right type.
///   3. Finally, if the existing global is the correct delclaration, return the
///      existing global.
Constant *Module::getOrInsertGlobal(StringRef Name, Type *Ty) {
  // See if we have a definition for the specified global already.
  GlobalVariable *GV = dyn_cast_or_null<GlobalVariable>(getNamedValue(Name));
  if (GV == 0) {
    // Nope, add it
    GlobalVariable *New =
      new GlobalVariable(*this, Ty, false, GlobalVariable::ExternalLinkage,
                         0, Name);
     return New;                    // Return the new declaration.
  }

  // If the variable exists but has the wrong type, return a bitcast to the
  // right type.
  if (GV->getType() != PointerType::getUnqual(Ty))
    return ConstantExpr::getBitCast(GV, PointerType::getUnqual(Ty));

  // Otherwise, we just found the existing function or a prototype.
  return GV;
}

//===----------------------------------------------------------------------===//
// Methods for easy access to the global variables in the module.
//

// getNamedAlias - Look up the specified global in the module symbol table.
// If it does not exist, return null.
//
GlobalAlias *Module::getNamedAlias(StringRef Name) const {
  return dyn_cast_or_null<GlobalAlias>(getNamedValue(Name));
}

/// getNamedMetadata - Return the first NamedMDNode in the module with the
/// specified name. This method returns null if a NamedMDNode with the
/// specified name is not found.
NamedMDNode *Module::getNamedMetadata(const Twine &Name) const {
  SmallString<256> NameData;
  StringRef NameRef = Name.toStringRef(NameData);
  return static_cast<StringMap<NamedMDNode*> *>(NamedMDSymTab)->lookup(NameRef);
}

/// getOrInsertNamedMetadata - Return the first named MDNode in the module
/// with the specified name. This method returns a new NamedMDNode if a
/// NamedMDNode with the specified name is not found.
NamedMDNode *Module::getOrInsertNamedMetadata(StringRef Name) {
  NamedMDNode *&NMD =
    (*static_cast<StringMap<NamedMDNode *> *>(NamedMDSymTab))[Name];
  if (!NMD) {
    NMD = new NamedMDNode(Name);
    NMD->setParent(this);
    NamedMDList.push_back(NMD);
  }
  return NMD;
}

/// eraseNamedMetadata - Remove the given NamedMDNode from this module and
/// delete it.
void Module::eraseNamedMetadata(NamedMDNode *NMD) {
  static_cast<StringMap<NamedMDNode *> *>(NamedMDSymTab)->erase(NMD->getName());
  NamedMDList.erase(NMD);
}

/// getModuleFlagsMetadata - Returns the module flags in the provided vector.
void Module::
getModuleFlagsMetadata(SmallVectorImpl<ModuleFlagEntry> &Flags) const {
  const NamedMDNode *ModFlags = getModuleFlagsMetadata();
  if (!ModFlags) return;

  for (unsigned i = 0, e = ModFlags->getNumOperands(); i != e; ++i) {
    MDNode *Flag = ModFlags->getOperand(i);
    ConstantInt *Behavior = cast<ConstantInt>(Flag->getOperand(0));
    MDString *Key = cast<MDString>(Flag->getOperand(1));
    Value *Val = Flag->getOperand(2);
    Flags.push_back(ModuleFlagEntry(ModFlagBehavior(Behavior->getZExtValue()),
                                    Key, Val));
  }
}

/// getModuleFlagsMetadata - Returns the NamedMDNode in the module that
/// represents module-level flags. This method returns null if there are no
/// module-level flags.
NamedMDNode *Module::getModuleFlagsMetadata() const {
  return getNamedMetadata("llvm.module.flags");
}

/// getOrInsertModuleFlagsMetadata - Returns the NamedMDNode in the module that
/// represents module-level flags. If module-level flags aren't found, it
/// creates the named metadata that contains them.
NamedMDNode *Module::getOrInsertModuleFlagsMetadata() {
  return getOrInsertNamedMetadata("llvm.module.flags");
}

/// addModuleFlag - Add a module-level flag to the module-level flags
/// metadata. It will create the module-level flags named metadata if it doesn't
/// already exist.
void Module::addModuleFlag(ModFlagBehavior Behavior, StringRef Key,
                           Value *Val) {
  Type *Int32Ty = Type::getInt32Ty(Context);
  Value *Ops[3] = {
    ConstantInt::get(Int32Ty, Behavior), MDString::get(Context, Key), Val
  };
  getOrInsertModuleFlagsMetadata()->addOperand(MDNode::get(Context, Ops));
}
void Module::addModuleFlag(ModFlagBehavior Behavior, StringRef Key,
                           uint32_t Val) {
  Type *Int32Ty = Type::getInt32Ty(Context);
  addModuleFlag(Behavior, Key, ConstantInt::get(Int32Ty, Val));
}
void Module::addModuleFlag(MDNode *Node) {
  assert(Node->getNumOperands() == 3 &&
         "Invalid number of operands for module flag!");
  assert(isa<ConstantInt>(Node->getOperand(0)) &&
         isa<MDString>(Node->getOperand(1)) &&
         "Invalid operand types for module flag!");
  getOrInsertModuleFlagsMetadata()->addOperand(Node);
}

//===----------------------------------------------------------------------===//
// Methods to control the materialization of GlobalValues in the Module.
//
void Module::setMaterializer(GVMaterializer *GVM) {
  assert(!Materializer &&
         "Module already has a GVMaterializer.  Call MaterializeAllPermanently"
         " to clear it out before setting another one.");
  Materializer.reset(GVM);
}

bool Module::isMaterializable(const GlobalValue *GV) const {
  if (Materializer)
    return Materializer->isMaterializable(GV);
  return false;
}

bool Module::isDematerializable(const GlobalValue *GV) const {
  if (Materializer)
    return Materializer->isDematerializable(GV);
  return false;
}

bool Module::Materialize(GlobalValue *GV, std::string *ErrInfo) {
  if (Materializer)
    return Materializer->Materialize(GV, ErrInfo);
  return false;
}

void Module::Dematerialize(GlobalValue *GV) {
  if (Materializer)
    return Materializer->Dematerialize(GV);
}

bool Module::MaterializeAll(std::string *ErrInfo) {
  if (!Materializer)
    return false;
  return Materializer->MaterializeModule(this, ErrInfo);
}

bool Module::MaterializeAllPermanently(std::string *ErrInfo) {
  if (MaterializeAll(ErrInfo))
    return true;
  Materializer.reset();
  return false;
}

//===----------------------------------------------------------------------===//
// Other module related stuff.
//


// dropAllReferences() - This function causes all the subelements to "let go"
// of all references that they are maintaining.  This allows one to 'delete' a
// whole module at a time, even though there may be circular references... first
// all references are dropped, and all use counts go to zero.  Then everything
// is deleted for real.  Note that no operations are valid on an object that
// has "dropped all references", except operator delete.
//
void Module::dropAllReferences() {
  for(Module::iterator I = begin(), E = end(); I != E; ++I)
    I->dropAllReferences();

  for(Module::global_iterator I = global_begin(), E = global_end(); I != E; ++I)
    I->dropAllReferences();

  for(Module::alias_iterator I = alias_begin(), E = alias_end(); I != E; ++I)
    I->dropAllReferences();
}

// @LOCALMOD-BEGIN
void Module::convertMetadataToLibraryList() {
  LibraryList.clear();
  // Get the DepLib node
  NamedMDNode *Node = getNamedMetadata("DepLibs");
  if (!Node)
    return;
  for (unsigned i = 0; i < Node->getNumOperands(); i++) {
    MDString* Mds = dyn_cast_or_null<MDString>(
        Node->getOperand(i)->getOperand(0));
    assert(Mds && "Bad NamedMetadata operand");
    LibraryList.push_back(Mds->getString());
  }
  // Clear the metadata so the linker won't try to merge it
  Node->dropAllReferences();
}

void Module::convertLibraryListToMetadata() const {
  if (LibraryList.size() == 0)
    return;
  // Get the DepLib node
  NamedMDNode *Node = getNamedMetadata("DepLibs");
  assert(Node && "DepLibs metadata node missing");
  // Erase all existing operands
  Node->dropAllReferences();
  // Add all libraries from the library list
  for (Module::lib_iterator I = lib_begin(), E = lib_end(); I != E; ++I) {
    MDString *value = MDString::get(getContext(), *I);
    Node->addOperand(MDNode::get(getContext(),
                                 makeArrayRef(static_cast<Value*>(value))));
  }
}

void Module::addLibrary(StringRef Lib) {
  for (Module::lib_iterator I = lib_begin(), E = lib_end(); I != E; ++I)
    if (*I == Lib)
      return;
  LibraryList.push_back(Lib);
  // If the module previously had no deplibs, it may not have the metadata node.
  // Ensure it exists now, so that we don't have to create it in
  // convertLibraryListToMetadata (which is const)
  getOrInsertNamedMetadata("DepLibs");
}

void Module::removeLibrary(StringRef Lib) {
  LibraryListType::iterator I = LibraryList.begin();
  LibraryListType::iterator E = LibraryList.end();
  for (;I != E; ++I)
    if (*I == Lib) {
      LibraryList.erase(I);
      return;
    }
}

static std::string
ModuleMetaGet(const Module *module, StringRef MetaName) {
  NamedMDNode *node = module->getNamedMetadata(MetaName);
  if (node == NULL)
    return "";
  assert(node->getNumOperands() == 1);
  MDNode *subnode = node->getOperand(0);
  assert(subnode->getNumOperands() == 1);
  MDString *value = dyn_cast<MDString>(subnode->getOperand(0));
  assert(value != NULL);
  return value->getString();
}

static void
ModuleMetaSet(Module *module, StringRef MetaName, StringRef ValueStr) {
  NamedMDNode *node = module->getNamedMetadata(MetaName);
  if (node)
    module->eraseNamedMetadata(node);
  node = module->getOrInsertNamedMetadata(MetaName);
  MDString *value = MDString::get(module->getContext(), ValueStr);
  node->addOperand(MDNode::get(module->getContext(),
                   makeArrayRef(static_cast<Value*>(value))));
}

const std::string &Module::getSOName() const {
  if (ModuleSOName == "")
    ModuleSOName.assign(ModuleMetaGet(this, "SOName"));
  return ModuleSOName;
}

void Module::setSOName(StringRef Name) {
  ModuleMetaSet(this, "SOName", Name);
  ModuleSOName = Name;
}

void Module::setOutputFormat(Module::OutputFormat F) {
  const char *formatStr;
  switch (F) {
  case ObjectOutputFormat: formatStr = "object"; break;
  case SharedOutputFormat: formatStr = "shared"; break;
  case ExecutableOutputFormat: formatStr = "executable"; break;
  default:
    llvm_unreachable("Unrecognized output format in setOutputFormat()");
  }
  ModuleMetaSet(this, "OutputFormat", formatStr);
}

Module::OutputFormat Module::getOutputFormat() const {
  std::string formatStr = ModuleMetaGet(this, "OutputFormat");
  if (formatStr == "" || formatStr == "object")
    return ObjectOutputFormat;
  else if (formatStr == "shared")
    return SharedOutputFormat;
  else if (formatStr == "executable")
    return ExecutableOutputFormat;
  llvm_unreachable("Invalid module compile type in getOutputFormat()");
}

void
Module::wrapSymbol(StringRef symName) {
  std::string wrapSymName("__wrap_");
  wrapSymName += symName;

  std::string realSymName("__real_");
  realSymName += symName;

  GlobalValue *SymGV = getNamedValue(symName);
  GlobalValue *WrapGV = getNamedValue(wrapSymName);
  GlobalValue *RealGV = getNamedValue(realSymName);

  // Replace uses of "sym" with __wrap_sym.
  if (SymGV) {
    if (!WrapGV)
      WrapGV = cast<GlobalValue>(getOrInsertGlobal(wrapSymName,
                                                   SymGV->getType()));
    SymGV->replaceAllUsesWith(ConstantExpr::getBitCast(WrapGV,
                                                       SymGV->getType()));
  }

  // Replace uses of "__real_sym" with "sym".
  if (RealGV) {
    if (!SymGV)
      SymGV = cast<GlobalValue>(getOrInsertGlobal(symName, RealGV->getType()));
    RealGV->replaceAllUsesWith(ConstantExpr::getBitCast(SymGV,
                                                        RealGV->getType()));
  }
}

// The metadata key prefix for NeededRecords.
static const char *NeededPrefix = "NeededRecord_";

void
Module::dumpMeta(raw_ostream &OS) const {
  OS << "OutputFormat: ";
  switch (getOutputFormat()) {
    case Module::ObjectOutputFormat: OS << "object"; break;
    case Module::SharedOutputFormat: OS << "shared"; break;
    case Module::ExecutableOutputFormat: OS << "executable"; break;
  }
  OS << "\n";
  OS << "SOName: " << getSOName() << "\n";
  for (Module::lib_iterator L = lib_begin(),
                            E = lib_end();
       L != E; ++L) {
    OS << "NeedsLibrary: " << (*L) << "\n";
  }
  std::vector<NeededRecord> NList;
  getNeededRecords(&NList);
  for (unsigned i = 0; i < NList.size(); ++i) {
    const NeededRecord &NR = NList[i];
    OS << StringRef(NeededPrefix) << NR.DynFile << ": ";
    for (unsigned j = 0; j < NR.Symbols.size(); ++j) {
      if (j != 0)
        OS << " ";
      OS << NR.Symbols[j];
    }
    OS << "\n";
  }
}

void Module::addNeededRecord(StringRef DynFile, GlobalValue *GV) {
  if (DynFile.empty()) {
    // We never resolved this symbol, even after linking.
    // This should only happen in a shared object.
    // It is safe to ignore this symbol, and let the dynamic loader
    // figure out where it comes from.
    return;
  }
  std::string Key = NeededPrefix;
  Key += DynFile;
  // Get the node for this file.
  NamedMDNode *Node = getOrInsertNamedMetadata(Key);
  // Add this global value's name to the list.
  MDString *value = MDString::get(getContext(), GV->getName());
  Node->addOperand(MDNode::get(getContext(),
                   makeArrayRef(static_cast<Value*>(value))));
}

// Get the NeededRecord for SOName.
// Returns an empty NeededRecord if there was no metadata found.
static void getNeededRecordFor(const Module *M,
                               StringRef SOName,
                               Module::NeededRecord *NR) {
  NR->DynFile = SOName;
  NR->Symbols.clear();

  std::string Key = NeededPrefix;
  Key += SOName;
  NamedMDNode *Node = M->getNamedMetadata(Key);
  if (!Node)
    return;

  for (unsigned k = 0; k < Node->getNumOperands(); ++k) {
    // Insert the symbol name.
    const MDString *SymName =
        dyn_cast<MDString>(Node->getOperand(k)->getOperand(0));
    NR->Symbols.push_back(SymName->getString());
  }
}

// Place the complete list of needed records in NeededOut.
void Module::getNeededRecords(std::vector<NeededRecord> *NeededOut) const {
  // Iterate through the libraries needed, grabbing each NeededRecord.
  for (lib_iterator I = lib_begin(), E = lib_end(); I != E; ++I) {
    NeededRecord NR;
    getNeededRecordFor(this, *I, &NR);
    NeededOut->push_back(NR);
  }
}
// @LOCALMOD-END
