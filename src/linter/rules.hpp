#pragma once

#include <minizinc/model.hh>
#include <string>
#include <vector>

namespace LZN {

typedef unsigned int lintId;

struct LintResult {
  LintResult(std::string message) : message(message) {}
  std::string message;
};

class LintRule {
protected:
  constexpr LintRule(lintId id) : id(id) {}

public:
  const lintId id;
  virtual void run(MiniZinc::Model *model, std::vector<LintResult> &results) const = 0;
  // NOTE: Can't have this if this class should be able to evaluate at compile time (constexpr).
  // This means that the destructors of derived classes won't run through a base class pointer.
  // virtual ~LintRule() = default;
};

inline constexpr unsigned int NUM_RULES = 1;

const LintRule *const *all_rules();

} // namespace LZN
