
#include <iostream>

#include "opcode/ppc.h"

#include "aipg/aipg.hpp"
#include "Udiv.hpp"

int main(int argc, char** argv) {
  uint32_t ins[] = {0x3c608889, 0x811c0014, 0x38038889, 0x38800000, 0x7c003896, 0x38600001, 0x7c003a14, 0x7c002e70, 0x54050ffe, 0x7cc02a14};
  aipg::Context parseCtx;
  bool match = aipg::matchUdiv(ins, sizeof(ins)/sizeof(uint32_t), PPC_OPCODE_PPC, parseCtx);
  std::cout << (match ? "Found match" : "No match found") << std::endl;
  for (uint32_t idx : parseCtx.matchInsIdxs)
    std::cout << "Matcing ins idx: " << idx << std::endl;
  for (auto& it : parseCtx.gprs) {
    std::cout << "GPR var " << it.first << " value is r" << it.second << std::endl;
  }
  for (auto& it : parseCtx.imms) {
    std::cout << "Immediate var " << it.first << " value is " << it.second << std::endl;
  }
}