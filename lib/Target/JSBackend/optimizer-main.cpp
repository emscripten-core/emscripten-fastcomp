
#include <mutex>

#include "simple_ast.h"
#include "optimizer.h"

#include <llvm/Support/raw_ostream.h>

static std::mutex printMutex;

void emscripten_optimizer(char *input, llvm::raw_pwrite_stream& Out) {
  //unsigned arenaState = arena.get();
  //llvm::errs() << "optimizer about to work on:\n[START]\n" << input << "\n[STOP]\n";
  cashew::Parser<Ref, ValueBuilder> builder;
  Ref doc = builder.parseToplevel(input);
  eliminate(doc);
  simplifyExpressions(doc);
  JSPrinter jser(true, false, doc);
  jser.printAst();
  {
    std::lock_guard<std::mutex> lock(printMutex);
    Out << jser.buffer << "\n"; // XXX is this actually needed? perhaps raw_pwrite_stream is threadsafe?
  }
  //arena.set(arenaState);
  //assert(arenaState == arena.get());
}

