#include <algorithm>
#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <minizinc/eval_par.hh>

namespace {
using namespace LZN;

class OneBasedArrays : public LintRule {
public:
  constexpr OneBasedArrays() : LintRule(19, "one-based-arrays") {}

private:
  using BT = MiniZinc::BinOpType;

  bool starts_at_one(MiniZinc::TypeInst *ti) const {
    auto followed = MiniZinc::follow_id(ti->domain());
    if (followed == nullptr)
      return false;

    if (auto setlit = followed->dynamicCast<MiniZinc::SetLit>(); setlit != nullptr) {
      if (auto isv = setlit->isv(); isv != nullptr) {
        return isv->min() == MiniZinc::IntVal(1);
      } else if (setlit->v().size() > 0) {
        return std::any_of(setlit->v().begin(), setlit->v().end(), [](MiniZinc::Expression *e) {
          if (auto intval = e->dynamicCast<MiniZinc::IntLit>(); intval != nullptr) {
            return intval->v() == MiniZinc::IntVal(1);
          }
          return false;
        });
      }
    } else if (auto set = followed->dynamicCast<MiniZinc::BinOp>();
               set != nullptr && set->op() == BT::BOT_DOTDOT) {
      auto min = set->lhs()->dynamicCast<MiniZinc::IntLit>();
      if (min == nullptr)
        return false;
      return min->v() == MiniZinc::IntVal(1);
    }

    return false;
  }

  virtual void do_run(LintEnv &env) const override {
    for (auto vd : env.user_defined_variable_declarations()) {
      if (!vd->ti()->isarray())
        continue;

      for (auto r : vd->ti()->ranges()) {
        if (!starts_at_one(r)) {
          const auto &loc = r->loc();
          auto &lr = env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                                        "better to start at 1");
          lr.add_relevant_decl(r->domain());
        }
      }
    }
  }
};

} // namespace

REGISTER_RULE(OneBasedArrays)
