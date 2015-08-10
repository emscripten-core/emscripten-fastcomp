
#include "simple_ast.h"
#include "optimizer.h"

#include <llvm/Support/raw_ostream.h>

void emscripten_optimizer(char *input, llvm::raw_pwrite_stream& Out) {
  //unsigned arenaState = arena.get();
  cashew::Parser<Ref, ValueBuilder> builder;
  Ref doc = builder.parseToplevel(input);
  eliminate(doc);
  simplifyExpressions(doc);
  JSPrinter jser(true, false, doc);
  jser.printAst();
  Out << jser.buffer << "\n";
  //arena.set(arenaState);
  //assert(arenaState == arena.get());
}

