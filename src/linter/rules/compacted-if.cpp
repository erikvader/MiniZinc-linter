#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/utils.hpp>

namespace {
using namespace LZN;

class CompactedIf : public LintRule {
public:
  constexpr CompactedIf() : LintRule(20, "compacted-if") {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  using BT = MiniZinc::BinOpType;
  using UT = MiniZinc::UnOpType;

  virtual void do_run(LintEnv &env) const override {
    const auto s =
        env.userdef_only_builder().in_everywhere().under(ExpressionId::E_ITE).capture().build();
    auto ms = s.search(env.model());

    while (ms.next()) {
      auto ite = ms.capture_cast<MiniZinc::ITE>(0);
      if (!is_compactable_ite(ite))
        continue;
      const auto &loc = ite->loc();
      env.emplace_result(FileContents::Type::OneLineMarked, loc, this, "should be compacted",
                         rewrite(ite));
    }
  }

  static const MiniZinc::Expression *rewrite(const MiniZinc::ITE *ite) {
    MiniZinc::GCLock lock;
    // NOTE: to be able to put it in binop, it shouldn't modify them
    auto ifexpr = const_cast<MiniZinc::Expression *>(ite->ifExpr(0));
    auto thenexpr = const_cast<MiniZinc::Expression *>(ite->thenExpr(0));
    auto elseexpr = const_cast<MiniZinc::Expression *>(ite->elseExpr());

    bool elsezero = is_zero(elseexpr);
    auto nonzero = elsezero ? thenexpr : elseexpr;
    auto cond = elsezero
                    ? ifexpr
                    : new MiniZinc::UnOp(MiniZinc::Location().introduce(), UT::UOT_NOT, ifexpr);
    auto mult = new MiniZinc::BinOp(MiniZinc::Location().introduce(), cond, BT::BOT_MULT, nonzero);
    return mult;
  }

  static bool is_compactable_ite(const MiniZinc::ITE *ite) {
    assert(ite != nullptr);
    if (ite->size() != 1)
      return false;
    if (ite->elseExpr() == nullptr)
      return false;
    if (!is_same_number_type(ite->thenExpr(0), ite->elseExpr()))
      return false;
    return is_zero(ite->thenExpr(0)) ^ is_zero(ite->elseExpr());
  }

  static bool is_same_number_type(const MiniZinc::Expression *l, const MiniZinc::Expression *r) {
    return l->type().isPresent() && r->type().isPresent() &&
           ((l->type().isint() && r->type().isint()) ||
            (l->type().isfloat() && r->type().isfloat()));
  }

  static bool is_zero(const MiniZinc::Expression *e) {
    return is_int_expr(e, 0) || is_float_expr(e, 0);
  }
};

} // namespace

REGISTER_RULE(CompactedIf)
