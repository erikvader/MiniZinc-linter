#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/searcher.hpp>

namespace {
using namespace LZN;

class VarInIfWhere : public LintRule {
public:
  constexpr VarInIfWhere() : LintRule(26, "var-in-if-where") {}

private:
  virtual void do_run(LintEnv &env) const override {
    find_where(env);
    find_if(env);
  }

  void find_where(LintEnv &env) const {
    const auto s =
        env.get_builder().in_everywhere().under(MiniZinc::Expression::E_COMP).capture().build();
    auto ms = s.search(env.model());

    while (ms.next()) {
      auto comp = ms.capture_cast<MiniZinc::Comprehension>(0);
      for (unsigned int gen = 0; gen < comp->numberOfGenerators(); ++gen) {
        const MiniZinc::Expression *where = comp->where(gen);
        if (where == nullptr)
          continue;

        auto &type = where->type();
        if (type.isvar()) {
          auto &loc = where->loc();
          env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                             "avoid var-expressions in where clauses");
        }
      }
    }
  }

  void find_if(LintEnv &env) const {
    const auto s =
        env.get_builder().in_everywhere().under(MiniZinc::Expression::E_ITE).capture().build();
    auto ms = s.search(env.model());

    while (ms.next()) {
      auto ite = ms.capture_cast<MiniZinc::ITE>(0);
      for (unsigned int i = 0; i < ite->size(); ++i) {
        const MiniZinc::Expression *cond = ite->ifExpr(i);
        auto &type = cond->type();
        if (type.isvar()) {
          auto &loc = cond->loc();
          env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                             "avoid var-expressions in if statements");
        }
      }
    }
  }
};

} // namespace

REGISTER_RULE(VarInIfWhere)
