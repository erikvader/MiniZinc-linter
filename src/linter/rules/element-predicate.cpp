#include <linter/registry.hpp>
#include <linter/rules.hpp>

namespace {
using namespace LZN;

class ElementPredicate : public LintRule {
public:
  constexpr ElementPredicate() : LintRule(15, "element-predicate", Category::STYLE) {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  using BT = MiniZinc::BinOpType;

  virtual void do_run(LintEnv &env) const override {
    const auto s =
        env.userdef_only_builder().in_everywhere().under(ExpressionId::E_CALL).capture().build();
    auto ms = s.search(env.model());

    while (ms.next()) {
      auto call = ms.capture_cast<MiniZinc::Call>(0);
      if (call->id() == MiniZinc::constants().ids.element && call->argCount() == 3) {
        MiniZinc::GCLock lock;
        auto arracc = new MiniZinc::ArrayAccess(MiniZinc::Location().introduce(), call->arg(1),
                                                {call->arg(0)});
        auto rewrite =
            new MiniZinc::BinOp(MiniZinc::Location().introduce(), arracc, BT::BOT_EQ, call->arg(2));

        const auto &loc = call->loc();
        env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                           "hard to read array access", rewrite);
      }
    }
  }
};

} // namespace

REGISTER_RULE(ElementPredicate)
