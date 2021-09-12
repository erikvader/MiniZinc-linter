#pragma once

#include <linter/searcher.hpp>
#include <minizinc/model.hh>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <variant>
#include <vector>

namespace LZN {

// forward declare
struct LintResult;

// type used for the ids of LintRules.
using lintId = unsigned int;

// Lint rule categories. Should always be in sync with `CATEGORY_NAMES`.
enum class Category {
  CHALLENGE = 0, // Enforce the rules of the MiniZinc challenge.
  STYLE,         // Stylistic changes.
  UNSURE,        // Changes on things that in general is bad, very imprecise.
  PERFORMANCE,   // Precise suggestions for performance increase
  REDUNDANT,     // Remove redundant or unused things.
};

// The string names for all categories in `Category`. Should always be in sync with the enum.
inline const std::vector<std::string> CATEGORY_NAMES = {"challenge", "style", "unsure",
                                                        "performance", "redundant"};

// Environment where rules get their information and where they store their information.
// Some commonly performed searches are cached here as well.
class LintEnv {
  // The parsed model to lint
  const MiniZinc::Model *_model;
  MiniZinc::Env &_env;
  // A vector of all results
  std::vector<LintResult> _results;
  // The include path
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
    const MiniZinc::Comprehension *comp; // is nullptr for the second form.
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

  // variables that are present in the search annotation
  using VDSet = std::unordered_set<const MiniZinc::VarDecl *>;
  std::optional<VDSet> _search_hinted;

  // all comprehensions
  using CSet = std::unordered_set<const MiniZinc::Comprehension *>;
  std::optional<CSet> _comprehensions;

public:
  LintEnv(const MiniZinc::Model *model, MiniZinc::Env &env,
          const std::vector<std::string> &includePath)
      : _model(model), _env(env), _includePath(includePath) {}

  // Add a LintResult, can be constructed in-place.
  template <typename... Args>
  decltype(_results)::reference emplace_result(Args &&...args) {
    return _results.emplace_back(std::forward<Args>(args)...);
  }
  void add_result(LintResult lr);

  // return a reference to all results.
  const std::vector<LintResult> &results() & { return _results; }
  // take all results
  std::vector<LintResult> &&take_results() { return std::move(_results); }
  // pointer to the model
  const MiniZinc::Model *model() const { return _model; }
  MiniZinc::Env &minizinc_env() { return _env; }

  // Cached searches, each one explained above.
  const ECMap &equal_constrained();
  const VDVec &user_defined_variable_declarations();
  const AECMap &array_equal_constrained();
  const UDFVec &user_defined_functions();
  const MiniZinc::SolveI *solve_item();
  const VDSet &search_hinted_variables();
  const ExprVec &constraints();
  const CSet &comprehensions();

  // return what the variable is equal constrained to
  const MiniZinc::Expression *get_equal_constrained_rhs(const MiniZinc::VarDecl *);
  // is every index in the array touched from constraints?
  bool is_every_index_touched(const MiniZinc::VarDecl *);
  // check whether a variable is mentioned in the search hint
  bool is_search_hinted(const MiniZinc::VarDecl *);

  // return a builder that filters out everything (functions and includes) that is not user defined.
  SearchBuilder userdef_only_builder() const;
};

// A lint rule. Contains necessary metadata and a function to perform analysis.
class LintRule {
protected:
  constexpr LintRule(lintId id, const char *name, Category cat)
      : id(id), name(name), category(cat) {}
  ~LintRule() = default;

public:
  const lintId id;         // an id that must be unique
  const char *const name;  // a unique printable name
  const Category category; // a category a rule fits in to

  // Perform the analysis
  void run(LintEnv &env) const { do_run(env); }

private:
  virtual void do_run(LintEnv &env) const = 0;
};

// This class represents a portion of a file. Used by LintResult to indicate where a match happened.
// Used by the stdoutprinter to print the affected lines.
struct FileContents {
  enum class Type { OneLineMarked, MultiLine, Empty };

  // A single line where a portion of it is of interest, and could e.g. be marked with a different
  // color.
  struct OneLineMarked {
    unsigned int line;
    unsigned int startcol;
    std::optional<unsigned int> endcol; // nullopt means end of line
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

  // Multiple whole lines are of interest.
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

  // The region of interest in `filename`. There are several cases:
  // - region and filename both empty: there is nothing of interest anywhere.
  // - only region empty: there is something of interest in a file, but exactly where is unknown.
  // - only filename empty: invalid.
  // - both given: a region in a file is of interest.
  using Region = std::variant<std::monostate, OneLineMarked, MultiLine>;
  Region region;
  std::string filename;

  // Various constructors.
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

  // returns true if empty
  bool is_empty() const noexcept {
    return filename.empty() && std::holds_alternative<std::monostate>(region);
  }
  // returns true if valid
  bool is_valid() const noexcept;

  bool operator==(const FileContents &other) const noexcept {
    return filename == other.filename && region == other.region;
  }
  bool operator<(const FileContents &other) const noexcept {
    return std::tie(filename, region) < std::tie(other.filename, other.region);
  }
};

// A result from a LintRule.
struct LintResult {
  // A subresult. Could be used for pointing out something in a different place. For example: "...
  // is constrained here ...".
  struct Sub {
    std::string message;
    FileContents content;

    template <typename... Args>
    Sub(std::string message, Args &&...args)
        : message(std::move(message)), content(std::forward<Args>(args)...) {}
  };

  LintRule const *rule;               // The rule which this result originates from.
  std::string message;                // A message of what is wrong.
  FileContents content;               // The affected region and file.
  std::optional<std::string> rewrite; // An optional rewrite.
  std::vector<Sub> sub_results;       // Zero or more subresult.
  bool depends_on_instance = false;   // Whether this result depends on parameters.

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

  // Shorthand for added a subresult that points out a relevant variable declaration from an id.
  void add_relevant_decl(const MiniZinc::Expression *);

  // Set a rewrite
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

  // Debug print
  friend std::ostream &operator<<(std::ostream &, const LintResult &);
};

} // namespace LZN
