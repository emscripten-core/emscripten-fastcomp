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

#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/NaCl.h"
#include "llvm/Assembly/PrintModulePass.h"
#include "llvm/Support/DataStream.h"
#include "llvm/CodeGen/CommandFlags.h"
#include "llvm/CodeGen/LinkAllAsmWriterComponents.h"
#include "llvm/CodeGen/LinkAllCodegenComponents.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/SubtargetFeature.h"
#include "llvm/Pass.h"
#include "llvm/PassManager.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FormattedStream.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/IRReader.h"
#include "llvm/Support/ManagedStatic.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Timer.h"
#include "llvm/Support/ToolOutputFile.h"
#include "llvm/Target/TargetLibraryInfo.h"
#include "llvm/Target/TargetMachine.h"
#include <memory>


using namespace llvm;

// @LOCALMOD-BEGIN
// NOTE: this tool can be build as a "sandboxed" translator.
//       There are two ways to build the translator
//       SRPC-style:  no file operations are allowed
//                    see nacl_file.cc for support code
//       non-SRPC-style: some basic file operations are allowed
//                       This can be useful for debugging but will
//                       not be deployed.
#if defined(__native_client__) && defined(NACL_SRPC)
int GetObjectFileFD();
// The following two functions communicate metadata to the SRPC wrapper for LLC.
void NaClRecordObjectInformation(bool is_shared, const std::string& soname);
void NaClRecordSharedLibraryDependency(const std::string& library_name);
DataStreamer* NaClBitcodeStreamer;
#endif
// @LOCALMOD-END

// @LOCALMOD-BEGIN
const char *TimeIRParsingGroupName = "LLVM IR Parsing";
const char *TimeIRParsingName = "Parse IR";

bool TimeIRParsingIsEnabled = false;
static cl::opt<bool,true>
EnableTimeIRParsing("time-ir-parsing", cl::location(TimeIRParsingIsEnabled),
                    cl::desc("Measure the time IR parsing takes"));
// @LOCALMOD-END

// General options for llc.  Other pass-specific options are specified
// within the corresponding llc passes, and target-specific options
// and back-end code generation options are specified with the target machine.
//
static cl::opt<std::string>
InputFilename(cl::Positional, cl::desc("<input bitcode>"), cl::init("-"));

static cl::opt<std::string>
OutputFilename("o", cl::desc("Output filename"), cl::value_desc("filename"));

static cl::opt<unsigned>
TimeCompilations("time-compilations", cl::Hidden, cl::init(1u),
                 cl::value_desc("N"),
                 cl::desc("Repeat compilation N times for timing"));

// Using bitcode streaming has a couple of ramifications. Primarily it means
// that the module in the file will be compiled one function at a time rather
// than the whole module. This allows earlier functions to be compiled before
// later functions are read from the bitcode but of course means no whole-module
// optimizations. For now, streaming is only supported for files and stdin.
static cl::opt<bool>
LazyBitcode("streaming-bitcode",
  cl::desc("Use lazy bitcode streaming for file inputs"),
  cl::init(false));

// The option below overlaps very much with bitcode streaming.
// We keep it separate because it is still experimental and we want
// to use it without changing the outside behavior which is especially
// relevant for the sandboxed case.
static cl::opt<bool>
ReduceMemoryFootprint("reduce-memory-footprint",
  cl::desc("Aggressively reduce memory used by llc"),
  cl::init(false));

static cl::opt<bool>
PNaClABIVerify("pnaclabi-verify",
  cl::desc("Verify PNaCl bitcode ABI before translating"),
  cl::init(false));
static cl::opt<bool>
PNaClABIVerifyFatalErrors("pnaclabi-verify-fatal-errors",
  cl::desc("PNaCl ABI verification errors are fatal"),
  cl::init(false));

// Determine optimization level.
static cl::opt<char>
OptLevel("O",
         cl::desc("Optimization level. [-O0, -O1, -O2, or -O3] "
                  "(default = '-O2')"),
         cl::Prefix,
         cl::ZeroOrMore,
         cl::init(' '));

