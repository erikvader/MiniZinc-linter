#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/searcher.hpp>

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

// TODO: check if it is an expression that evaluates to a constant
bool isAssignedConstant(const MiniZinc::VarDecl &vd) {
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  if (vd.e() == nullptr)
    return false;
  switch (vd.e()->eid()) {
  case ExpressionId::E_INTLIT:
  case ExpressionId::E_FLOATLIT:
  case ExpressionId::E_SETLIT:
  case ExpressionId::E_BOOLLIT:
  case ExpressionId::E_STRINGLIT:
  case ExpressionId::E_ARRAYLIT: return true;
  default: return false;
  }
}

class NoDomainVarDecl : public LintRule {
public:
  constexpr NoDomainVarDecl() : LintRule(13, "unbounded-variable") {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;

  virtual void do_run(const MiniZinc::Model *model,
                      std::vector<LintResult> &results) const override {
    auto s = SearchBuilder()
                 .in_vardecl()
                 .in_assign_rhs()
                 .in_constraint()
                 .in_function_body()
                 .under(ExpressionId::E_VARDECL)
                 .capture()
                 .build();

    s.search(model).for_each([&results, t = this](const Search::ModelSearcher &ms) {
      auto &vd = *ms.capture(0)->cast<MiniZinc::VarDecl>();
      if (isNoDomainVar(vd) && !isAssignedConstant(vd)) {
        auto &loc = vd.loc();
        results.emplace_back(
            loc.filename().c_str(), t, "no explicit domain on variable declaration",
            LintResult::OneLineMarked{loc.firstLine(), loc.firstColumn(), loc.lastColumn()});
      }
    });
  }
};

} // namespace

REGISTER_RULE(NoDomainVarDecl)
