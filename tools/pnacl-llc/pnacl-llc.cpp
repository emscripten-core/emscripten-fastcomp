//===-- pnacl-llc.cpp - PNaCl-specific llc: pexe ---> nexe  ---------------===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
// pnacl-llc: the core of the PNaCl translator, compiling a pexe into a nexe.
//
//===----------------------------------------------------------------------===//

#include "ThreadedFunctionQueue.h"
#include "ThreadedStreamingCache.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/Bitcode/NaCl/NaClReaderWriter.h"
#include "llvm/Bitcode/ReaderWriter.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/DataStream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/Mutex.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/StreamableMemoryObject.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Transforms/NaCl.h"
#include <pthread.h>
#include <memory>
using namespace llvm;

// NOTE: When __native_client__ is defined it means pnacl-llc is built as a
// sandboxed translator (from pnacl-llc.pexe to pnacl-llc.nexe). In this mode
// it uses SRPC operations instead of direct OS intefaces.
#if defined(__native_client__)
int srpc_main(int argc, char **argv);
int getObjectFileFD(unsigned index);
DataStreamer *getNaClBitcodeStreamer();

fatal_error_handler_t getSRPCErrorHandler();
#endif

cl::opt<NaClFileFormat>
InputFileFormat(
    "bitcode-format",
    cl::desc("Define format of input file:"),
    cl::values(
        clEnumValN(LLVMFormat, "llvm", "LLVM file (default)"),
        clEnumValN(PNaClFormat, "pnacl", "PNaCl bitcode file"),
        clEnumValEnd),
#if defined(__native_client__)
    cl::init(PNaClFormat)
#else
    cl::init(LLVMFormat)
#endif
                );

// General options for llc.  Other pass-specific options are specified
// within the corresponding llc passes, and target-specific options
// and back-end code generation options are specified with the target machine.
//
static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"), cl::value_desc("filename"));

// Using bitcode streaming allows compilation of one function at a time. This
// allows earlier functions to be compiled before later functions are read from
// the bitcode but of course means no whole-module optimizations. This means
// that Module passes that run should only touch globals/function declarations
// and not function bodies, otherwise the streaming and non-streaming code
// pathes wouldn't emit the same code for each function. For now, streaming is
// only supported for files and stdin.
static cl::opt<bool>
LazyBitcode("streaming-bitcode",
  cl::desc("Use lazy bitcode streaming for file inputs"),
  cl::init(false));

static cl::opt<bool>
PNaClABIVerify("pnaclabi-verify",
  cl::desc("Verify PNaCl bitcode ABI before translating"),
  cl::init(false));
static cl::opt<bool>
PNaClABIVerifyFatalErrors("pnaclabi-verify-fatal-errors",
  cl::desc("PNaCl ABI verification errors are fatal"),
  cl::init(false));


static cl::opt<bool>
NoIntegratedAssembler("no-integrated-as", cl::Hidden,
                      cl::desc("Disable integrated assembler"));

// Determine optimization level.
static cl::opt<char>
OptLevel("O",
         cl::desc("Optimization level. [-O0, -O1, -O2, or -O3] "
                  "(default = '-O2')"),
         cl::Prefix,
         cl::ZeroOrMore,
         cl::init(' '));

static cl::opt<std::string>
UserDefinedTriple("mtriple", cl::desc("Set target triple"));

static cl::opt<bool> NoVerify("disable-verify", cl::Hidden,
                              cl::desc("Do not verify input module"));

static cl::opt<bool>
    DisableSimplifyLibCalls("disable-simplify-libcalls",
                            cl::desc("Disable simplify-libcalls"));

static cl::opt<unsigned>
SplitModuleCount("split-module",
                 cl::desc("Split PNaCl module"), cl::init(1U));

enum SplitModuleSchedulerKind {
  SplitModuleDynamic,
  SplitModuleStatic
};

static cl::opt<SplitModuleSchedulerKind>
SplitModuleSched(
    "split-module-sched",
    cl::desc("Choose thread scheduler for split module compilation."),
    cl::values(
        clEnumValN(SplitModuleDynamic, "dynamic",
                   "Dynamic thread scheduling (default)"),
        clEnumValN(SplitModuleStatic, "static",
                   "Static thread scheduling"),
        clEnumValEnd),
    cl::init(SplitModuleDynamic));

