#pragma once

#include <linter/searcher.hpp>
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
  MiniZinc::Env &_env;
  std::vector<LintResult> _results;
  const std::vector<std::string> &_includePath;

  // Looks anywhere for constraints on the form: constraint Id = Expr;
  using ECMap = std::unordered_map<const MiniZinc::VarDecl *, const MiniZinc::Expression *>;
  std::optional<ECMap> _equal_constrained;

  // Looks everywhere for all variable and parameter declarations.
  using VDVec = std::vector<const MiniZinc::VarDecl *>;
  std::optional<VDVec> _vardecls;

  // Looks anywhere for constrains on the form: constraint forall([Id[Expr] = Expr | ... ]); and
  // constraint Id[Expr] = Expr;
  struct AECValue {
    const MiniZinc::ArrayAccess *arrayaccess;
    const MiniZinc::Expression *rhs;
    const MiniZinc::Comprehension *comp;
    AECValue(const MiniZinc::ArrayAccess *arrayaccess, const MiniZinc::Expression *rhs,
             const MiniZinc::Comprehension *comp)
        : arrayaccess(arrayaccess), rhs(rhs), comp(comp) {}
  };
  using AECMap = std::unordered_multimap<const MiniZinc::VarDecl *, AECValue>;
  std::optional<AECMap> _array_equal_constrained;

  // functions not from stdlib nor auto generated (enums)
  using UDFVec = std::vector<const MiniZinc::FunctionI *>;
  std::optional<UDFVec> _user_defined_funcs;

  // the one and only solve item
  std::optional<const MiniZinc::SolveI *> _solve_item;

  // constraints inside let
  using ExprVec = std::vector<const MiniZinc::Expression *>;
  std::optional<ExprVec> _constraints;

public:
  LintEnv(const MiniZinc::Model *model, MiniZinc::Env &env,
          const std::vector<std::string> &includePath)
      : _model(model), _env(env), _includePath(includePath) {}

  template <typename... Args>
  decltype(_results)::reference emplace_result(Args &&...args) {
    return _results.emplace_back(std::forward<Args>(args)...);
  }
  void add_result(LintResult lr);

  const std::vector<LintResult> &results() & { return _results; }
  std::vector<LintResult> &&take_results() { return std::move(_results); }
  const MiniZinc::Model *model() const { return _model; }

  const ECMap &equal_constrained();
  const VDVec &user_defined_variable_declarations();
  const AECMap &array_equal_constrained();
  const UDFVec &user_defined_functions();
  const MiniZinc::SolveI *solve_item();
  const ExprVec &constraints();

  const MiniZinc::Expression *get_equal_constrained_rhs(const MiniZinc::VarDecl *);
  // is every index in the array touched from constraints?
  bool is_every_index_touched(const MiniZinc::VarDecl *); // TODO: move to utils?

  // return a builder that filters out everything (functions and includes) that is not user defined.
  SearchBuilder userdef_only_builder() const;
  MiniZinc::Env &minizinc_env() { return _env; }
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

struct FileContents {
  enum class Type { OneLineMarked, MultiLine, Empty };

  struct OneLineMarked {
    unsigned int line;
    unsigned int startcol;
    std::optional<unsigned int> endcol;
    OneLineMarked(unsigned int line, unsigned int startcol, unsigned int endcol) noexcept;
    OneLineMarked(unsigned int line, unsigned int startcol) noexcept;
    OneLineMarked(const MiniZinc::Location &loc) noexcept;
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
    MultiLine(unsigned int startline, unsigned int endline) noexcept;
    MultiLine(const MiniZinc::Location &loc) noexcept;
    bool operator==(const MultiLine &other) const noexcept {
      return startline == other.startline && endline == other.endline;
    };
    bool operator<(const MultiLine &other) const noexcept {
      return std::tie(startline, endline) < std::tie(other.startline, other.endline);
    };
  };

  using Region = std::variant<std::monostate, OneLineMarked, MultiLine>;
  Region region;
  std::string filename;

  FileContents(const Region r, const MiniZinc::Location &loc) : FileContents(Type::Empty, loc) {
    region = r;
  }
  FileContents(const Type tag, const MiniZinc::Location &loc) {
    switch (tag) {
    case Type::OneLineMarked: region.emplace<OneLineMarked>(loc); break;
    case Type::MultiLine: region.emplace<MultiLine>(loc); break;
    case Type::Empty: break;
    }
    auto fname = loc.filename().c_str();
    if (fname != nullptr)
      filename = fname;
  }
  FileContents(const Region r, const char *f) : region(r), filename(f) {}
  FileContents() = default;

  bool is_empty() const noexcept {
    return filename.empty() && std::holds_alternative<std::monostate>(region);
  }
  bool is_valid() const noexcept;

  bool operator==(const FileContents &other) const noexcept {
    return filename == other.filename && region == other.region;
  }
  bool operator<(const FileContents &other) const noexcept {
    return std::tie(filename, region) < std::tie(other.filename, other.region);
  }
};

struct LintResult {
  struct Sub {
    std::string message;
    FileContents content;

    template <typename... Args>
    Sub(std::string message, Args &&...args)
        : message(std::move(message)), content(std::forward<Args>(args)...) {}
  };

  LintRule const *rule;
  std::string message;
  FileContents content;
  std::optional<std::string> rewrite;
  std::vector<Sub> sub_results;
  bool depends_on_instance = false;

  template <typename RegionOrType, typename LocOrCstr>
  LintResult(const RegionOrType rot, const LocOrCstr &loc, const LintRule *rule,
             std::string message)
      : rule(rule), message(std::move(message)), content(rot, loc) {}

  template <typename RegionOrType, typename LocOrCstr>
  LintResult(const RegionOrType rot, const LocOrCstr &loc, const LintRule *rule,
             std::string message, const MiniZinc::Expression *rewrite)
      : rule(rule), message(std::move(message)), content(rot, loc) {
    set_rewrite(rewrite);
  }

  template <typename... Args>
  decltype(sub_results)::reference emplace_subresult(Args &&...args) {
    return sub_results.emplace_back(std::forward<Args>(args)...);
  }

  void add_relevant_decl(const MiniZinc::Expression *);

  void set_rewrite(const MiniZinc::Expression *);
  void set_rewrite(const MiniZinc::Item *);

  void set_depends_on_instance();

  // Are equal if both reference the same rule on the same place (doesn't care about message).
  bool operator==(const LintResult &other) const noexcept {
    return rule == other.rule && content == other.content;
  };

  bool operator<(const LintResult &other) const noexcept {
    return std::tie(rule, content) < std::tie(other.rule, other.content);
  }

  friend std::ostream &operator<<(std::ostream &, const LintResult &);
};

} // namespace LZN
