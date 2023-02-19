
#include <iostream>

#include <gtest/gtest.h>

#include "opcode/ppc.h"
#include "ppcdisasm/ppc-relocations.h"

#include "aipg/aipg.hpp"
#include "Udiv.hpp"
#include "LabelTest.hpp"

/*
original ASM:
lis     r3, 0x8889
lwz     r8, 0x14(r28)
addi    r0, r3, -0x7777
li      r3, 0
mulhw   r0, r0, r7
li      r3, 1
add     r0, r0, r7
srawi   r0, r0, 5
srwi    r5, r0, 0x1f
add     r6, r0, r5
*/
TEST(IdiomTest, Udiv) {
  uint32_t ins[] = {0x3c608889, 0x811c0014, 0x38038889, 0x38800000, 0x7c003896, 0x38600001, 0x7c003a14, 0x7c002e70, 0x54050ffe, 0x7cc02a14};
  aipg::Context parseCtx;
  bool match = aipg::matchUdiv(std::begin(ins), std::end(ins), PPC_OPCODE_PPC, parseCtx);

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

// same test as above with a li   r3, 0x18 as the second instruction to fail the register overwrite instruction constraint
TEST(IdiomTestWriteConstraintNegative, Udiv) {
  uint32_t ins[] = {0x3c608889, 0x38600018, 0x811c0014, 0x38038889, 0x38800000, 0x7c003896, 0x38600001, 0x7c003a14, 0x7c002e70, 0x54050ffe, 0x7cc02a14};
  aipg::Context parseCtx;
  bool match = aipg::matchUdiv(std::begin(ins), std::end(ins), PPC_OPCODE_PPC, parseCtx);
  
  EXPECT_FALSE(match);
}

/*
Test with relocations and labels

 805103F0 4182005C  beq-        lbl_8051044c
 805103F4 3CA0808B  lis         r5, lbl_808b2c10@ha
 805103F8 3C80809C  lis         r4, lbl_809bd6e0@ha
 805103FC 38A52C10  addi        r5, r5, lbl_808b2c10@l
 80510400 90A30000  stw         r5, 0(r3)
 80510404 8064D6E0  lwz         r3, lbl_809bd6e0@l(r4)
*/
TEST(LabelTestPositive, LatelTest) {
  uint32_t start_vma = 0x805103f0;
  uint32_t ins[] = {0x4182005c, 0x3ca0808b, 0x3c80809c, 0x38a52c10, 0x90A30000, 0x8064d6e0};
  SymbolGetter symGetter = [](uint32_t address) -> RelocationTarget {
    if (address == 0x805103f0) {
      return {R_PPC_ADDR14, "lbl_8051044c"};
    } else if (address == 0x805103f4) {
      return {R_PPC_ADDR16_HA, "lbl_808b2c10"};
    } else if (address == 0x805103f8) {
      return {R_PPC_ADDR16_HA, "lbl_809bd6e0"};
    } else if (address == 0x805103fc) {
      return {R_PPC_ADDR16_LO, "lbl_808b2c10"};
    } else if (address == 0x80510404) {
      return {R_PPC_ADDR16_LO, "lbl_809bd6e0"};
    } else {
      return RELOC_TARGET_NONE;
    }
  };
  aipg::Context parseCtx;
  bool match = aipg::matchLabelTest(std::begin(ins), std::end(ins), PPC_OPCODE_PPC, parseCtx, start_vma, symGetter);

  ASSERT_TRUE(match);

  ASSERT_EQ(parseCtx.matchInsIdxs.size(), 3);
  EXPECT_EQ(parseCtx.matchInsIdxs[0], 0);
  EXPECT_EQ(parseCtx.matchInsIdxs[1], 1);
  EXPECT_EQ(parseCtx.matchInsIdxs[2], 3);

  EXPECT_EQ(parseCtx.gprs[1], 5);
  EXPECT_EQ(parseCtx.gprs[2], 5);

  EXPECT_STREQ(parseCtx.labs[1].c_str(), "lbl_8051044c");
  EXPECT_STREQ(parseCtx.labs[2].c_str(), "lbl_808b2c10");
}

TEST(LabelTestNegative, LatelTest) {
  uint32_t start_vma = 0x805103f0;
  uint32_t ins[] = {0x4182005c, 0x3ca0808b, 0x3c80809c, 0x38a52c10, 0x90A30000, 0x8064d6e0};
  SymbolGetter symGetter = [](uint32_t address) -> RelocationTarget {
    if (address == 0x805103f0) {
      return {R_PPC_ADDR14, "lbl_8051044c"};
    } else if (address == 0x805103f4) {
      return {R_PPC_ADDR16_LO, "lbl_808b2c10"};
    } else if (address == 0x805103f8) {
      return {R_PPC_ADDR16_HA, "lbl_809bd6e0"};
    } else if (address == 0x805103fc) {
      return {R_PPC_ADDR16_LO, "lbl_808b2c10"};
    } else if (address == 0x80510404) {
      return {R_PPC_ADDR16_LO, "lbl_809bd6e0"};
    } else {
      return RELOC_TARGET_NONE;
    }
  };
  aipg::Context parseCtx;
  bool match = aipg::matchLabelTest(std::begin(ins), std::end(ins), PPC_OPCODE_PPC, parseCtx, start_vma, symGetter);

  ASSERT_FALSE(match);
}