/// Compile the module provided to pnacl-llc. The file name for reading the
/// module and other options are taken from globals populated by command-line
/// option parsing.
static int compileModule(StringRef ProgramName);

#if !defined(__native_client__)
// GetFileNameRoot - Helper function to get the basename of a filename.
static std::string
GetFileNameRoot(StringRef InputFilename) {
  std::string IFN = InputFilename;
  std::string outputFilename;
  int Len = IFN.length();
  if ((Len > 2) &&
      IFN[Len-3] == '.' &&
      ((IFN[Len-2] == 'b' && IFN[Len-1] == 'c') ||
       (IFN[Len-2] == 'l' && IFN[Len-1] == 'l'))) {
    outputFilename = std::string(IFN.begin(), IFN.end()-3); // s/.bc/.s/
  } else {
    outputFilename = IFN;
  }
  return outputFilename;
}

static tool_output_file *GetOutputStream(const char *TargetName,
                                         Triple::OSType OS,
                                         std::string Filename) {
  // If we don't yet have an output filename, make one.
  if (Filename.empty()) {
    if (InputFilename == "-")
      Filename = "-";
    else {
      Filename = GetFileNameRoot(InputFilename);

      switch (FileType) {
      case TargetMachine::CGFT_AssemblyFile:
        if (TargetName[0] == 'c') {
          if (TargetName[1] == 0)
            Filename += ".cbe.c";
          else if (TargetName[1] == 'p' && TargetName[2] == 'p')
            Filename += ".cpp";
          else
            Filename += ".s";
        } else
          Filename += ".s";
        break;
      case TargetMachine::CGFT_ObjectFile:
        if (OS == Triple::Win32)
          Filename += ".obj";
        else
          Filename += ".o";
        break;
      case TargetMachine::CGFT_Null:
        Filename += ".null";
        break;
      }
    }
  }

  // Decide if we need "binary" output.
  bool Binary = false;
  switch (FileType) {
  case TargetMachine::CGFT_AssemblyFile:
    break;
  case TargetMachine::CGFT_ObjectFile:
  case TargetMachine::CGFT_Null:
    Binary = true;
    break;
  }

  // Open the file.
  std::string error;
  sys::fs::OpenFlags OpenFlags = sys::fs::F_None;
  if (!Binary)
    OpenFlags |= sys::fs::F_Text;
  tool_output_file *FDOut =
      new tool_output_file(Filename.c_str(), error, OpenFlags);
  if (!error.empty()) {
    errs() << error << '\n';
    delete FDOut;
    return nullptr;
  }

  return FDOut;
}
#endif // !defined(__native_client__)

// main - Entry point for the llc compiler.
//
int llc_main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  // Enable debug stream buffering.
  EnableDebugBuffering = true;

  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

#if defined(__native_client__)
  install_fatal_error_handler(getSRPCErrorHandler(), nullptr);
#endif

  // Initialize targets first, so that --version shows registered targets.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
#if !defined(__native_client__)
  // Prune asm parsing from sandboxed translator.
  // Do not prune "AsmPrinters" because that includes
  // the direct object emission.
  InitializeAllAsmParsers();
#endif

  // Initialize codegen and IR passes used by pnacl-llc so that the -print-after,
  // -print-before, and -stop-after options work.
  PassRegistry *Registry = PassRegistry::getPassRegistry();
  initializeCore(*Registry);
  initializeCodeGen(*Registry);
  initializeLoopStrengthReducePass(*Registry);
  initializeLowerIntrinsicsPass(*Registry);
  initializeUnreachableBlockElimPass(*Registry);

  // Register the target printer for --version.
  cl::AddExtraVersionPrinter(TargetRegistry::printRegisteredTargetsForVersion);

  // Enable the PNaCl ABI verifier by default in sandboxed mode.
#if defined(__native_client__)
  PNaClABIVerify = true;
  PNaClABIVerifyFatalErrors = true;
#endif

  cl::ParseCommandLineOptions(argc, argv, "pnacl-llc\n");

#if defined(__native_client__)
  // If the user explicitly requests LLVM format in sandboxed mode
  // (where the default is PNaCl format), they probably want debug
  // metadata enabled.
  if (InputFileFormat == LLVMFormat) {
    PNaClABIAllowDebugMetadata = true;
  }
#endif

  if (SplitModuleCount > 1)
    LLVMStartMultithreaded();

  return compileModule(argv[0]);
}

