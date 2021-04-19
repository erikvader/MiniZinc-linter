#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/utils.hpp>
#include <minizinc/astexception.hh>

namespace {
using namespace LZN;

class ZeroOneVars : public LintRule {
public:
  constexpr ZeroOneVars() : LintRule(22, "zero-one-vars") {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  using BT = MiniZinc::BinOpType;

  virtual void do_run(LintEnv &env) const override {
    case_impl(env, BT::BOT_LQ, MiniZinc::IntVal(1)); // expr1 = 1 -> expr2 = 1
    case_impl(env, BT::BOT_GQ, MiniZinc::IntVal(0)); // expr1 = 0 -> expr2 = 0
    case_sum(env);
  }

  void case_sum(LintEnv &env) const {
    const auto s = env.get_builder()
                       .in_everywhere()
                       .under(ExpressionId::E_CALL)
                       .capture()
                       .direct(ExpressionId::E_COMP)
                       .capture()
                       .filter(filter_comprehension_expr)
                       .direct(ExpressionId::E_CALL)
                       .capture()
                       .direct(BT::BOT_EQ)
                       .capture()
                       .direct(ExpressionId::E_ARRAYACCESS)
                       .capture()
                       .filter(filter_arrayaccess_name)
                       .direct(ExpressionId::E_ID)
                       .capture()
                       .build();
    auto ms = s.search(env.model());

    while (ms.next()) {
      const auto sum = ms.capture_cast<MiniZinc::Call>(0);
      if (sum->id() != MiniZinc::constants().ids.sum)
        continue;
      const auto bool2int = ms.capture_cast<MiniZinc::Call>(2);
      if (bool2int->id() != MiniZinc::constants().ids.bool2int)
        continue;

      const auto comp = ms.capture_cast<MiniZinc::Comprehension>(1);
      const auto eq = ms.capture_cast<MiniZinc::BinOp>(3);
      const auto access = ms.capture_cast<MiniZinc::ArrayAccess>(4);
      const auto id = ms.capture_cast<MiniZinc::Id>(5);
      const auto decl = id->decl();
      assert(decl != nullptr);
      const MiniZinc::Expression *rhs = other_side(eq, access);
      assert(rhs != nullptr);

      if (!is_int_expr(rhs, 1))
        continue;
      if (!is_array_access_simple(access))
        continue;
      if (!comprehension_satisfies_array_access(comp, access))
        continue;
      if (comprehension_contains_where(comp))
        continue;
      if (!comprehension_covers_whole_array(comp, decl))
        continue;
      if (!is_zero_one_expr(env, access))
        continue;

      const auto &loc = sum->loc();
      auto &res = env.emplace_result(loc.filename().c_str(), this, "abuse 0..1 domain",
                                     FileContents::OneLineMarked(loc), sum_rewrite(id));
      if (depends_on_instance(decl->ti()->domain())) {
        res.set_depends_on_instance();
      }
      res.emplace_subresult("has domain 0..1", access->loc().filename().c_str(),
                            FileContents::OneLineMarked(access->loc()));
    }
  }

  void case_impl(LintEnv &env, BT rewrite_type, MiniZinc::IntVal equal_to) const {
    const auto main_searcher = env.get_builder()
                                   .in_everywhere()
                                   .under(BT::BOT_IMPL)
                                   .capture()
                                   .filter([](const auto *impl, const auto *side) -> bool {
                                     return impl->template cast<MiniZinc::BinOp>()->lhs() == side;
                                   })
                                   .direct(BT::BOT_EQ)
                                   .capture()
                                   .direct(ExpressionId::E_INTLIT)
                                   .capture()
                                   .build();

    const auto off_searcher = env.get_builder()
                                  .direct(BT::BOT_EQ)
                                  .capture()
                                  .direct(ExpressionId::E_INTLIT)
                                  .capture()
                                  .build();

    auto main = main_searcher.search(env.model());

    while (main.next()) {
      if (main.capture_cast<MiniZinc::IntLit>(2)->v() != equal_to)
        continue;

      auto otherside = other_side(main.capture_cast<MiniZinc::BinOp>(0), main.capture(1));
      auto off = off_searcher.search(otherside);
      if (!off.next())
        continue;
      if (off.capture_cast<MiniZinc::IntLit>(1)->v() != equal_to)
        continue;

      auto expr1 = other_side(main.capture_cast<MiniZinc::BinOp>(1), main.capture(2));
      auto expr2 = other_side(off.capture_cast<MiniZinc::BinOp>(0), off.capture(1));

      if (!is_zero_one_expr(env, expr1) || !is_zero_one_expr(env, expr2))
        continue;

      const auto &loc = main.capture(0)->loc();
      auto &res = env.emplace_result(loc.filename().c_str(), this, "abuse 0..1 domain",
                                     FileContents::OneLineMarked(loc),
                                     binary_rewrite(rewrite_type, expr1, expr2));
      if (depends_on_instance(expr1) || depends_on_instance(expr2)) {
        res.set_depends_on_instance();
      }

      res.emplace_subresult("has domain 0..1", expr1->loc().filename().c_str(),
                            FileContents::OneLineMarked(expr1->loc()));
      res.emplace_subresult("has domain 0..1", expr2->loc().filename().c_str(),
                            FileContents::OneLineMarked(expr2->loc()));
    }
  }

  const MiniZinc::Expression *binary_rewrite(BT type, const MiniZinc::Expression *expr1,
                                             const MiniZinc::Expression *expr2) const {
    MiniZinc::GCLock lock;
    // NOTE: shouldn't modify expr1 and expr2
    return new MiniZinc::BinOp(MiniZinc::Location().introduce(),
                               const_cast<MiniZinc::Expression *>(expr1), type,
                               const_cast<MiniZinc::Expression *>(expr2));
  }

  const MiniZinc::Expression *sum_rewrite(const MiniZinc::Expression *arr_id) const {
    MiniZinc::GCLock lock;
    // NOTE: shouldn't modify arr_id
    auto mut_arr_id = const_cast<MiniZinc::Expression *>(arr_id);
    return new MiniZinc::Call(MiniZinc::Location().introduce(), MiniZinc::constants().ids.sum,
                              {mut_arr_id});
  }

  bool contains_non_toplevel_var_in_access(const MiniZinc::Expression *e) const {
    static const auto s = SearchBuilder()
                              .under(ExpressionId::E_ARRAYACCESS)
                              .filter(filter_arrayaccess_idx)
                              .under(ExpressionId::E_ID)
                              .capture()
                              .build();
    auto ms = s.search(e);
    while (ms.next()) {
      auto id = ms.capture_cast<MiniZinc::Id>(0);
      if (id->decl() != nullptr && !id->decl()->toplevel())
        return true;
    }
    return false;
  }

  bool is_zero_one_expr(LintEnv &env, const MiniZinc::Expression *e) const {
    if (e == nullptr)
      return false;

    // TODO: compute_int_bounds crashes (assertion fails) if `e` contains an array access using
    // generators.
    // forall(i in 1..4)(xs[i]) would crash if e = xs[i].
    // Check bounds manually instead if e happens to be an array access.
    if (contains_non_toplevel_var_in_access(e)) {
      if (auto access = e->dynamicCast<MiniZinc::ArrayAccess>(); access != nullptr) {
        if (auto id = access->v()->dynamicCast<MiniZinc::Id>(); id != nullptr) {
          if (id->decl() != nullptr) {
            return is_zero_one_intlit(id->decl()->ti());
          }
        }
      }
      return false;
    }

    std::optional<MiniZinc::IntBounds> bounds;
    try {
      bounds = compute_int_bounds(env.minizinc_env().envi(), e);
    } catch (const MiniZinc::LocationException &exc) { return false; }
    assert(bounds);

    return bounds->valid && bounds->l == MiniZinc::IntVal(0) && bounds->u == MiniZinc::IntVal(1);
  }

  bool is_zero_one_intlit(MiniZinc::TypeInst *ti) const {
    auto followed = MiniZinc::follow_id(ti->domain());
    if (followed == nullptr)
      return false;

    if (auto setlit = followed->dynamicCast<MiniZinc::SetLit>(); setlit != nullptr) {
      if (auto isv = setlit->isv(); isv != nullptr) {
        return isv->min() == MiniZinc::IntVal(0) && isv->max() == MiniZinc::IntVal(1);
      }
    }
    return false;
  }
};

} // namespace

REGISTER_RULE(ZeroOneVars)
