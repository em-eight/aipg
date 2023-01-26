
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace aipg {
struct Context {
  std::unordered_map<uint32_t, uint32_t> gprs;
  std::unordered_map<uint32_t, uint32_t> fprs;
  std::unordered_map<uint32_t, int32_t> imms;

  /// @brief The index of each instruction that matched with the idiom's assembly lines from the starting instruction
  std::vector<uint32_t> matchInsIdxs;
};
}