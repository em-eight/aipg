
//#include "aipg/aipg.hpp"

#include <sstream>
#include <regex>
#include <vector>

// ppcdisasm-cpp
#include "opcode/ppc.h"
#include "ppcdisasm/ppc-dis.hpp"
#include "ppcdisasm/ppc-relocations.h"

// inja
#include <inja/inja.hpp>

#define STR(N) std::to_string(N)

// consume any asm line
#define CONSUME_ASM_PTRN "..."

// insn mnemonic
#define MNEMONIC_PTRN "[a-zA-Z][a-zA-Z0-9.+\\-]{0,20}"
// insn operand
#define OPERAND_PTRN "\\$?\\w+"

// GPR variable
#define VAR_GPR_PTRN "\\$GPR(\\d*)"
// wildcard (any) GPR
#define WLD_GPR_PTRN "\\$GPR\\?"
// defined GPR
#define DEF_GPR_PTRN "r(\\d\\d?)"

// FPR variable
#define VAR_FPR_PTRN "\\$FPR(\\d*)"
// wildcard (any) FPR
#define WLD_FPR_PTRN "\\$FPR\\?"
// defined FPR
#define DEF_FPR_PTRN "f(\\d\\d?)"
#define FPR_PTRN "(?:" VAR_FPR_PTRN "|" DEF_FPR_PTRN ")"

#define HEX_NATURALNUM_LITERAL_PTRN "0[xX][0-9a-fA-F]+"
#define DEC_NATURALNUM_LITERAL_PTRN "\\d+"
// defined immediate
#define DEF_IMM_PTRN "-?(?:" HEX_NATURALNUM_LITERAL_PTRN "|" DEC_NATURALNUM_LITERAL_PTRN ")"
// immediate variable
#define VAR_IMM_PTRN "\\$IMM(\\d*)"
// wildcard (any) immediate
#define WLD_IMM_PTRN "\\$IMM\\?"

// TODO: figure out how to support relocs to support label/reloc idiom parsing (match target address vs match label string)
#if 1
// label (aka valid C identifier)
#define DEF_LABEL_PTRN "[_a-zA-Z][_a-zA-Z0-9]{0,30}"
#define VAR_LABEL_PTRN "\\$LAB(\\d*)"
#define WLD_LABEL_PTRN "\\$LAB\\?"
#endif

#define RELOC_ADDR16_LO 4
#define RELOC_ADDR16_HI 5
#define RELOC_ADDR16_HA 6
#define RELOC_EMB_SDA21 109

// register specifiers
#define READ_SPEC_LIST_PTRN "\\^?\\{([\\$\\w,\\s]+)\\}"
#define WRITE_SPEC_LIST_PTRN "\\^?\\[([\\$\\w,\\s]+)\\]"

const std::regex EMPTY_RE("\\s");
const std::regex CONSUME_ASM_RE(CONSUME_ASM_PTRN);
const std::regex MNEMONIC_RE(MNEMONIC_PTRN);
const std::regex OPERAND_RE(OPERAND_PTRN);
const std::regex VAR_GPR_RE(VAR_GPR_PTRN);
const std::regex WLD_GPR_RE(WLD_GPR_PTRN);
const std::regex DEF_GPR_RE(DEF_GPR_PTRN);
const std::regex VAR_FPR_RE(VAR_FPR_PTRN);
const std::regex WLD_FPR_RE(WLD_FPR_PTRN);
const std::regex DEF_FPR_RE(DEF_FPR_PTRN);
const std::regex VAR_IMM_RE(VAR_IMM_PTRN);
const std::regex WLD_IMM_RE(WLD_IMM_PTRN);
const std::regex DEF_IMM_RE(DEF_IMM_PTRN);
const std::regex VAR_LAB_RE(VAR_LABEL_PTRN);
const std::regex WLD_LAB_RE(WLD_LABEL_PTRN);
const std::regex DEF_LAB_RE(DEF_LABEL_PTRN);
const std::regex READ_SPEC_LIST_RE(READ_SPEC_LIST_PTRN);
const std::regex WRITE_SPEC_LIST_RE(WRITE_SPEC_LIST_PTRN);

