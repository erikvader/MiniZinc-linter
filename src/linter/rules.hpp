#pragma once

#include <minizinc/model.hh>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

namespace LZN {

struct LintResult;

using lintId = unsigned int;

class LintEnv {
  const MiniZinc::Model *_model;
  std::vector<LintResult> _results;

  using ECMap = std::unordered_map<const MiniZinc::VarDecl *, const MiniZinc::Expression *>;
  std::optional<ECMap> _equal_constrained;
  using VDVec = std::vector<const MiniZinc::VarDecl *>;
  std::optional<VDVec> _vardecls;
  // TODO: hitta alla array[i] = asd

public:
  LintEnv(const MiniZinc::Model *model) : _model(model) {}

  template <typename... Args>
  void add_result(Args &&...args) {
    _results.emplace_back(std::forward<Args>(args)...);
  }
  void add_result(LintResult lr);

  const std::vector<LintResult> &results() & { return _results; }
  std::vector<LintResult> &&take_results() { return std::move(_results); }
  const MiniZinc::Model *model() const { return _model; }

  const ECMap &equal_constrained();
  const VDVec &variable_declarations();

  const MiniZinc::Expression *get_equal_constrained_rhs(const MiniZinc::VarDecl *);
};

class LintRule {
protected:
  constexpr LintRule(lintId id, const char *name) : id(id), name(name) {}
  ~LintRule() = default;

public:
  const lintId id;
  const char *const name;
  void run(LintEnv &env) const { do_run(env); }

private:
  virtual void do_run(LintEnv &env) const = 0;
};

struct LintResult {
  std::string filename;
  LintRule const *rule;
  std::string message;

  struct OneLineMarked {
    unsigned int line;
    unsigned int startcol;
    unsigned int endcol;
    bool operator==(const OneLineMarked &other) const noexcept {
      return line == other.line && startcol == other.startcol && endcol == other.endcol;
    };
    bool operator<(const OneLineMarked &other) const noexcept {
      return std::tie(line, startcol, endcol) < std::tie(other.line, other.startcol, other.endcol);
    };
  };

  struct MultiLine {
    unsigned int startline;
    unsigned int endline;
    bool operator==(const MultiLine &other) const noexcept {
      return startline == other.startline && endline == other.endline;
    };
    bool operator<(const MultiLine &other) const noexcept {
      return std::tie(startline, endline) < std::tie(other.startline, other.endline);
    };
  };

  struct None {
    bool operator==(const None &) const noexcept { return true; };
    bool operator<(const None &) const noexcept { return false; };
  };

  typedef std::variant<OneLineMarked, MultiLine, None> Region;
  Region region;

  LintResult(std::string filename, const LintRule *rule, std::string message, Region region)
      : filename(std::move(filename)), rule(rule), message(std::move(message)),
        region(std::move(region)) {}

  // Are equal if both reference the same rule on the same place (doesn't care about message).
  bool operator==(const LintResult &other) const noexcept {
    return filename == other.filename && rule == other.rule && region == other.region;
  };

  bool operator<(const LintResult &other) const noexcept {
    return std::tie(filename, rule, region) < std::tie(other.filename, other.rule, other.region);
  }

  friend std::ostream &operator<<(std::ostream &, const LintResult &);
};

} // namespace LZN
