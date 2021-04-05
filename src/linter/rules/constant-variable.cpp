#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/searcher.hpp>

namespace {
using namespace LZN;

class ConstantVariable : public LintRule {
public:
  constexpr ConstantVariable() : LintRule(4, "constant-variable") {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;

  virtual void do_run(LintEnv &env) const override {
    for (const MiniZinc::VarDecl *vd : env.variable_declarations()) {
      const MiniZinc::Expression *rhs = nullptr;
      if ((rhs = vd->e()) == nullptr)
        rhs = env.get_equal_constrained_rhs(vd);

      // TODO: what if array that doesn't cover all values?
      if (rhs != nullptr && vd->type().isvar() && rhs->type().isPar()) {
        auto &loc = vd->loc();
        env.add_result(
            loc.filename().c_str(), this, "is only assigned to par values, shouldn't be var",
            LintResult::OneLineMarked{loc.firstLine(), loc.firstColumn(), loc.lastColumn()});
      }
    }
  }
};

} // namespace

REGISTER_RULE(ConstantVariable)