inja::Environment injaEnv {TEMPLATES_DIR};
const inja::Template sourceTemplate = injaEnv.parse_template("/source.j2");
const inja::Template includeTemplate = injaEnv.parse_template("/header.j2");
const inja::Template insCheckSingleTemplate = injaEnv.parse_template("/insCheckSingle.j2");
const inja::Template insCheckLoopTemplate = injaEnv.parse_template("/insCheckLoop.j2");
const inja::Template isInsMatchingTemplate = injaEnv.parse_template("/isInsnMatching.j2");
const inja::Template isMnemonicMatchingTemplate = injaEnv.parse_template("/isMnemonicMatching.j2");
const inja::Template hasOperandOptionalValueTemplate = injaEnv.parse_template("/hasOperandOptionalValue.j2");
const inja::Template isVariableGprMatchingTemplate = injaEnv.parse_template("/isVariableGprMatching.j2");
const inja::Template isDefinedGprMatchingTemplate = injaEnv.parse_template("/isDefinedGprMatching.j2");
const inja::Template isVariableFprMatchingTemplate = injaEnv.parse_template("/isVariableFprMatching.j2");
const inja::Template isDefinedFprMatchingTemplate = injaEnv.parse_template("/isDefinedFprMatching.j2");
const inja::Template isVariableImmMatchingTemplate = injaEnv.parse_template("/isVariableImmMatching.j2");
const inja::Template isDefinedImmMatchingTemplate = injaEnv.parse_template("/isDefinedImmMatching.j2");
const inja::Template isVariableLabMatchingTemplate = injaEnv.parse_template("/isVariableLabMatching.j2");
const inja::Template isDefinedLabMatchingTemplate = injaEnv.parse_template("/isDefinedLabMatching.j2");

using json = nlohmann::json;

struct powerpc_opcode* lookup_mnemonic(const std::string& mnemonic) {
  for (struct powerpc_opcode* op = (struct powerpc_opcode*) powerpc_opcodes; op < powerpc_opcodes + powerpc_num_opcodes; op++) {
    if (mnemonic == op->name)
      return op;
  }
  return nullptr;
}

void parseRelocIfExists(json& operand_json, const std::string& suffix) {
    std::smatch reloc_match;
    if (suffix[0] == '@') {
      std::string reloc_name = suffix.substr(1);
      if (std::regex_match(reloc_name, reloc_match, DEF_LAB_RE)) {
        if (reloc_match[0] == "ha") {
          operand_json["relocKind"] = R_PPC_ADDR16_HA;
        } else if (reloc_match[0] == "h") {
          operand_json["relocKind"] = R_PPC_ADDR16_HI;
        } else if (reloc_match[0] == "l") {
          operand_json["relocKind"] = R_PPC_ADDR16_LO;
        } else if (reloc_match[0] == "sda21") {
          operand_json["relocKind"] = R_PPC_EMB_SDA21;
        } else {
          std::cout << "Unknown reloc specifier " << reloc_match[0] << std::endl;
          exit(-1);
        }
      } else {
        std::cout << "Failed to parse reloc specifier at " << reloc_name << std::endl;
        exit(-1);
      }
    }
}

// I wholeheartedly trust this excerpt from gas' gas/tc-ppc.c for detecting if optional operands are skipped
// https://chromium.googlesource.com/chromiumos/third_party/binutils/+/refs/heads/firmware-samus-6300.B/gas/config/tc-ppc.c#2663
bool skip_optional(char* line, const powerpc_opcode* opcode) {
  char *s;
  bool skip_optional = false;
  const ppc_opindex_t* opindex_ptr = opcode->operands;
  for (opindex_ptr = opcode->operands; *opindex_ptr != 0; opindex_ptr++) {
    const struct powerpc_operand *operand;
    operand = &powerpc_operands[*opindex_ptr];
    if ((operand->flags & PPC_OPERAND_OPTIONAL) != 0) {
      unsigned int opcount;
      unsigned int num_operands_expected;
      unsigned int i;
      /* There is an optional operand.  Count the number of
        commas in the input line.  */
      if (*line == '\0')
        opcount = 0;
      else {
        opcount = 1;
        s = line;
        while ((s = strchr (s, ',')) != (char *) NULL) {
          ++opcount;
          ++s;
        }
      }
      /* Compute the number of expected operands.
        Do not count fake operands.  */
      for (num_operands_expected = 0, i = 0; opcode->operands[i]; i++)
        ++num_operands_expected;
      /* If there are fewer operands in the line then are called
        for by the instruction, we want to skip the optional
        operands.  */
      if (opcount < num_operands_expected)
        skip_optional = true;
      break;
    }
  }
  return skip_optional;
}

