#pragma once

#include <linter/rules.hpp>

namespace LZN {

class NoDomainVarDecl : public LintRule {
public:
  constexpr NoDomainVarDecl() : LintRule(42) {}
  void run(MiniZinc::Model *model, std::vector<LintResult> &results) const override;
};

} // namespace LZN
