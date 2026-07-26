#pragma once
#include <algorithm>
#include <cctype>
#include <string>

inline std::string timemoreToLower(const std::string& s) {
  std::string out = s;
  std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return std::tolower(c); });
  return out;
}