namespace aipg {
std::tuple<std::string, std::string> generateParser(const std::string& idiom, const std::string& idiom_name) {
  // read idiom line by line
  std::istringstream iss(idiom);
  std::string line;
  int lineNum = 0;

  json source_data;
  source_data["ins_data"] = json::array();
  source_data["definitions"] = json::array();
  source_data["idiom_name"] = idiom_name;
  json include_data;
  include_data["idiom_name"] = idiom_name;
  std::vector<std::string> definitions;
  definitions.push_back(injaEnv.render(isMnemonicMatchingTemplate, source_data));
  std::vector<std::string> parserChecks;

  // flag for ... expression to generate runtime that repeatedly checks for pattern
  bool checkNextRepeated = false;
  json ins_constraints;
  auto clear_ins_constraints = [&ins_constraints]() {
    ins_constraints["gprWriteConstraints"] = json::array();
    ins_constraints["gprReadConstraints"] = json::array();
    ins_constraints["fprWriteConstraints"] = json::array();
    ins_constraints["fprReadConstraints"] = json::array();
  };
  clear_ins_constraints();

  bool isInMultiLineComment = false;
  while (std::getline(iss, line)) {
    lineNum++;
    // ignore inline comment
    int commentIdx = line.find("//");
    if (commentIdx != std::string::npos) {
      line.resize(commentIdx);
    }
    // ignore multiline comment
    int multiCommentIdx = line.find("/*");
    if (multiCommentIdx != std::string::npos) {
      line.resize(multiCommentIdx);
      isInMultiLineComment = true;
    }
    if (isInMultiLineComment) {
      int multiCommentEndIdx = line.find("*/");
      if (multiCommentEndIdx != std::string::npos) {
        line = line.substr(multiCommentEndIdx);
        isInMultiLineComment = false;
      } else continue;
    }
    if (std::all_of(line.begin(),line.end(),isspace)) continue; // ignore empty lines

    std::smatch mnemonic_match;
    std::smatch dummy_match;
    std::string tmp; // hack because match does not work with temp strings
    // mnemonic at the start of line ?
    if (std::regex_search(line, mnemonic_match, MNEMONIC_RE) && mnemonic_match.prefix().str() == "") {
      // -------- Assembly line --------
      std::string mnemonic = mnemonic_match[0];
      struct powerpc_opcode* opcode = lookup_mnemonic(mnemonic);
      if (opcode == nullptr) {
        std::cerr << "Unknown mnemonic" << mnemonic << " at line " << lineNum << std::endl;
        std::cerr << ">> " << line << std::endl;
        exit(-1);
      }
      json ins_data;
      ins_data["lineNo"] = lineNum;
      ins_data["idiom_name"] = idiom_name;
      ins_data["operands"] = json::array();
      ins_data["opindex"] = opcode - powerpc_opcodes;

      std::string operands = mnemonic_match.suffix();
      bool skips_optional_operands = skip_optional(const_cast<char*>(line.c_str()), opcode);
      int num_optional = 0; // (negative) number of optional arguments in this instruction (needed for checking optional operands)

      // parse operands
      const struct powerpc_operand *operand;
      const ppc_opindex_t *opindex;
      for (opindex = opcode->operands; *opindex != 0; opindex++) {
        operand = powerpc_operands + *opindex;
        json operand_data;
        operand_data["idx"] = *opindex;
        json opData; // container of operand_data for operand matching templates
        opData["lineNo"] = lineNum;
        opData["idiom_name"] = idiom_name;

        // match next operand (word) (maybe check if the asm is properly formatted and not just go to next operand?)
        std::smatch operand_match;
        std::string operand_string;
        if ((operand->flags & PPC_OPERAND_OPTIONAL) != 0 && skips_optional_operands) { // TODO: Support OPERAND_NEXT for 5 arg rotate-mask instructions
          num_optional--;
          operand_data["isSkippedOptional"] = true;
          operand_data["num_optional"] = num_optional;
          opData["operand"] = operand_data;
          definitions.push_back(injaEnv.render(hasOperandOptionalValueTemplate, opData));
          ins_data["operands"].push_back(operand_data);
          continue;
        } else if (std::regex_search(operands, operand_match, OPERAND_RE)) {
          operand_string = operand_match[0];
          operands = operand_match.suffix();
          operand_data["isSkippedOptional"] = false;
        } else {
          std::cerr << "Expected more operands at line " << lineNum << std::endl;
          exit(-1);
        }

        std::smatch operand_type_match;
        if ((operand->flags & PPC_OPERAND_GPR) != 0 ||
             (operand->flags & PPC_OPERAND_GPR_0) != 0) {
          // GPR
          if (std::regex_match(operand_string, operand_type_match, VAR_GPR_RE)) {
            try {
              uint32_t gpr = std::stoi(operand_type_match[1]);
              operand_data["gpr"] = gpr;
            } catch (std::invalid_argument iae) {
              std::cerr << "Invalid GPR variable expression at line " << lineNum << ", " << operand_string << std::endl;
              exit(-1);
            }

            opData["operand"] = operand_data;
            definitions.push_back(injaEnv.render(isVariableGprMatchingTemplate, opData));
            ins_data["operands"].push_back(operand_data);
          } else if (std::regex_match(operand_string, operand_type_match, WLD_GPR_RE)) {
            // no runtime check is added
          } else if (std::regex_match(operand_string, operand_type_match, DEF_GPR_RE)) {
            try {
              uint32_t gpr = std::stoi(operand_type_match[1]);
              operand_data["gpr"] = gpr;
            } catch (std::invalid_argument iae) {
              std::cerr << "Invalid defined GPR expression at line " << lineNum << ", " << operand_string << std::endl;
              exit(-1);
            }

            opData["operand"] = operand_data;
            definitions.push_back(injaEnv.render(isDefinedGprMatchingTemplate, opData));
            ins_data["operands"].push_back(operand_data);
          } else {
            std::cerr << "Expected mandatory GPR expression at line " << lineNum << ", got " << operand_string << " instead" << std::endl;
            exit(-1);
          }
        } else if ((operand->flags & PPC_OPERAND_FPR) != 0) {
          // FPR
          if (std::regex_match(operand_string, operand_type_match, VAR_FPR_RE)) {
            try {
              uint32_t fpr = std::stoi(operand_type_match[1]);
              operand_data["fpr"] = fpr;
            } catch (std::invalid_argument iae) {
              std::cerr << "Invalid FPR variable expression at line " << lineNum << ", " << operand_string << std::endl;
              exit(-1);
            }

            opData["operand"] = operand_data;
            definitions.push_back(injaEnv.render(isVariableFprMatchingTemplate, opData));
            ins_data["operands"].push_back(operand_data);
          } else if (std::regex_match(operand_string, operand_type_match, WLD_FPR_RE)) {
            // no runtime check is added
          } else if (std::regex_match(operand_string, operand_type_match, DEF_FPR_RE)) {
            try {
              uint32_t fpr = std::stoi(operand_type_match[1]);
              operand_data["fpr"] = fpr;
            } catch (std::invalid_argument iae) {
              std::cerr << "Invalid defined FPR expression at line " << lineNum << ", " << operand_string << std::endl;
              exit(-1);
            }

            opData["operand"] = operand_data;
            definitions.push_back(injaEnv.render(isDefinedFprMatchingTemplate, opData));
            ins_data["operands"].push_back(operand_data);
          } else {
            std::cerr << "Expected mandatory FPR expression at line " << lineNum << ", got " << operand_string << " instead" << std::endl;
            exit(-1);
          }
        } else {
          // immediate or label/address (TODO)
          if (std::regex_match(operand_string, operand_type_match, VAR_LAB_RE)) {
            try {
              uint32_t lab = std::stoi(operand_type_match[1]);
              operand_data["lab"] = lab;
              parseRelocIfExists(operand_data, operands);
            } catch (std::invalid_argument iae) {
              std::cerr << "Invalid label expression at line " << lineNum << ", " << operand_string << std::endl;
              exit(-1);
            }

            opData["operand"] = operand_data;
            definitions.push_back(injaEnv.render(isVariableLabMatchingTemplate, opData));
            ins_data["operands"].push_back(operand_data);
          } else if (std::regex_match(operand_string, operand_type_match, WLD_LAB_RE)) {
            // no runtime check is added
          } else if (std::regex_match(operand_string, operand_type_match, DEF_LAB_RE)) {
            try {
              uint32_t lab = std::stoi(operand_type_match[1]);
              operand_data["lab"] = lab;
              parseRelocIfExists(operand_data, operands);
            } catch (std::invalid_argument iae) {
              std::cerr << "Invalid defined immediate expression at line " << lineNum << ", " << operand_string << std::endl;
              exit(-1);
            }

            opData["operand"] = operand_data;
            definitions.push_back(injaEnv.render(isDefinedLabMatchingTemplate, opData));
            ins_data["operands"].push_back(operand_data);
          } else if (std::regex_match(operand_string, operand_type_match, VAR_IMM_RE)) {
            try {
              uint32_t imm = std::stoi(operand_type_match[1]);
              operand_data["imm"] = imm;
            } catch (std::invalid_argument iae) {
              std::cerr << "Invalid immediate variable expression at line " << lineNum << ", " << operand_string << std::endl;
              exit(-1);
            }

            opData["operand"] = operand_data;
            definitions.push_back(injaEnv.render(isVariableImmMatchingTemplate, opData));
            ins_data["operands"].push_back(operand_data);
          } else if (std::regex_match(operand_string, operand_type_match, WLD_IMM_RE)) {
            // no runtime check is added
          } else if (std::regex_match(operand_string, operand_type_match, DEF_IMM_RE)) {
            try {
              uint32_t imm = std::stoi(operand_type_match[1]);
              operand_data["imm"] = imm;
            } catch (std::invalid_argument iae) {
              std::cerr << "Invalid defined immediate expression at line " << lineNum << ", " << operand_string << std::endl;
              exit(-1);
            }

            opData["operand"] = operand_data;
            definitions.push_back(injaEnv.render(isDefinedImmMatchingTemplate, opData));
            ins_data["operands"].push_back(operand_data);
          } else {
            std::cerr << "Expected mandatory immediate expression or label at line " << lineNum << ", got " << operand_string << " instead" << std::endl;
            exit(-1);
          }
        }
      } // end of operand matching loop

      definitions.push_back(injaEnv.render(isInsMatchingTemplate, ins_data));

      if (checkNextRepeated) {
        ins_data["ins_constraints"] = ins_constraints;
        ins_data["parseCheck"] = injaEnv.render(insCheckLoopTemplate, ins_data);
      } else {
        ins_data["parseCheck"] = injaEnv.render(insCheckSingleTemplate, ins_data);
      }
      source_data["ins_data"].push_back(ins_data);

      checkNextRepeated = false;
      clear_ins_constraints();
    } else if (std::regex_search(line, mnemonic_match, CONSUME_ASM_RE)) {
      // -------- Consume any asm line --------
      checkNextRepeated = true;
      
      std::string restOfLine = mnemonic_match.suffix();
      // record operand constraints
      std::smatch constraints_match;
      std::string constraints_string;
      while (std::regex_search(restOfLine, constraints_match, READ_SPEC_LIST_RE) || std::regex_search(restOfLine, constraints_match, WRITE_SPEC_LIST_RE)) {
        constraints_string = constraints_match[0];
        bool isNegative = constraints_string[0] == '^';
        bool isRead = constraints_string.find('{') != std::string::npos;
        restOfLine = constraints_match.suffix();

        std::smatch constraint_match;
        while (std::regex_search(constraints_string, constraint_match, OPERAND_RE)) {
          std::string constraint_string = constraint_match[0];
          constraints_string = constraint_match.suffix();
          json constraint;
          constraint["isNotAllowed"] = isNegative;
          constraint["isRead"] = isRead;
          if (std::regex_match(constraint_string, constraint_match, VAR_GPR_RE)) {
            constraint["val"] = std::stoi(constraint_match[1]);
            constraint["isVariable"] = true;
            constraint["type"] = "gpr";
          } else if (std::regex_match(constraint_string, constraint_match, DEF_GPR_RE)) {
            constraint["val"] = std::stoi(constraint_match[1]);
            constraint["isVariable"] = false;
            constraint["type"] = "gpr";
          } else if (std::regex_match(constraint_string, constraint_match, VAR_FPR_RE)) {
            constraint["val"] = std::stoi(constraint_match[1]);
            constraint["isVariable"] = true;
            constraint["type"] = "fpr";
          } else if (std::regex_match(constraint_string, constraint_match, DEF_FPR_RE)) {
            constraint["val"] = std::stoi(constraint_match[1]);
            constraint["isVariable"] = false;
            constraint["type"] = "fpr";
          } else {
            std::cerr << "Expected constraint definition at line " << lineNum << " got " << constraint_string << " instead" << std::endl;
            exit(-1);
          }

          if (isRead) {
            if (constraint["type"] == "gpr")
              ins_constraints["gprReadConstraints"].push_back(constraint);
            else if (constraint["type"] == "fpr") {
              ins_constraints["fprReadConstraints"].push_back(constraint);
            }
          } else {
            if (constraint["type"] == "gpr")
              ins_constraints["gprWriteConstraints"].push_back(constraint);
            else if (constraint["type"] == "fpr") {
              ins_constraints["fprWriteConstraints"].push_back(constraint);
            }
          }
        }
      }
    }
  }

  source_data["definitions"] = definitions;
  std::string inc_string = injaEnv.render(includeTemplate, include_data);
  std::string src_string = injaEnv.render(sourceTemplate, source_data);

  return {inc_string, src_string};
}
}

