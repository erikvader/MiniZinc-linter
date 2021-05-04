#include <linter/file_utils.hpp>
#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/utils.hpp>

namespace {
using namespace LZN;

class GlobalConstraintReified : public LintRule {
public:
  constexpr GlobalConstraintReified() : LintRule(17, "global-reified") {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;

  virtual void do_run(LintEnv &env) const override {
    const auto s = env.get_builder().under(ExpressionId::E_CALL).capture().build();

    for (auto con : env.constraints()) {
      auto ms = s.search(con);

      while (ms.next()) {
        auto call = ms.capture_cast<MiniZinc::Call>(0);
        auto decl = call->decl();
        if (decl == nullptr)
          continue;

        auto decl_path = decl->loc().filename();
        if (s.include_path() != nullptr && decl_path.size() > 0 &&
            path_included_from(*s.include_path(), decl_path) && decl->ti()->type().isvarbool() &&
            !decl->fromStdLib() && !is_not_reified(ms)) {
          const auto &loc = call->loc();
          env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                             "reified global constraint");
        }
      }
    }
  }
};

} // namespace

REGISTER_RULE(GlobalConstraintReified)
