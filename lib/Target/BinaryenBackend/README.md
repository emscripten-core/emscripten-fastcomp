Binaryen Backend
================

This generates code by translating LLVM IR into Binaryen IR, then optimizing in Binaryen, and then emitting WebAssembly.

Building
--------

From a subdirectory of the LLVM root (e.g. `./build`), run

        cmake .. -DCMAKE_BUILD_TYPE=Release -DLLVM_TARGETS_TO_BUILD="X86;JSBackend;BinaryenBackend" -DLLVM_INCLUDE_EXAMPLES=OFF -DLLVM_INCLUDE_TESTS=OFF -DCLANG_INCLUDE_EXAMPLES=OFF -DCLANG_INCLUDE_TESTS=OFF -DLLVM_ENABLE_ASSERTIONS=ON

You may want to remove assertions (the last command) for speed.

Then

        make


