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
  using VarGraph = std::unordered_multimap<Thing, const MiniZinc::VarDecl *>;
  using ThingSet = std::unordered_set<Thing>;

  using EID = MiniZinc::Expression::ExpressionId;

  struct Searchers {
    Search collect_dependans_id;
    Search collect_dependans_call;
    Search find_uses_id;
    Search find_uses_call;
  };

  ThingSet find_unused(LintEnv &env, const Searchers &sear) const {
    Graph deps = find_dependencies(env, sear);
    std::vector<Thing> uses;
    find_uses<MiniZinc::Id>(env, uses, sear.find_uses_id);
    find_uses<MiniZinc::Call>(env, uses, sear.find_uses_call);
    recursively_remove(deps, uses);

    ThingSet unused;
    for (auto d : deps) {
      unused.insert(d.first);
    }

    VarGraph containment = find_containment(env);
    remove_contained_decls(deps, containment, unused);

    return unused;
  }

  static std::optional<Thing> convert_optthing(OptThing ot) {
    if (auto fi = std::get_if<const MiniZinc::FunctionI *>(&ot)) {
      return *fi;
    } else if (auto vd = std::get_if<const MiniZinc::VarDecl *>(&ot)) {
      return *vd;
    }
    return std::nullopt;
  }

  void recursively_remove(Graph &g, std::vector<Thing> &toremove) const {
    while (!toremove.empty()) {
      auto r = toremove.back();
      toremove.pop_back();

      auto [first, last] = g.equal_range(r);
      for (; first != last; ++first) {
        auto thing = convert_optthing(first->second);
        if (!thing)
          continue;
        toremove.push_back(thing.value());
      }

      g.erase(r);
    }
  }

  template <typename T>
  void find_uses(LintEnv &env, std::vector<Thing> &uses, const Search &s) const {
    auto ms = s.search(env.model());

    while (ms.next()) {
      auto decl = ms.capture_cast<T>(0)->decl();
      if (decl != nullptr)
        uses.emplace_back(decl);
    }
  }

  Graph find_dependencies(LintEnv &env, const Searchers &sear) const {
    Graph g;
    auto collect = [&](Thing t, const MiniZinc::Expression *e) {
      if (e != nullptr) {
        collect_dependans<MiniZinc::Id>(g, t, e, sear.collect_dependans_id);
        collect_dependans<MiniZinc::Call>(g, t, e, sear.collect_dependans_call);
      }
    };

    auto collect_var = [&](const MiniZinc::VarDecl *vd) {
      collect(vd, vd->e());
      collect(vd, vd->ti()->domain());
      for (auto r : vd->ti()->ranges()) {
        collect(vd, r->domain());
      }
      if (g.count(vd) == 0) {
        g.emplace(vd, std::monostate());
      }
    };

    for (auto fi : env.user_defined_functions()) {
      collect(fi, fi->e());
      if (g.count(fi) == 0) {
        g.emplace(fi, std::monostate());
      }
      for (auto arg : fi->params()) {
        collect_var(arg);
      }
    }

    for (auto vd : env.user_defined_variable_declarations()) {
      collect_var(vd);
    }

    return g;
  }

  template <typename T>
  void collect_dependans(Graph &g, Thing t, const MiniZinc::Expression *e, const Search &s) const {
    {
      auto vs = s.search(e);
      while (vs.next()) {
        auto id = vs.capture_cast<T>(0);
        auto decl = id->decl();
        if (decl == nullptr)
          continue;
        g.emplace(t, decl);
      }
    }
  }

  VarGraph find_containment(LintEnv &env) const {
    VarGraph g;
    const auto decl_searcher =
        env.userdef_only_builder().under(MiniZinc::Expression::E_VARDECL).capture().build();

    for (auto fi : env.user_defined_functions()) {
      find_vardecls(fi, fi->e(), g, decl_searcher);
      for (auto arg : fi->params()) {
        g.emplace(fi, arg);
      }
    }

    for (auto vd : env.user_defined_variable_declarations()) {
      find_vardecls(vd, vd->e(), g, decl_searcher);
    }

    return g;
  }

  void find_vardecls(Thing t, const MiniZinc::Expression *e, VarGraph &g,
                     const Search &decl_searcher) const {
    if (e == nullptr)
      return;
    auto ms = decl_searcher.search(e);
    while (ms.next()) {
      auto vd = ms.capture_cast<MiniZinc::VarDecl>(0);
      g.emplace(t, vd);
    }
  }

  // don't report contained VarDecls inside another unused VarDecl or function
  void remove_contained_decls(const Graph &deps, const VarGraph &cont, ThingSet &unused) const {
    for (auto c : cont) {
      Thing t = c.first;
      Thing vd = c.second;

      auto [f, l] = deps.equal_range(t);
      bool t_uses_vd = std::any_of(f, l, [vd](const auto &kv) {
        auto thing = convert_optthing(kv.second);
        if (!thing)
          return false;
        return thing.value() == vd;
      });
      if (t_uses_vd)
        unused.erase(vd);
    }
  }

  static Search collect_dependans_searcher(LintEnv &env, EID search_for) {
    return env.userdef_only_builder()
        .global_filter(filter_out_vardecls)
        .under(search_for)
        .capture()
        .build();
  }

  static Search find_uses_searcher(LintEnv &env, EID search_for) {
    return env.userdef_only_builder()
        .global_filter(filter_out_vardecls)
        .in_solve()
        .in_constraint()
        .under(search_for)
        .capture()
        .build();
  }

  virtual void do_run(LintEnv &env) const override {
    const Searchers sear{
        collect_dependans_searcher(env, EID::E_ID),
        collect_dependans_searcher(env, EID::E_CALL),
        find_uses_searcher(env, EID::E_ID),
        find_uses_searcher(env, EID::E_CALL),
    };

    for (auto unused : find_unused(env, sear)) {
      if (std::holds_alternative<const MiniZinc::FunctionI *>(unused)) {
        auto fi = std::get<const MiniZinc::FunctionI *>(unused);
        auto &loc = fi->loc();
        env.emplace_result(FileContents::Type::OneLineMarked, loc, this, "unused function");
      } else {
        auto vd = std::get<const MiniZinc::VarDecl *>(unused);
        auto &loc = vd->loc();
        env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                           "unused variable/parameter");
      }
    }
  }
};

} // namespace

REGISTER_RULE(UnusedVarFuncs)
