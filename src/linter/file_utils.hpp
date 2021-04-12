#pragma once

#include <minizinc/aststring.hh>
#include <string>
#include <vector>

namespace LZN {
std::vector<std::string> lines_of_file(const std::string &filename, unsigned int start,
                                       unsigned int end);

bool is_user_defined(const std::vector<std::string> &includePath, MiniZinc::ASTString path);
} // namespace LZN
