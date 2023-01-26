
//#include "aipg/aipg.hpp"

#include <sstream>
#include <regex>
#include <vector>

// ppcdisasm-cpp
#include "opcode/ppc.h"
#include "ppcdisasm/ppc-dis.hpp"

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
#define GPR_PTRN "(?:" VAR_GPR_PTRN "|" DEF_GPR_PTRN ")"

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
#if 0
// label (aka valid C identifier)
#define DEF_LABEL_PTRN "[_a-zA-Z][_a-zA-Z0-9]{0,30}"
#define VAR_LABEL_PTRN "$LAB(\\d*)"
#endif

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

inja::Environment injaEnv {TEMPLATES_DIR};
const inja::Template sourceTemplate = injaEnv.parse_template("/source.j2");
const inja::Template includeTemplate = injaEnv.parse_template("/header.j2");
const inja::Template insCheckSingleTemplate = injaEnv.parse_template("/insCheckSingle.j2");
const inja::Template insCheckLoopTemplate = injaEnv.parse_template("/insCheckLoop.j2");
const inja::Template isInsMatchingTemplate = injaEnv.parse_template("/isInsnMatching.j2");
const inja::Template isMnemonicMatchingTemplate = injaEnv.parse_template("/isMnemonicMatching.j2");
const inja::Template isVariableGprMatchingTemplate = injaEnv.parse_template("/isVariableGprMatching.j2");
const inja::Template isDefinedGprMatchingTemplate = injaEnv.parse_template("/isDefinedGprMatching.j2");
const inja::Template isVariableFprMatchingTemplate = injaEnv.parse_template("/isVariableFprMatching.j2");
const inja::Template isDefinedFprMatchingTemplate = injaEnv.parse_template("/isDefinedFprMatching.j2");
const inja::Template isVariableImmMatchingTemplate = injaEnv.parse_template("/isVariableImmMatching.j2");
const inja::Template isDefinedImmMatchingTemplate = injaEnv.parse_template("/isDefinedImmMatching.j2");

using json = nlohmann::json;

struct powerpc_opcode* lookup_mnemonic(const std::string& mnemonic) {
  for (struct powerpc_opcode* op = (struct powerpc_opcode*) powerpc_opcodes; op < powerpc_opcodes + powerpc_num_opcodes; op++) {
    if (mnemonic == op->name)
      return op;
  }
  return nullptr;
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

  while (std::getline(iss, line)) {
    lineNum++;
    if (std::all_of(line.begin(),line.end(),isspace)) continue; // ignore empty lines

    std::smatch mnemonic_match;
    if (std::regex_search(line, mnemonic_match, MNEMONIC_RE)) {
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
      ins_data["operands"] = json::array();
      ins_data["opindex"] = opcode - powerpc_opcodes;

      std::string operands = mnemonic_match.suffix();
      int num_optional = 0; // (negative) number of optional arguments in this instruction (needed for checking optional operands)

      // parse operands
      const struct powerpc_operand *operand;
      const ppc_opindex_t *opindex;
      for (opindex = opcode->operands; *opindex != 0; opindex++) {
        operand = powerpc_operands + *opindex;
        json operand_data;
        operand_data["idx"] = *opindex;

        // match next operand (word) (maybe check if the asm is properly formatted and not just go to next operand?)
        std::smatch operand_match;
        std::string operand_string;
        if (std::regex_search(operands, operand_match, OPERAND_RE)) {
          operand_string = operand_match[0];
          operands = operand_match.suffix();
          operand_data["optional"] = false;
        } else if ((operand->flags & PPC_OPERAND_OPTIONAL) != 0) { // TODO: Support OPERAND_NEXT for 5 arg rotate-mask instructions
          num_optional--;
          operand_data["optional"] = true;
          operand_data["num_optional"] = num_optional;
        } else {
          std::cerr << "Expected more operands at line " << lineNum << std::endl;
          exit(-1);
        }

        std::smatch operand_type_match;
        json opData; // container of operand_data for operand matching templates
        opData["lineNo"] = lineNum;
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
        } else if ((operand->flags & PPC_OPERAND_RELATIVE) != 0 ||
                   (operand->flags & PPC_OPERAND_ABSOLUTE) != 0 ||
                   (operand->flags & PPC_OPERAND_PARENS) != 0) {
          // immediate or label/address (TODO)
          if (std::regex_match(operand_string, operand_type_match, VAR_IMM_RE)) {
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
            std::cerr << "Expected mandatory immediate expression at line " << lineNum << ", got " << operand_string << " instead" << std::endl;
            exit(-1);
          }

        } else {
          // immediate
          if (std::regex_match(operand_string, operand_type_match, VAR_IMM_RE)) {
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
            std::cerr << "Expected mandatory immediate expression at line " << lineNum << ", got " << operand_string << " instead" << std::endl;
            exit(-1);
          }
        }
      } // end of operand matching loop

      definitions.push_back(injaEnv.render(isInsMatchingTemplate, ins_data));

      if (checkNextRepeated) {
        ins_data["parseCheck"] = injaEnv.render(insCheckLoopTemplate, ins_data);
      } else {
        ins_data["parseCheck"] = injaEnv.render(insCheckSingleTemplate, ins_data);
      }
      source_data["ins_data"].push_back(ins_data);

      checkNextRepeated = false;
    } else if (std::regex_search(line, mnemonic_match, CONSUME_ASM_RE)) {
      // -------- Consume any asm line --------
      checkNextRepeated = true;
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
  char* src_out = (char*) "./";
  char* inc_out = (char*) "./";
  std::vector<std::string> idiom_paths;

  // parse args
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--src_out") == 0) {
      i++;
      if (i < argc) {
        src_out = argv[i];
      } else {
        std::cerr << "Expected path after --src_out" << std::endl;
        exit(-1);
      }
    } else if (strcmp(argv[i], "--inc_out") == 0) {
      i++;
      if (i < argc) {
        inc_out = argv[i];
      } else {
        std::cerr << "Expected path after --inc_out" << std::endl;
        exit(-1);
      }
    } else if (strcmp(argv[i], "--help") == 0) {
      std::cout << "Usage: aipg [--src_out src_out] [--inc_out inc_out] file1.idiom file2.idiom ..." << std::endl;
      exit(0);
    }else {
      idiom_paths.push_back(argv[i]);
    }
  }

  if (idiom_paths.empty()) {
    std::cout << "Usage: aipg [--src_out src_out] [--inc_out inc_out] file1.idiom file2.idiom ..." << std::endl;
    exit(0);
  }
    
  if (!std::filesystem::exists(inc_out))
    std::filesystem::copy(CTX_INC_FILE, inc_out);

  std::filesystem::path inc_path(inc_out);
  std::filesystem::path src_path(src_out);
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