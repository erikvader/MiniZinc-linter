#pragma once

#include <linter/rules.hpp>
#include <vector>

namespace LZN {
// Print all results in `results` to stdout with pretty colors.
void stdout_print(const std::vector<LintResult> &results);
} // namespace LZN