static void CheckABIVerifyErrors(PNaClABIErrorReporter &Reporter,
                                 const Twine &Name) {
  if (PNaClABIVerify && Reporter.getErrorCount() > 0) {
    std::string errors;
    raw_string_ostream os(errors);
    os << (PNaClABIVerifyFatalErrors ? "ERROR: " : "WARNING: ");
    os << Name << " is not valid PNaCl bitcode:\n";
    Reporter.printErrors(os);
    if (PNaClABIVerifyFatalErrors) {
      report_fatal_error(os.str());
    }
    errs() << os.str();
  }
  Reporter.reset();
}

static Module* getModule(StringRef ProgramName, LLVMContext &Context,
                         StreamingMemoryObject *StreamingObject) {
  Module *M = nullptr;
  SMDiagnostic Err;
  if (LazyBitcode) {
    std::string StrError;
    if (InputFileFormat == PNaClFormat) {
      M = getNaClStreamedBitcodeModule(
          InputFilename,
          new ThreadedStreamingCache(StreamingObject), Context, &StrError);
    } else if (InputFileFormat == LLVMFormat) {
      M = getStreamedBitcodeModule(
          InputFilename,
          new ThreadedStreamingCache(StreamingObject), Context, &StrError);
    } else {
      llvm_unreachable("Unknown bitcode format");
    }
    if (!StrError.empty())
      Err = SMDiagnostic(InputFilename, SourceMgr::DK_Error, StrError);
  } else {
#if defined(__native_client__)
    llvm_unreachable("native client SRPC only supports streaming");
#else
    // Parses binary bitcode as well as textual assembly
    // (so pulls in more code into pnacl-llc).
    M = NaClParseIRFile(InputFilename, InputFileFormat, Err, Context);
#endif
  }
  if (!M) {
#if defined(__native_client__)
    report_fatal_error(Err.getMessage());
#else
    // Err.print is prettier, so use it for the non-sandboxed translator.
    Err.print(ProgramName.data(), errs());
    return nullptr;
#endif
  }
  return M;
}