static cl::opt<std::string>
TargetTriple("mtriple", cl::desc("Override target triple for module"));

cl::opt<bool> NoVerify("disable-verify", cl::Hidden,
                       cl::desc("Do not verify input module"));

cl::opt<bool>
DisableSimplifyLibCalls("disable-simplify-libcalls",
                        cl::desc("Disable simplify-libcalls"),
                        cl::init(false));

static int compileModule(char**, LLVMContext&);

// GetFileNameRoot - Helper function to get the basename of a filename.
static inline std::string
GetFileNameRoot(const std::string &InputFilename) {
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
                                         const char *ProgName) {
  // If we don't yet have an output filename, make one.
  if (OutputFilename.empty()) {
    if (InputFilename == "-")
      OutputFilename = "-";
    else {
      OutputFilename = GetFileNameRoot(InputFilename);

      switch (FileType) {
      case TargetMachine::CGFT_AssemblyFile:
        if (TargetName[0] == 'c') {
          if (TargetName[1] == 0)
            OutputFilename += ".cbe.c";
          else if (TargetName[1] == 'p' && TargetName[2] == 'p')
            OutputFilename += ".cpp";
          else
            OutputFilename += ".s";
        } else
          OutputFilename += ".s";
        break;
      case TargetMachine::CGFT_ObjectFile:
        if (OS == Triple::Win32)
          OutputFilename += ".obj";
        else
          OutputFilename += ".o";
        break;
      case TargetMachine::CGFT_Null:
        OutputFilename += ".null";
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
  unsigned OpenFlags = 0;
  if (Binary) OpenFlags |= raw_fd_ostream::F_Binary;
  tool_output_file *FDOut = new tool_output_file(OutputFilename.c_str(), error,
                                                 OpenFlags);
  if (!error.empty()) {
    errs() << error << '\n';
    delete FDOut;
    return 0;
  }

  return FDOut;
}

// main - Entry point for the llc compiler.
//
int llc_main(int argc, char **argv) {
  sys::PrintStackTraceOnErrorSignal();
  PrettyStackTraceProgram X(argc, argv);

  // Enable debug stream buffering.
  EnableDebugBuffering = true;

  LLVMContext &Context = getGlobalContext();
  llvm_shutdown_obj Y;  // Call llvm_shutdown() on exit.

  // Initialize targets first, so that --version shows registered targets.
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
// @LOCALMOD-BEGIN
// Prune asm parsing from sandboxed translator.
// Do not prune "AsmPrinters" because that includes
// the direct object emission.
 #if !defined(__native_client__)
   InitializeAllAsmParsers();
#endif
// @LOCALMOD-END

  // Initialize codegen and IR passes used by llc so that the -print-after,
  // -print-before, and -stop-after options work.
  PassRegistry *Registry = PassRegistry::getPassRegistry();
  initializeCore(*Registry);
  initializeCodeGen(*Registry);
  initializeLoopStrengthReducePass(*Registry);
  initializeLowerIntrinsicsPass(*Registry);
  initializeUnreachableBlockElimPass(*Registry);

  // Register the target printer for --version.
  cl::AddExtraVersionPrinter(TargetRegistry::printRegisteredTargetsForVersion);

  cl::ParseCommandLineOptions(argc, argv, "llvm system compiler\n");

  // Compile the module TimeCompilations times to give better compile time
  // metrics.
  for (unsigned I = TimeCompilations; I; --I)
    if (int RetVal = compileModule(argv, Context))
      return RetVal;
  return 0;
}

// @LOCALMOD-BEGIN
static void CheckABIVerifyErrors(PNaClABIErrorReporter &Reporter,
                                 const Twine &Name) {
  if (PNaClABIVerify && Reporter.getErrorCount() > 0) {
    errs() << (PNaClABIVerifyFatalErrors ? "ERROR: " : "WARNING: ");
    errs() << Name << " is not valid PNaCl bitcode:\n";
    Reporter.printErrors(errs());
    if (PNaClABIVerifyFatalErrors)
      exit(1);
  }
  Reporter.reset();
}
// @LOCALMOD-END

static int compileModule(char **argv, LLVMContext &Context) {
  // Load the module to be compiled...
  SMDiagnostic Err;
  std::auto_ptr<Module> M;
  Module *mod = 0;
  Triple TheTriple;

  bool SkipModule = MCPU == "help" ||
                    (!MAttrs.empty() && MAttrs.front() == "help");

  PNaClABIErrorReporter ABIErrorReporter; // @LOCALMOD

  // If user just wants to list available options, skip module loading
  if (!SkipModule) {
    // @LOCALMOD-BEGIN
#if defined(__native_client__) && defined(NACL_SRPC)
    if (LazyBitcode) {
      std::string StrError;
      M.reset(getStreamedBitcodeModule(
          std::string("<SRPC stream>"),
          NaClBitcodeStreamer, Context, &StrError));
      if (!StrError.empty()) {
        Err = SMDiagnostic(InputFilename, SourceMgr::DK_Error, StrError);
      }
    } else {
      // Avoid using ParseIRFile to avoid pulling in the LLParser.
      // Only handle binary bitcode.
      llvm_unreachable("native client SRPC only supports streaming");
    }
#else
    {
      // @LOCALMOD: timing is temporary, until it gets properly added upstream
      NamedRegionTimer T(TimeIRParsingName, TimeIRParsingGroupName,
                         TimeIRParsingIsEnabled);
      M.reset(ParseIRFile(InputFilename, Err, Context));
    }
#endif
    // @LOCALMOD-END

    mod = M.get();
    if (mod == 0) {
      Err.print(argv[0], errs());
      return 1;
    }

    // @LOCALMOD-BEGIN
    if (PNaClABIVerify) {
      // Verify the module (but not the functions yet)
      ModulePass *VerifyPass = createPNaClABIVerifyModulePass(&ABIErrorReporter);
      VerifyPass->runOnModule(*mod);
      CheckABIVerifyErrors(ABIErrorReporter, "Module");
    }
#if defined(__native_client__) && defined(NACL_SRPC)
    // Record that this isn't a shared library.
    // TODO(eliben): clean this up more once the pnacl-llc switch-over is
    // working.
    NaClRecordObjectInformation(false, mod->getSOName());

    // To determine if we should compile PIC or not, we needed to load at
    // least the metadata. Since we've already constructed the commandline,
    // we have to hack this in after commandline processing.
    if (mod->getOutputFormat() == Module::SharedOutputFormat) {
      RelocModel = Reloc::PIC_;
    }
    // Also set PIC_ for dynamic executables:
    // BUG= http://code.google.com/p/nativeclient/issues/detail?id=2351
    if (mod->lib_size() > 0) {
      RelocModel = Reloc::PIC_;
    }
#endif  // defined(__native_client__) && defined(NACL_SRPC)
    // @LOCALMOD-END

    // If we are supposed to override the target triple, do so now.
    if (!TargetTriple.empty())
      mod->setTargetTriple(Triple::normalize(TargetTriple));
    TheTriple = Triple(mod->getTargetTriple());
  } else {
    TheTriple = Triple(Triple::normalize(TargetTriple));
  }

  if (TheTriple.getTriple().empty())
    TheTriple.setTriple(sys::getDefaultTargetTriple());

  // Get the target specific parser.
  std::string Error;
  const Target *TheTarget = TargetRegistry::lookupTarget(MArch, TheTriple,
                                                         Error);
  if (!TheTarget) {
    errs() << argv[0] << ": " << Error;
    return 1;
  }

  // Package up features to be passed to target/subtarget
  std::string FeaturesStr;
  if (MAttrs.size()) {
    SubtargetFeatures Features;
    // @LOCALMOD-BEGIN
    // Use the same default attribute settings as libLTO.
    // TODO(pdox): Figure out why this isn't done for upstream llc.
    Features.getDefaultSubtargetFeatures(TheTriple);
    // @LOCALMOD-END
    for (unsigned i = 0; i != MAttrs.size(); ++i)
      Features.AddFeature(MAttrs[i]);
    FeaturesStr = Features.getString();
  }

  CodeGenOpt::Level OLvl = CodeGenOpt::Default;
  switch (OptLevel) {
  default:
    errs() << argv[0] << ": invalid optimization level.\n";
    return 1;
  case ' ': break;
  case '0': OLvl = CodeGenOpt::None; break;
  case '1': OLvl = CodeGenOpt::Less; break;
  case '2': OLvl = CodeGenOpt::Default; break;
  case '3': OLvl = CodeGenOpt::Aggressive; break;
  }

  TargetOptions Options;
  Options.LessPreciseFPMADOption = EnableFPMAD;
  Options.NoFramePointerElim = DisableFPElim;
  Options.NoFramePointerElimNonLeaf = DisableFPElimNonLeaf;
  Options.AllowFPOpFusion = FuseFPOps;
  Options.UnsafeFPMath = EnableUnsafeFPMath;
  Options.NoInfsFPMath = EnableNoInfsFPMath;
  Options.NoNaNsFPMath = EnableNoNaNsFPMath;
  Options.HonorSignDependentRoundingFPMathOption =
      EnableHonorSignDependentRoundingFPMath;
  Options.UseSoftFloat = GenerateSoftFloatCalls;
  if (FloatABIForCalls != FloatABI::Default)
    Options.FloatABIType = FloatABIForCalls;
  Options.NoZerosInBSS = DontPlaceZerosInBSS;
  Options.GuaranteedTailCallOpt = EnableGuaranteedTailCallOpt;
  Options.DisableTailCalls = DisableTailCalls;
  Options.StackAlignmentOverride = OverrideStackAlignment;
  Options.RealignStack = EnableRealignStack;
  Options.TrapFuncName = TrapFuncName;
  Options.PositionIndependentExecutable = EnablePIE;
  Options.EnableSegmentedStacks = SegmentedStacks;
  Options.UseInitArray = UseInitArray;
  Options.SSPBufferSize = SSPBufferSize;

  std::auto_ptr<TargetMachine>
    target(TheTarget->createTargetMachine(TheTriple.getTriple(),
                                          MCPU, FeaturesStr, Options,
                                          RelocModel, CMModel, OLvl));
  assert(target.get() && "Could not allocate target machine!");
  assert(mod && "Should have exited after outputting help!");
  TargetMachine &Target = *target.get();

  if (DisableDotLoc)
    Target.setMCUseLoc(false);

  if (DisableCFI)
    Target.setMCUseCFI(false);

  if (EnableDwarfDirectory)
    Target.setMCUseDwarfDirectory(true);

  if (GenerateSoftFloatCalls)
    FloatABIForCalls = FloatABI::Soft;

  // Disable .loc support for older OS X versions.
  if (TheTriple.isMacOSX() &&
      TheTriple.isMacOSXVersionLT(10, 6))
    Target.setMCUseLoc(false);

#if !defined(NACL_SRPC)
  // Figure out where we are going to send the output.
  OwningPtr<tool_output_file> Out
    (GetOutputStream(TheTarget->getName(), TheTriple.getOS(), argv[0]));
  if (!Out) return 1;
#endif

  // Build up all of the passes that we want to do to the module.
  // @LOCALMOD-BEGIN
  OwningPtr<PassManagerBase> PM;
  if (LazyBitcode || ReduceMemoryFootprint)
    PM.reset(new FunctionPassManager(mod));
  else
    PM.reset(new PassManager());

  // Add the ABI verifier pass before the analysis and code emission passes.
  FunctionPass *FunctionVerifyPass = NULL;
  if (PNaClABIVerify) {
    FunctionVerifyPass = createPNaClABIVerifyFunctionsPass(&ABIErrorReporter);
    PM->add(FunctionVerifyPass);
  }
  // @LOCALMOD-END

  // Add an appropriate TargetLibraryInfo pass for the module's triple.
  TargetLibraryInfo *TLI = new TargetLibraryInfo(TheTriple);
  if (DisableSimplifyLibCalls)
    TLI->disableAllFunctions();
  PM->add(TLI);

  // Add intenal analysis passes from the target machine.
  Target.addAnalysisPasses(*PM.get());

  // Add the target data from the target machine, if it exists, or the module.
  if (const DataLayout *TD = Target.getDataLayout())
    PM->add(new DataLayout(*TD));
  else
    PM->add(new DataLayout(mod));

  // Override default to generate verbose assembly.
  Target.setAsmVerbosityDefault(true);

  if (RelaxAll) {
    if (FileType != TargetMachine::CGFT_ObjectFile)
      errs() << argv[0]
             << ": warning: ignoring -mc-relax-all because filetype != obj";
    else
      Target.setMCRelaxAll(true);
  }


#if defined __native_client__ && defined(NACL_SRPC)
  {
    raw_fd_ostream ROS(GetObjectFileFD(), true);
    ROS.SetBufferSize(1 << 20);
    formatted_raw_ostream FOS(ROS);

    // Ask the target to add backend passes as necessary.
    if (Target.addPassesToEmitFile(*PM, FOS, FileType, NoVerify)) {
      errs() << argv[0] << ": target does not support generation of this"
             << " file type!\n";
      return 1;
    }

    if (LazyBitcode || ReduceMemoryFootprint) {
      FunctionPassManager* P = static_cast<FunctionPassManager*>(PM.get());
      P->doInitialization();
      for (Module::iterator I = mod->begin(), E = mod->end(); I != E; ++I) {
        P->run(*I);
        CheckABIVerifyErrors(ABIErrorReporter, "Function " + I->getName());
        if (ReduceMemoryFootprint) {
          I->Dematerialize();
        }
      }
      P->doFinalization();
    } else {
      static_cast<PassManager*>(PM.get())->run(*mod);
    }
    FOS.flush();
    ROS.flush();
  }
#else

  {
    formatted_raw_ostream FOS(Out->os());

    AnalysisID StartAfterID = 0;
    AnalysisID StopAfterID = 0;
    const PassRegistry *PR = PassRegistry::getPassRegistry();
    if (!StartAfter.empty()) {
      const PassInfo *PI = PR->getPassInfo(StartAfter);
      if (!PI) {
        errs() << argv[0] << ": start-after pass is not registered.\n";
        return 1;
      }
      StartAfterID = PI->getTypeInfo();
    }
    if (!StopAfter.empty()) {
      const PassInfo *PI = PR->getPassInfo(StopAfter);
      if (!PI) {
        errs() << argv[0] << ": stop-after pass is not registered.\n";
        return 1;
      }
      StopAfterID = PI->getTypeInfo();
    }

    // Ask the target to add backend passes as necessary.
    if (Target.addPassesToEmitFile(*PM, FOS, FileType, NoVerify,
                                   StartAfterID, StopAfterID)) {
      errs() << argv[0] << ": target does not support generation of this"
             << " file type!\n";
      return 1;
    }

    // Before executing passes, print the final values of the LLVM options.
    cl::PrintOptionValues();

    if (LazyBitcode || ReduceMemoryFootprint) {
      FunctionPassManager *P = static_cast<FunctionPassManager*>(PM.get());
      P->doInitialization();
      for (Module::iterator I = mod->begin(), E = mod->end(); I != E; ++I) {
        P->run(*I);
        CheckABIVerifyErrors(ABIErrorReporter, "Function " + I->getName());
        if (ReduceMemoryFootprint) {
          I->Dematerialize();
        }
      }
      P->doFinalization();
    } else {
      static_cast<PassManager*>(PM.get())->run(*mod);
    }
  }

  // Declare success.
  Out->keep();
#endif

  return 0;
}

#if !defined(NACL_SRPC)
int
main (int argc, char **argv) {
  return llc_main(argc, argv);
}
#else
// main() is in nacl_file.cpp.
#endif
