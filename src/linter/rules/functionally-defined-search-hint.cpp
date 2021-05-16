#include <linter/registry.hpp>
#include <linter/rules.hpp>
#include <linter/utils.hpp>

namespace {
using namespace LZN;

class NonFuncHint : public LintRule {
public:
  constexpr NonFuncHint() : LintRule(9, "non-func-hint") {}

private:
  using ExpressionId = MiniZinc::Expression::ExpressionId;

  using VDSet = std::unordered_set<const MiniZinc::VarDecl *>;
  using Vis = std::unordered_set<const MiniZinc::FunctionI *>;

  // TODO: remove all these
  // TODO: write tests
  // limitation: funktioner som returnar oändrad global variabel, elr lokalt alias till ett sådant,
  // f() = 2
  // limitation: false \/ (x=2)
  // limitation: x = f(a, b) -- a och b kanske konstrainar varandra. Gör den ändå?
  // limitation: a=b ==== b=a??
  // limitation: count([..|..], 3)
  // limitation: [a] = [b+1]
  // limitation: constraints of top-level inside function bodies
  virtual void do_run(LintEnv &env) const override {
    VDSet non_func;
    for (auto vd : env.user_defined_variable_declarations()) {
      if (vd->e() == nullptr && vd->toplevel() && vd->type().isvar() && !env.is_search_hinted(vd))
        non_func.insert(vd);
    }

    for (auto vde : env.equal_constrained()) {
      auto vd = vde.first;
      non_func.erase(vd);
    }
    for (auto vde : env.array_equal_constrained()) {
      auto vd = vde.first;
      non_func.erase(vd);
    }

    equal_constrained_functions(env, non_func);

    for (auto vd : non_func) {
      const auto &loc = vd->loc();
      env.emplace_result(FileContents::Type::OneLineMarked, loc, this,
                         "possibly non-functionally defined variable not in search hint");
    }
  }

  void equal_constrained_functions(LintEnv &env, VDSet &non_func) const {
    const auto s =
        env.userdef_only_builder().in_constraint().under(ExpressionId::E_CALL).capture().build();
    auto ms = s.search(env.model());
    while (ms.next()) {
      auto call = ms.capture_cast<MiniZinc::Call>(0);
      auto [pb, pe] = ms.current_path();
      assert(pb != pe);
      ++pb;
      if (!is_conjunctive(pb, pe))
        continue;

      Vis visited;
      auto args = function_funcdef(call, visited, s);
      assert(args.size() == call->argCount());

      for (unsigned int i = 0; i < args.size(); i++) {
        if (args[i]) {
          auto vd = argument_to_vardecl(call->arg(i));
          if (vd != nullptr && vd->toplevel() && vd->type().isvar()) {
            non_func.erase(vd);
          }
        }
      }
    }
  }

  std::vector<bool> function_funcdef(const MiniZinc::Call *call, Vis &visited,
                                     const Search &call_search) const {
    assert(call != nullptr);
    std::vector<bool> ans(call->argCount(), false);

    auto decl = call->decl();
    if (decl == nullptr || decl->fromStdLib() || decl->e() == nullptr)
      return ans;

    std::cout << *decl << std::endl;

    if (visited.count(decl) == 0) {
      visited.insert(decl);
    } else {
      std::cerr << "cyclic function calls detected" << std::endl;
      return ans;
    }

    auto set_funcdef = [&](const MiniZinc::VarDecl *vd) {
      assert(call->decl()->params().size() == ans.size());
      for (unsigned int i = 0; i < ans.size(); ++i) {
        if (call->decl()->params()[i] == vd) {
          ans[i] = true;
          break;
        }
      }
    };

    equal_constrained_variables(decl->e(), [&](const MiniZinc::BinOp *, const MiniZinc::Id *id) {
      set_funcdef(id->decl());
    });
    equal_constrained_access(decl->e(),
                             [&](const MiniZinc::BinOp *, const MiniZinc::ArrayAccess *,
                                 const MiniZinc::Id *id, const MiniZinc::Expression *,
                                 const MiniZinc::Comprehension *) { set_funcdef(id->decl()); });

    auto ms = call_search.search(decl->e());
    while (ms.next()) {
      auto call2 = ms.capture_cast<MiniZinc::Call>(0);
      auto [pb, pe] = ms.current_path();
      assert(pb != pe);
      ++pb;
      if (!is_conjunctive(pb, pe))
        continue;

      auto args = function_funcdef(call2, visited, call_search);
      assert(args.size() == call2->argCount());

      for (unsigned int i = 0; i < args.size(); i++) {
        if (args[i]) {
          set_funcdef(argument_to_vardecl(call2->arg(i)));
        }
      }
    }

    visited.erase(decl);
    return ans;
  }

  static const MiniZinc::VarDecl *argument_to_vardecl(const MiniZinc::Expression *arg) {
    if (auto id = arg->dynamicCast<MiniZinc::Id>(); id != nullptr) {
      return id->decl();
    }
    // TODO: allow comprehensions like [c+1 | c in Array]
    if (auto call = arg->dynamicCast<MiniZinc::Call>();
        // TODO: allow all functions that take one argument and returns something with the same (or
        // similar) type?
        call != nullptr && call->argCount() == 1 && call->id() == MiniZinc::ASTString("array1d")) {
      return argument_to_vardecl(call->arg(0));
    }
    if (auto acc = arg->dynamicCast<MiniZinc::ArrayAccess>(); acc != nullptr) {
      return argument_to_vardecl(acc->v());
    }
    return nullptr;
  }
};

} // namespace

REGISTER_RULE(NonFuncHint)
