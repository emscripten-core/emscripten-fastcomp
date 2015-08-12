
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
  simplifyIfs(doc);
  registerize(doc);

  static thread_local char *buffer = nullptr;
  static thread_local int size = 0;
  JSPrinter jser(true, false, doc);
  jser.reserve(buffer, size);
  jser.printAst();
  jser.preserve(buffer, size);

  {
    std::lock_guard<std::mutex> lock(printMutex);
    Out << jser.buffer << "\n";
  }
  //arena.set(arenaState);
  //assert(arenaState == arena.get());
}

