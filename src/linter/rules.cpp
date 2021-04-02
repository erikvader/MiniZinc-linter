#include "rules.hpp"
#include <linter/overload.hpp>
#include <linter/searcher.hpp>

namespace {
template <typename T, typename F>
const T &lazy_value(std::optional<T> &opt, F f) {
  if (!opt) {
    opt = f();
  }
  return *opt;
}
} // namespace

namespace LZN {
std::ostream &operator<<(std::ostream &os, const LintResult &value) {
  os << "(" << value.rule->name << ")";
  std::visit(overload{[&](const LintResult::None &) { os << "None"; },
                      [&](const LintResult::OneLineMarked &olm) {
                        os << "OLM{" << olm.line << "," << olm.startcol << "," << olm.endcol << "}";
                      },
                      [&](const LintResult::MultiLine &ml) {
                        os << "ML{" << ml.startline << "," << ml.endline << "}";
                      }},
             value.region);
  return os;
}

void LintEnv::add_result(LintResult lr) {
  _results.push_back(std::move(lr));
}

const LintEnv::ECMap &LintEnv::equal_constrained() {
  return lazy_value(_equal_constrained, [model = _model]() {
    auto s = SearchBuilder()
                 .global_filter(filter_out_annotations)
                 // TODO: look for constraints in let
                 // .in_vardecl()
                 // .in_assign_rhs()
                 // .in_function_body()
                 .in_constraint()
                 .under(MiniZinc::BinOpType::BOT_EQ)
                 .capture()
                 .direct(MiniZinc::Expression::E_ID)
                 .build();
    auto ms = s.search(model);

    LintEnv::ECMap ids;
    while (ms.next()) {
      auto eq = ms.capture(0)->cast<MiniZinc::BinOp>();
      MiniZinc::Id *left;
      if ((left = eq->lhs()->dynamicCast<MiniZinc::Id>()) != nullptr) {
        assert(left->decl() != nullptr);
        ids.emplace(left->decl(), eq->rhs());
      }
      MiniZinc::Id *right;
      if ((right = eq->rhs()->dynamicCast<MiniZinc::Id>()) != nullptr) {
        assert(right->decl() != nullptr);
        ids.emplace(right->decl(), eq->lhs());
      }
    }
    return ids;
  });
}

const MiniZinc::Expression *LintEnv::get_equal_constrained_rhs(const MiniZinc::VarDecl *vd) {
  const auto &map = equal_constrained();
  auto it = map.find(vd);
  if (it != map.end()) {
    assert(it->second != nullptr);
    return it->second;
  }
  return nullptr;
}
} // namespace LZN
