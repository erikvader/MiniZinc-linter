#pragma once

#include <minizinc/model.hh>
#include <string>
#include <variant>
#include <vector>

namespace LZN {

struct LintResult;

typedef unsigned int lintId;

class LintRule {
protected:
  constexpr LintRule(lintId id, const char *name) : id(id), name(name) {}

public:
  const lintId id;
  const char *const name;
  virtual void run(MiniZinc::Model *model, std::vector<LintResult> &results) const = 0;
  // NOTE: Can't have this if this class should be able to evaluate at compile time (constexpr).
  // This means that the destructors of derived classes won't run through a base class pointer.
  // virtual ~LintRule() = default;
};

struct LintResult {
  const std::string filename;
  const LintRule *const rule;
  const std::string message;

  struct OneLineMarked {
    const unsigned int line;
    const unsigned int startcol;
    const unsigned int endcol;
  };

  struct MultiLine {
    const unsigned int startline;
    const unsigned int endline;
  };

  struct None {};

  typedef std::variant<OneLineMarked, MultiLine, None> Region;
  const Region region;

  LintResult(std::string filename, const LintRule *rule, std::string message, Region region)
      : filename(std::move(filename)), rule(rule), message(std::move(message)),
        region(std::move(region)) {}

  std::string file_position() const;
  void file_position(std::ostream &stream) const;
};

} // namespace LZN
