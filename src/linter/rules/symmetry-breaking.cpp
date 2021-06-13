#include <algorithm>
#include <linter/registry.hpp>
#include <linter/rules.hpp>

namespace {
using namespace LZN;

class SymmetryBreaking : public LintRule {
public:
  constexpr SymmetryBreaking() : LintRule(6, "symmetry-breaking", Category::UNSURE) {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;

  static constexpr const char *SymmetryBreakers[] = {
      "lex2",
      "lex_greater",
      "lex_greatereq",
      "lex_less",
      "lex_lesseq",
      "strict_lex2",
      "seq_precede_chain",
      "value_precede",
      "value_precede_chain",
      "increasing",
      "decreasing",
  };

  virtual void do_run(LintEnv &env) const override {
    const auto s = env.userdef_only_builder().direct(ExpressionId::E_CALL).capture().build();
    for (auto con : env.constraints()) {
      auto ms = s.search(con);
      if (!ms.next())
        continue;
      auto call = ms.capture_cast<MiniZinc::Call>(0);
      bool is_symmetry = std::any_of(
          SymmetryBreakers, SymmetryBreakers + std::size(SymmetryBreakers),
          [callid = call->id().c_str()](const char *n) { return strcmp(n, callid) == 0; });

      if (is_symmetry) {
        const auto &loc = call->loc();
        const std::string fname = "symmetry_breaking_constraint";
        // NOTE: removing const so it can be used to generate a rewrite, that shouldn't modify it
        const std::vector<MiniZinc::Expression *> fargs = {const_cast<MiniZinc::Call *>(call)};
        MiniZinc::GCLock lock;
        auto rewrite = new MiniZinc::Call(MiniZinc::Location().introduce(), fname, fargs);
        env.emplace_result(FileContents::Type::OneLineMarked, loc, this, "common symmetry breaker",
                           rewrite);
      }
    }
  }
};

} // namespace

REGISTER_RULE(SymmetryBreaking)
