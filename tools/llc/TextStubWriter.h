#ifndef __TEXT_STUB_WRITER_H
#define __TEXT_STUB_WRITER_H

#include "ELFStub.h"

namespace llvm {

void WriteTextELFStub(const ELFStub *Stub, std::string *output);

}

#endif
