#include <algorithm>
#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/searcher.hpp>
#include <unordered_map>
#include <unordered_set>

namespace {
using namespace LZN;

class UnusedVarFuncs : public LintRule {
public:
  constexpr UnusedVarFuncs() : LintRule(1, "unused-var-funcs") {}

private:
  using Thing = std::variant<const MiniZinc::FunctionI *, const MiniZinc::VarDecl *>;
  using OptThing =
      std::variant<const MiniZinc::FunctionI *, const MiniZinc::VarDecl *, std::monostate>;
  using Graph = std::unordered_multimap<Thing, OptThing>;
  using ThingSet = std::unordered_set<Thing>;

  ThingSet find_unused(LintEnv &env) const {
    Graph deps = find_dependencies(env);
    std::vector<Thing> uses;
    find_uses<MiniZinc::Id>(env, MiniZinc::Expression::E_ID, uses);
    find_uses<MiniZinc::Call>(env, MiniZinc::Expression::E_CALL, uses);

    recursively_remove(deps, uses);
    ThingSet unused;
    for (auto d : deps) {
      unused.insert(d.first);
    }
    return unused;
  }

  void recursively_remove(Graph &g, std::vector<Thing> &toremove) const {
    while (!toremove.empty()) {
      auto r = toremove.back();
      toremove.pop_back();

      auto [first, last] = g.equal_range(r);
      for (; first != last; ++first) {
        auto dep = first->second;
        if (std::holds_alternative<const MiniZinc::FunctionI *>(dep)) {
          toremove.emplace_back(std::get<const MiniZinc::FunctionI *>(dep));
        } else if (std::holds_alternative<const MiniZinc::VarDecl *>(dep)) {
          toremove.emplace_back(std::get<const MiniZinc::VarDecl *>(dep));
        }
      }

      g.erase(r);
    }
  }

  template <typename T>
  void find_uses(LintEnv &env, MiniZinc::Expression::ExpressionId search_for,
                 std::vector<Thing> &uses) const {
    static const auto s = env.get_builder()
                              .global_filter(filter_out_vardecls)
                              .in_solve()
                              .in_constraint()
                              .under(search_for)
                              .capture()
                              .build();
    auto ms = s.search(env.model());

    while (ms.next()) {
      auto decl = ms.capture_cast<T>(0)->decl();
      if (decl != nullptr)
        uses.emplace_back(decl);
    }
  }

  Graph find_dependencies(LintEnv &env) const {
    Graph g;
    auto collect = [&](Thing t, const MiniZinc::Expression *e) {
      if (e != nullptr) {
        collect_dependans<MiniZinc::Id>(MiniZinc::Expression::E_ID, g, t, e, env);
        collect_dependans<MiniZinc::Call>(MiniZinc::Expression::E_CALL, g, t, e, env);
      }
    };

    for (auto fi : env.user_defined_functions()) {
      collect(fi, fi->e());
      if (g.count(fi) == 0) {
        g.emplace(fi, std::monostate());
      }
    }

    for (auto vd : env.user_defined_variable_declarations()) {
      collect(vd, vd->e());
      collect(vd, vd->ti()->domain());
      for (auto r : vd->ti()->ranges()) {
        collect(vd, r->domain());
      }
      if (g.count(vd) == 0) {
        g.emplace(vd, std::monostate());
      }
    }

    return g;
  }

  template <typename T>
  void collect_dependans(MiniZinc::Expression::ExpressionId search_for, Graph &g, Thing t,
                         const MiniZinc::Expression *e, const LintEnv &env) const {
    {
      static const auto var_searcher =
          env.get_builder().global_filter(filter_out_vardecls).under(search_for).capture().build();
      auto vs = var_searcher.search(e);
      while (vs.next()) {
        auto id = vs.capture_cast<T>(0);
        auto decl = id->decl();
        if (decl == nullptr)
          continue;
        g.emplace(t, decl);
      }
    }
  }

  virtual void do_run(LintEnv &env) const override {
    for (auto unused : find_unused(env)) {
      if (std::holds_alternative<const MiniZinc::FunctionI *>(unused)) {
        auto fi = std::get<const MiniZinc::FunctionI *>(unused);
        auto &loc = fi->loc();
        env.emplace_result(loc.filename().c_str(), this, "unused function",
                           FileContents::OneLineMarked{loc});
      } else {
        auto vd = std::get<const MiniZinc::VarDecl *>(unused);
        auto &loc = vd->loc();
        env.emplace_result(loc.filename().c_str(), this, "unused variable/parameter",
                           FileContents::OneLineMarked{loc});
      }
    }
  }
};

} // namespace

REGISTER_RULE(UnusedVarFuncs)