#include <fstream>
#include <filesystem>

using namespace aipg;

int main(int argc, char** argv) {
  char* out = (char*) "./";
  std::vector<std::string> idiom_paths;

  // parse args
  std::string usage_string = "Usage: aipg [--out out] file1.idiom file2.idiom ...";
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--out") == 0) {
      i++;
      if (i < argc) {
        out = argv[i];
      } else {
        std::cerr << "Expected path after --out" << std::endl;
        exit(-1);
      }
    } else if (strcmp(argv[i], "--help") == 0) {
      std::cout << usage_string << std::endl;
      exit(0);
    }else {
      idiom_paths.push_back(argv[i]);
    }
  }

  if (idiom_paths.empty()) {
    std::cout << usage_string << std::endl;
    exit(0);
  }
    
  if (!std::filesystem::exists(out))
    std::filesystem::copy(CTX_INC_FILE, out);

  std::filesystem::path inc_path(out);
  std::filesystem::path src_path(out);
  for (const auto& idiom_path : idiom_paths) {
    std::ifstream idiom_file(idiom_path);
    if (idiom_file.is_open()) {
      std::stringstream buffer;
      buffer << idiom_file.rdbuf();
    
      std::filesystem::path idiom_filepath(idiom_path);
      std::filesystem::path idiom_stem = idiom_filepath.stem();
      std::string idiom_src_filename = idiom_stem.string() + ".cpp";
      std::string idiom_inc_filename = idiom_stem.string() + ".hpp";
      std::ofstream idiom_parser_src(src_path / idiom_src_filename);
      std::ofstream idiom_parser_inc(inc_path / idiom_inc_filename);

      std::string idiom_name = idiom_stem.string();
      auto [inc_string, src_string] = generateParser(buffer.str(), idiom_name);

      if (idiom_parser_src.is_open()) {
        idiom_parser_src << src_string;
      } else {
        std::cerr << "Failed to open output src file " << idiom_src_filename << std::endl;
        exit(-1);
      }
      if (idiom_parser_inc.is_open()) {
        idiom_parser_inc << inc_string;
      } else {
        std::cerr << "Failed to open output include file " << idiom_inc_filename << std::endl;
        exit(-1);
      }
    } else {
      std::cerr << "Failed to open idiom " << idiom_path << std::endl;
      exit(-1);
    }
  }
}