static int runCompilePasses(Module *mod,
                            unsigned ModuleIndex,
                            ThreadedFunctionQueue *FuncQueue,
                            const Triple &TheTriple,
                            TargetMachine &Target,
                            StringRef ProgramName,
                            formatted_raw_ostream &FOS){
  PNaClABIErrorReporter ABIErrorReporter;

  if (SplitModuleCount > 1) {
    // Add function and global names, and give them external linkage.
    // This relies on LLVM's consistent auto-generation of names, we could
    // maybe do our own in case something changes there.
    for (Module::iterator I = mod->begin(), E = mod->end(); I != E; ++I) {
      if (!I->hasName())
        I->setName("Function");
      if (I->hasInternalLinkage())
        I->setLinkage(GlobalValue::ExternalLinkage);
    }
    for (Module::global_iterator GI = mod->global_begin(),
         GE = mod->global_end();
         GI != GE; ++GI) {
      if (!GI->hasName())
        GI->setName("Global");
      if (GI->hasInternalLinkage())
        GI->setLinkage(GlobalValue::ExternalLinkage);
    }
    if (ModuleIndex > 0) {
      // Remove the initializers for all global variables, turning them into
      // declarations.
      for (Module::global_iterator GI = mod->global_begin(),
          GE = mod->global_end();
          GI != GE; ++GI) {
        assert(GI->hasInitializer() && "Global variable missing initializer");
        Constant *Init = GI->getInitializer();
        GI->setInitializer(nullptr);
        if (Init->getNumUses() == 0)
          Init->destroyConstant();
      }
    }
  }

  // Build up all of the passes that we want to do to the module.
  std::unique_ptr<PassManagerBase> PM;
  if (LazyBitcode)
    PM.reset(new FunctionPassManager(mod));
  else
    PM.reset(new PassManager());

  // Add the target data from the target machine, if it exists, or the module.
  if (const DataLayout *DL = Target.getDataLayout())
    mod->setDataLayout(DL);
  PM->add(new DataLayoutPass(mod));

  // For conformance with llc, we let the user disable LLVM IR verification with
  // -disable-verify. Unlike llc, when LLVM IR verification is enabled we only
  // run it once, before PNaCl ABI verification.
  if (!NoVerify)
    PM->add(createVerifierPass());

  // Add the ABI verifier pass before the analysis and code emission passes.
  if (PNaClABIVerify)
    PM->add(createPNaClABIVerifyFunctionsPass(&ABIErrorReporter));

  // Add the intrinsic resolution pass. It assumes ABI-conformant code.
  PM->add(createResolvePNaClIntrinsicsPass());

  // Add an appropriate TargetLibraryInfo pass for the module's triple.
  TargetLibraryInfo *TLI = new TargetLibraryInfo(TheTriple);
  if (DisableSimplifyLibCalls)
    TLI->disableAllFunctions();
  PM->add(TLI);

  // Allow subsequent passes and the backend to better optimize instructions
  // that were simplified for PNaCl's ABI. This pass uses the TargetLibraryInfo
  // above.
  PM->add(createBackendCanonicalizePass());

  // Add internal analysis passes from the target machine.
  Target.addAnalysisPasses(*PM);

  // Ask the target to add backend passes as necessary. We explicitly ask it
  // not to add the verifier pass because we added it earlier.
  if (Target.addPassesToEmitFile(*PM, FOS, FileType,
                                 /* DisableVerify */ true)) {
    errs() << ProgramName
    << ": target does not support generation of this file type!\n";
    return 1;
  }

  if (LazyBitcode) {
    auto FPM = static_cast<FunctionPassManager *>(PM.get());
    FPM->doInitialization();
    unsigned FuncIndex = 0;
    switch (SplitModuleSched) {
    case SplitModuleStatic:
      for (Module::iterator I = mod->begin(), E = mod->end(); I != E; ++I) {
        if (FuncQueue->GrabFunctionStatic(FuncIndex, ModuleIndex)) {
          FPM->run(*I);
          CheckABIVerifyErrors(ABIErrorReporter, "Function " + I->getName());
          I->Dematerialize();
        }
        ++FuncIndex;
      }
      break;
    case SplitModuleDynamic:
      unsigned ChunkSize = 0;
      unsigned NumFunctions = FuncQueue->Size();
      Module::iterator I = mod->begin();
      while (FuncIndex < NumFunctions) {
        ChunkSize = FuncQueue->RecommendedChunkSize();
        unsigned NextIndex;
        bool grabbed = FuncQueue->GrabFunctionDynamic(FuncIndex, ChunkSize,
                                                      NextIndex);
        if (grabbed) {
          while (FuncIndex < NextIndex) {
            if (!I->isMaterializable() && I->isDeclaration()) {
              ++I;
              continue;
            }
            FPM->run(*I);
            CheckABIVerifyErrors(ABIErrorReporter, "Function " + I->getName());
            I->Dematerialize();
            ++FuncIndex;
            ++I;
          }
        } else {
          while (FuncIndex < NextIndex) {
            if (!I->isMaterializable() && I->isDeclaration()) {
              ++I;
              continue;
            }
            ++FuncIndex;
            ++I;
          }
        }
      }
      break;
    }
    FPM->doFinalization();
  } else
    static_cast<PassManager *>(PM.get())->run(*mod);

  return 0;
}

