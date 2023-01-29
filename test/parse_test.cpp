
#include <iostream>

#include <gtest/gtest.h>

#include "opcode/ppc.h"

#include "aipg/aipg.hpp"
#include "Udiv.hpp"

TEST(IdiomTest, Udiv) {
  uint32_t ins[] = {0x3c608889, 0x811c0014, 0x38038889, 0x38800000, 0x7c003896, 0x38600001, 0x7c003a14, 0x7c002e70, 0x54050ffe, 0x7cc02a14};
  aipg::Context parseCtx;
  bool match = aipg::matchUdiv(ins, sizeof(ins)/sizeof(uint32_t), PPC_OPCODE_PPC, parseCtx);

  ASSERT_TRUE(match);

  ASSERT_EQ(parseCtx.matchInsIdxs.size(), 4);
  EXPECT_EQ(parseCtx.matchInsIdxs[0], 0);
  EXPECT_EQ(parseCtx.matchInsIdxs[1], 2);
  EXPECT_EQ(parseCtx.matchInsIdxs[2], 4);
  EXPECT_EQ(parseCtx.matchInsIdxs[3], 7);

  EXPECT_EQ(parseCtx.gprs[1], 7);
  EXPECT_EQ(parseCtx.gprs[2], 0);
  EXPECT_EQ(parseCtx.gprs[3], 0);
  EXPECT_EQ(parseCtx.gprs[4], 0);
  EXPECT_EQ(parseCtx.gprs[8], 0);
  EXPECT_EQ(parseCtx.gprs[9], 3);

  EXPECT_EQ(parseCtx.imms[1], -30583);
  EXPECT_EQ(parseCtx.imms[2], -30583);
  EXPECT_EQ(parseCtx.imms[3], 5);
}