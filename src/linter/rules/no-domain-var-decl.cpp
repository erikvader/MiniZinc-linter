#include <linter/registry.hpp>
#include <linter/rules.hpp>

// TODO: False positive when [..|i in arr] is encountered.
// array[1..5] of var 1..2: xs;
// constraint forall(i in xs)(i > 1);
// `i` should not be considered
namespace {
using namespace LZN;

bool isNoDomainVar(const MiniZinc::VarDecl &vd) {
  auto &t = vd.type();
  auto domain = vd.ti()->domain();
  return t.isvar() && t.st() == MiniZinc::Type::SetType::ST_PLAIN &&
         (t.bt() == MiniZinc::Type::BaseType::BT_INT ||
          t.bt() == MiniZinc::Type::BaseType::BT_FLOAT) &&
         t.dim() >= 0 && t.isPresent() && domain == nullptr;
}

class NoDomainVarDecl : public LintRule {
public:
  constexpr NoDomainVarDecl() : LintRule(13, "unbounded-variable", Category::PERFORMANCE) {}

private:
  virtual void do_run(LintEnv &env) const override {
    for (const MiniZinc::VarDecl *vd : env.user_defined_variable_declarations()) {
      if (isNoDomainVar(*vd) && vd->e() == nullptr &&
          env.get_equal_constrained_rhs(vd) == nullptr) {
        auto &loc = vd->loc();
        env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                           "no explicit domain on variable declaration");
      }
    }
  }
};

} // namespace

REGISTER_RULE(NoDomainVarDecl)