static int compileSplitModule(const TargetOptions &Options,
                              const Triple &TheTriple,
                              const Target *TheTarget,
                              const std::string &FeaturesStr,
                              CodeGenOpt::Level OLvl,
                              const StringRef &ProgramName,
                              Module *GlobalModule,
                              StreamingMemoryObject *StreamingObject,
                              unsigned ModuleIndex,
                              ThreadedFunctionQueue *FuncQueue) {
  std::auto_ptr<TargetMachine>
    target(TheTarget->createTargetMachine(TheTriple.getTriple(),
                                          MCPU, FeaturesStr, Options,
                                          RelocModel, CMModel, OLvl));
  assert(target.get() && "Could not allocate target machine!");
  TargetMachine &Target = *target.get();
  // Override default to generate verbose assembly.
  Target.setAsmVerbosityDefault(true);
  if (RelaxAll.getNumOccurrences() > 0 &&
      FileType != TargetMachine::CGFT_ObjectFile)
    errs() << ProgramName
             << ": warning: ignoring -mc-relax-all because filetype != obj";
  // The OwningPtrs are only used if we are not the primary module.
  std::unique_ptr<LLVMContext> C;
  std::unique_ptr<Module> M;
  Module *mod(nullptr);

  if (ModuleIndex == 0) {
    mod = GlobalModule;
  } else {
    C.reset(new LLVMContext());
    mod = getModule(ProgramName, *C, StreamingObject);
    if (!mod)
      return 1;
    M.reset(mod);

    // Add declarations for external functions required by PNaCl. The
    // ResolvePNaClIntrinsics function pass running during streaming
    // depends on these declarations being in the module.
    std::unique_ptr<ModulePass> AddPNaClExternalDeclsPass(
        createAddPNaClExternalDeclsPass());
    AddPNaClExternalDeclsPass->runOnModule(*M);
    AddPNaClExternalDeclsPass.reset();
  }

  mod->setTargetTriple(Triple::normalize(UserDefinedTriple));

  {
#if !defined(__native_client__)
      // Figure out where we are going to send the output.
    std::string N(OutputFilename);
    raw_string_ostream OutFileName(N);
    if (ModuleIndex > 0)
      OutFileName << ".module" << ModuleIndex;
    std::unique_ptr<tool_output_file> Out
        (GetOutputStream(TheTarget->getName(), TheTriple.getOS(),
                         OutFileName.str()));
    if (!Out) return 1;
    formatted_raw_ostream FOS(Out->os());
#else
    raw_fd_ostream ROS(getObjectFileFD(ModuleIndex), true, sys::fs::F_None);
    ROS.SetBufferSize(1 << 20);
    formatted_raw_ostream FOS(ROS);
#endif
    int ret = runCompilePasses(mod, ModuleIndex, FuncQueue,
                               TheTriple, Target, ProgramName,
                               FOS);
    if (ret)
      return ret;
#if defined (__native_client__)
    FOS.flush();
    ROS.flush();
#else
    // Declare success.
    Out->keep();
#endif // __native_client__
  }
  return 0;
}

struct ThreadData {
  const TargetOptions *Options;
  const Triple *TheTriple;
  const Target *TheTarget;
  std::string FeaturesStr;
  CodeGenOpt::Level OLvl;
  std::string ProgramName;
  Module *GlobalModule;
  StreamingMemoryObject *StreamingObject;
  unsigned ModuleIndex;
  ThreadedFunctionQueue *FuncQueue;
};


static void *runCompileThread(void *arg) {
  struct ThreadData *Data = static_cast<ThreadData *>(arg);
  int ret = compileSplitModule(*Data->Options,
                               *Data->TheTriple,
                               Data->TheTarget,
                               Data->FeaturesStr,
                               Data->OLvl,
                               Data->ProgramName,
                               Data->GlobalModule,
                               Data->StreamingObject,
                               Data->ModuleIndex,
                               Data->FuncQueue);
  return reinterpret_cast<void *>(static_cast<intptr_t>(ret));
}

