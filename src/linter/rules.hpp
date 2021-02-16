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
  constexpr LintRule(lintId id, const char *name) : id(id), name(name) {}

public:
  const lintId id;
  const char *name;
  virtual void run(MiniZinc::Model *model, std::vector<LintResult> &results) const = 0;
  // NOTE: Can't have this if this class should be able to evaluate at compile time (constexpr).
  // This means that the destructors of derived classes won't run through a base class pointer.
  // virtual ~LintRule() = default;
};

} // namespace LZN
