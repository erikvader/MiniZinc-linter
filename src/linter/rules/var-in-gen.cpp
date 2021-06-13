#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/searcher.hpp>

namespace {
using namespace LZN;

class VarInGen : public LintRule {
public:
  constexpr VarInGen() : LintRule(7, "var-in-gen", Category::UNSURE) {}

private:
  virtual void do_run(LintEnv &env) const override {
    const auto s = env.userdef_only_builder()
                       .in_everywhere()
                       .under(MiniZinc::Expression::E_COMP)
                       .capture()
                       .build();
    auto ms = s.search(env.model());

    while (ms.next()) {
      auto comp = ms.capture_cast<MiniZinc::Comprehension>(0);
      for (unsigned int gen = 0; gen < comp->numberOfGenerators(); ++gen) {
        auto &type = comp->in(gen)->type();
        if (type.isIntSet() && type.isvar()) {
          auto &loc = comp->in(gen)->loc();
          env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                             "avoid variables in generators");
        }
      }
    }
  }
};

} // namespace

REGISTER_RULE(VarInGen)