static int compileModule(StringRef ProgramName) {
  // Use a new context instead of the global context for the main module. It must
  // outlive the module object, declared below. We do this because
  // lib/CodeGen/PseudoSourceValue.cpp gets a type from the global context and
  // races with any other use of the context. Rather than doing an invasive
  // plumbing change to fix it, we work around it by using a new context here
  // and leaving PseudoSourceValue as the only user of the global context.
  std::unique_ptr<LLVMContext> MainContext(new LLVMContext());
  std::unique_ptr<Module> mod;
  Triple TheTriple;
  PNaClABIErrorReporter ABIErrorReporter;
  std::unique_ptr<StreamingMemoryObject> StreamingObject;

  if (!MainContext) return 1;

#if defined(__native_client__)
  StreamingObject.reset(
      new StreamingMemoryObjectImpl(getNaClBitcodeStreamer()));
#else
  if (LazyBitcode) {
    std::string StrError;
    DataStreamer* FileStreamer(getDataFileStreamer(InputFilename, &StrError));
    if (!StrError.empty()) {
      SMDiagnostic Err(InputFilename, SourceMgr::DK_Error, StrError);
      Err.print(ProgramName.data(), errs());
    }
    if (!FileStreamer)
      return 1;
    StreamingObject.reset(new StreamingMemoryObjectImpl(FileStreamer));
  }
#endif
  mod.reset(getModule(ProgramName, *MainContext.get(), StreamingObject.get()));

  if (!mod) return 1;

  if (PNaClABIVerify) {
    // Verify the module (but not the functions yet)
    std::unique_ptr<ModulePass> VerifyPass(
        createPNaClABIVerifyModulePass(&ABIErrorReporter, LazyBitcode));
    VerifyPass->runOnModule(*mod);
    CheckABIVerifyErrors(ABIErrorReporter, "Module");
    VerifyPass.reset();
  }

  // Add declarations for external functions required by PNaCl. The
  // ResolvePNaClIntrinsics function pass running during streaming
  // depends on these declarations being in the module.
  std::unique_ptr<ModulePass> AddPNaClExternalDeclsPass(
      createAddPNaClExternalDeclsPass());
  AddPNaClExternalDeclsPass->runOnModule(*mod);
  AddPNaClExternalDeclsPass.reset();

  if (UserDefinedTriple.empty()) {
    report_fatal_error("-mtriple must be set to a target triple for pnacl-llc");
  } else {
    mod->setTargetTriple(Triple::normalize(UserDefinedTriple));
    TheTriple = Triple(mod->getTargetTriple());
  }

  // Get the target specific parser.
  std::string Error;
  const Target *TheTarget = TargetRegistry::lookupTarget(MArch, TheTriple,
                                                         Error);
  if (!TheTarget) {
    errs() << ProgramName << ": " << Error;
    return 1;
  }

  TargetOptions Options = InitTargetOptionsFromCodeGenFlags();
  Options.DisableIntegratedAS = NoIntegratedAssembler;

  if (GenerateSoftFloatCalls)
    FloatABIForCalls = FloatABI::Soft;

  // Package up features to be passed to target/subtarget
  std::string FeaturesStr;
  if (MAttrs.size()) {
    SubtargetFeatures Features;
    for (unsigned i = 0; i != MAttrs.size(); ++i)
      Features.AddFeature(MAttrs[i]);
    FeaturesStr = Features.getString();
  }

  CodeGenOpt::Level OLvl = CodeGenOpt::Default;
  switch (OptLevel) {
  default:
    errs() << ProgramName << ": invalid optimization level.\n";
    return 1;
  case ' ': break;
  case '0': OLvl = CodeGenOpt::None; break;
  case '1': OLvl = CodeGenOpt::Less; break;
  case '2': OLvl = CodeGenOpt::Default; break;
  case '3': OLvl = CodeGenOpt::Aggressive; break;
  }

  SmallVector<pthread_t, 4> Pthreads(SplitModuleCount);
  SmallVector<ThreadData, 4> ThreadDatas(SplitModuleCount);
  ThreadedFunctionQueue FuncQueue(mod.get(), SplitModuleCount);

  if (SplitModuleCount == 1) {
    // No need for dynamic scheduling with one thread.
    SplitModuleSched = SplitModuleStatic;
    return compileSplitModule(Options, TheTriple, TheTarget, FeaturesStr,
                              OLvl, ProgramName, mod.get(), nullptr, 0,
                              &FuncQueue);
  }

  for(unsigned ModuleIndex = 0; ModuleIndex < SplitModuleCount; ++ModuleIndex) {
    ThreadDatas[ModuleIndex].Options = &Options;
    ThreadDatas[ModuleIndex].TheTriple = &TheTriple;
    ThreadDatas[ModuleIndex].TheTarget = TheTarget;
    ThreadDatas[ModuleIndex].FeaturesStr = FeaturesStr;
    ThreadDatas[ModuleIndex].OLvl = OLvl;
    ThreadDatas[ModuleIndex].ProgramName = ProgramName.str();
    ThreadDatas[ModuleIndex].GlobalModule = mod.get();
    ThreadDatas[ModuleIndex].StreamingObject = StreamingObject.get();
    ThreadDatas[ModuleIndex].ModuleIndex = ModuleIndex;
    ThreadDatas[ModuleIndex].FuncQueue = &FuncQueue;
    if (pthread_create(&Pthreads[ModuleIndex], nullptr, runCompileThread,
                        &ThreadDatas[ModuleIndex])) {
      report_fatal_error("Failed to create thread");
    }
  }
  for(unsigned ModuleIndex = 0; ModuleIndex < SplitModuleCount; ++ModuleIndex) {
    void *retval;
    if (pthread_join(Pthreads[ModuleIndex], &retval))
      report_fatal_error("Failed to join thread");
    intptr_t ret = reinterpret_cast<intptr_t>(retval);
    if (ret != 0)
      report_fatal_error("Thread returned nonzero");
  }
  return 0;
}

int main(int argc, char **argv) {
#if defined(__native_client__)
  return srpc_main(argc, argv);
#else
  return llc_main(argc, argv);
#endif // __native_client__
}
