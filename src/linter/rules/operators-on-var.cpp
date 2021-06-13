#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/utils.hpp>

// TODO: multiplication?
namespace {
using namespace LZN;

class OperatorsOnVar : public LintRule {
public:
  constexpr OperatorsOnVar() : LintRule(18, "operator-on-var", Category::UNSURE) {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  using BT = MiniZinc::BinOpType;
  using UT = MiniZinc::UnOpType;

  virtual void do_run(LintEnv &env) const override {
    find_binop(env);
    find_unop(env);
  }

  void find_binop(LintEnv &env) const {
    const auto s =
        env.userdef_only_builder().in_everywhere().under(ExpressionId::E_BINOP).capture().build();
    auto ms = s.search(env.model());

    while (ms.next()) {
      auto bin = ms.capture_cast<MiniZinc::BinOp>(0);

      switch (bin->op()) {
      case BT::BOT_POW:
      case BT::BOT_DIV:
      case BT::BOT_MOD:
      case BT::BOT_IDIV:
      case BT::BOT_XOR:
      case BT::BOT_OR:
      case BT::BOT_IMPL:
      case BT::BOT_RIMPL:
      case BT::BOT_EQUIV:
        if (bin->lhs()->type().isvar() || bin->rhs()->type().isvar()) {
          const auto &loc = bin->loc();
          auto oper_loc = location_between(bin->lhs()->loc(), bin->rhs()->loc());
          FileContents::Region region;
          if (oper_loc) {
            auto [fl, fc, ll, lc] = oper_loc.value();
            if (fl == ll)
              region = FileContents::OneLineMarked(fl, fc, lc);
            else
              region = FileContents::OneLineMarked(fl, fc);
          } else {
            region = FileContents::OneLineMarked(loc);
          }

          std::string msg = "avoid using ";
          msg += bin->opToString().c_str();
          msg += " on var-expressions";
          env.emplace_result(region, loc, this, std::move(msg));
        }
      default:;
      }
    }
  }

  void find_unop(LintEnv &env) const {
    const auto s = env.userdef_only_builder().in_everywhere().under(UT::UOT_NOT).capture().build();
    auto ms = s.search(env.model());

    while (ms.next()) {
      auto unop = ms.capture_cast<MiniZinc::UnOp>(0);

      if (unop->e()->type().isvar()) {
        const auto &loc = unop->loc();
        FileContents::Region region(
            FileContents::OneLineMarked(loc.firstLine(), loc.firstColumn(), loc.firstColumn() + 2));

        std::string msg = "avoid using ";
        msg += unop->opToString().c_str();
        msg += " on var-expressions";
        env.emplace_result(region, loc, this, std::move(msg));
      }
    }
  }
};

} // namespace

REGISTER_RULE(OperatorsOnVar)
