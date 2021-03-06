#include <linter/registry.hpp>
#include <linter/rules.hpp>

namespace {
using namespace LZN;

class GlobalsInFunction : public LintRule {
public:
  constexpr GlobalsInFunction() : LintRule(5, "globals-in-function", Category::STYLE) {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;

  virtual void do_run(LintEnv &env) const override {
    const auto s = env.userdef_only_builder().under(ExpressionId::E_ID).capture().build();

    for (auto fun : env.user_defined_functions()) {
      auto ms = s.search(fun->e());
      while (ms.next()) {
        auto id = ms.capture_cast<MiniZinc::Id>(0);
        if (id->decl() != nullptr && id->decl()->toplevel() && id->type().isvar()) {
          const auto &loc = id->loc();
          env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                             "avoid using globals in functions, pass as an argument instead");
        }
      }
    }
  }
};
} // namespace

REGISTER_RULE(GlobalsInFunction)
