#include "rules.hpp"
#include <algorithm>
#include <linter/file_utils.hpp>
#include <linter/overload.hpp>
#include <linter/utils.hpp>
#include <minizinc/hash.hh>
#include <minizinc/prettyprinter.hh>

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
  std::visit(overload{[&](const std::monostate &) { os << "None"; },
                      [&](const FileContents::OneLineMarked &olm) {
                        os << "OLM{" << olm.line << "," << olm.startcol << ",";
                        if (olm.endcol)
                          os << olm.endcol.value();
                        else
                          os << "$";
                        os << "}";
                      },
                      [&](const FileContents::MultiLine &ml) {
                        os << "ML{" << ml.startline << "," << ml.endline << "}";
                      }},
             value.content.region);
  return os;
}

FileContents::OneLineMarked::OneLineMarked(unsigned int line, unsigned int startcol,
                                           unsigned int endcolumn) noexcept
    : line(line), startcol(startcol) {
  if (endcolumn >= startcol)
    endcol = endcolumn;
}

FileContents::OneLineMarked::OneLineMarked(unsigned int line, unsigned int startcol) noexcept
    : line(line), startcol(startcol) {}

FileContents::OneLineMarked::OneLineMarked(const MiniZinc::Location &loc) noexcept
    : line(loc.firstLine()), startcol(loc.firstColumn()) {
  if (loc.firstLine() == loc.lastLine()) {
    endcol = loc.lastColumn();
  }
}

FileContents::MultiLine::MultiLine(unsigned int startline, unsigned int endline) noexcept
    : startline(startline), endline(endline) {}

FileContents::MultiLine::MultiLine(const MiniZinc::Location &loc) noexcept
    : startline(loc.firstLine()), endline(loc.lastLine()) {}

bool FileContents::is_valid() const noexcept {
  if (is_empty())
    return true;
  return !filename.empty() &&
         std::visit(overload{
                        [](const std::monostate &) { return true; },
                        [](const FileContents::MultiLine &ml) {
                          return ml.startline > 0 && ml.endline > 0 && ml.endline >= ml.startline;
                        },
                        [](const FileContents::OneLineMarked &olm) {
                          return olm.line > 0 && olm.startcol > 0 &&
                                 (!olm.endcol || olm.endcol.value() > 0) &&
                                 (!olm.endcol || olm.endcol.value() >= olm.startcol);
                        },
                    },
                    region);
}

void LintEnv::add_result(LintResult lr) {
  _results.push_back(std::move(lr));
}

const LintEnv::ECMap &LintEnv::equal_constrained() {
  return lazy_value(_equal_constrained, [this]() {
    LintEnv::ECMap ids;
    for (auto con : constraints()) {
      equal_constrained_variables(con, [&ids](const MiniZinc::BinOp *eq, const MiniZinc::Id *id) {
        auto other = other_side(eq, id);
        assert(id->decl() != nullptr);
        ids.emplace(id->decl(), other);
      });
    }
    return ids;
  });
}

const LintEnv::VDVec &LintEnv::user_defined_variable_declarations() {
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  return lazy_value(_vardecls, [this, model = _model]() {
    const auto s = userdef_only_builder()
                       .in_vardecl()
                       .in_assign_rhs()
                       .in_constraint()
                       .in_function_body()
                       .under(ExpressionId::E_VARDECL)
                       .capture()
                       .build();
    auto ms = s.search(model);

    LintEnv::VDVec vec;
    while (ms.next()) {
      // Remove duplicated functions, see `user_defined_functions`. This check is redundant
      // otherwise.
      if (auto func = ms.cur_item()->dynamicCast<MiniZinc::FunctionI>(); func != nullptr) {
        auto udf = user_defined_functions();
        if (std::find(udf.cbegin(), udf.cend(), func) == udf.cend()) {
          ms.skip_item();
          continue;
        }
      }

      auto vd = ms.capture(0)->cast<MiniZinc::VarDecl>();
      vec.push_back(vd);
    }

    // add _objective
    if (auto si = solve_item(); si != nullptr && si->e() != nullptr) {
      auto id = si->e()->dynamicCast<MiniZinc::Id>();
      if (id->decl() != nullptr && std::find(vec.begin(), vec.end(), id->decl()) == vec.end())
        vec.push_back(id->decl());
    }

    return vec;
  });
}

const LintEnv::AECMap &LintEnv::array_equal_constrained() {
  return lazy_value(_array_equal_constrained, [this]() {
    LintEnv::AECMap map;

    for (auto con : constraints()) {
      equal_constrained_access(con, [&map](const MiniZinc::BinOp * /*eq*/,
                                           const MiniZinc::ArrayAccess *access,
                                           const MiniZinc::Id *id, const MiniZinc::Expression *rhs,
                                           const MiniZinc::Comprehension *comp) {
        auto decl = id->decl();
        assert(decl != nullptr);
        map.emplace(std::piecewise_construct, std::forward_as_tuple(decl),
                    std::forward_as_tuple(access, rhs, comp));
      });
    }
    return map;
  });
}

const LintEnv::UDFVec &LintEnv::user_defined_functions() {
  return lazy_value(_user_defined_funcs, [this, model = _model]() {
    const auto s = userdef_only_builder().in_function().build();
    auto ms = s.search(model);
    LintEnv::UDFVec vec;
    while (ms.next()) {
      auto fi = ms.cur_item()->cast<MiniZinc::FunctionI>();
      // NOTE: A function declared with var arguments generats a par version of the same function in
      // the exact same location. It doesn't seem like it is possible to use this new variant, so it
      // is not included. Both should be considered as the same function anyway.
      // TODO: double check that the correct one is being removed, is it always the second one?.
      if (std::any_of(vec.cbegin(), vec.cend(), [fi](const MiniZinc::FunctionI *f) {
            return f->id() == fi->id() && f->loc() == fi->loc();
          }))
        continue;
      vec.push_back(fi);
    }
    return vec;
  });
}

const MiniZinc::SolveI *LintEnv::solve_item() {
  // TODO: why this instead of MiniZinc::Model::solveItem?
  return lazy_value(_solve_item, [this, model = _model]() -> const MiniZinc::SolveI * {
    const auto s = userdef_only_builder().in_solve().build();
    auto ms = s.search(model);
    while (ms.next()) {
      return ms.cur_item()->cast<MiniZinc::SolveI>();
    }
    return nullptr;
  });
}

const LintEnv::VDSet &LintEnv::search_hinted_variables() {
  return lazy_value(_search_hinted, [this]() {
    LintEnv::VDSet set;
    auto solve = solve_item();
    if (solve == nullptr || solve->ann().isEmpty())
      return set;

    const auto s = userdef_only_builder().under(MiniZinc::Expression::E_ID).capture().build();

    for (const auto *e : solve->ann()) {
      auto ms = s.search(e);
      while (ms.next()) {
        auto id = ms.capture_cast<MiniZinc::Id>(0);
        if (id->decl() != nullptr)
          set.insert(id->decl());
      }
    }

    return set;
  });
}

const LintEnv::ExprVec &LintEnv::constraints() {
  return lazy_value(_constraints, [this, model = _model]() {
    LintEnv::ExprVec vec;

    { // constraints in let
      const auto s = userdef_only_builder()
                         .in_vardecl()
                         .in_assign_rhs()
                         .in_function_body()
                         .in_constraint()
                         .under(MiniZinc::Expression::E_LET)
                         .capture()
                         .build();
      auto ms = s.search(model);

      while (ms.next()) {
        auto let = ms.capture_cast<MiniZinc::Let>(0);
        for (auto constr : let->let()) {
          if (constr->eid() == MiniZinc::Expression::E_VARDECL)
            continue;
          vec.push_back(constr);
        }
      }
    }

    {
      const auto s = userdef_only_builder().in_constraint().build();
      auto ms = s.search(model);
      while (ms.next()) {
        auto con = ms.cur_item()->cast<MiniZinc::ConstraintI>();
        vec.push_back(con->e());
      }
    }

    return vec;
  });
}

const LintEnv::CSet &LintEnv::comprehensions() {
  return lazy_value(_comprehensions, [this, model = _model]() {
    LintEnv::CSet set;

    const auto s = userdef_only_builder()
                       .in_everywhere()
                       .under(MiniZinc::Expression::E_COMP)
                       .capture()
                       .build();
    auto ms = s.search(model);

    while (ms.next()) {
      auto comp = ms.capture_cast<MiniZinc::Comprehension>(0);
      set.insert(comp);
    }

    return set;
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

bool LintEnv::is_every_index_touched(const MiniZinc::VarDecl *arraydecl) {
  auto [first, last] = array_equal_constrained().equal_range(arraydecl);
  if (first == last)
    return false;

  for (; first != last; ++first) {
    auto [arrayaccess, rhs, comp] = first->second;
    if (comp == nullptr)
      continue;

    if (!is_array_access_simple(arrayaccess) ||
        !comprehension_satisfies_array_access(comp, arrayaccess) ||
        comprehension_contains_where(comp))
      continue;

    if (comprehension_covers_whole_array(comp, arraydecl))
      return true;
  }
  return false;
}

bool LintEnv::is_search_hinted(const MiniZinc::VarDecl *vd) {
  return search_hinted_variables().count(vd) > 0;
}

SearchBuilder LintEnv::userdef_only_builder() const {
  return SearchBuilder().only_user_defined(_includePath).recursive();
}

void LintResult::set_rewrite(const MiniZinc::Expression *expr) {
  std::ostringstream oss;
  MiniZinc::Printer p(oss, 0, false);
  p.print(expr);
  rewrite = oss.str();
}

void LintResult::set_rewrite(const MiniZinc::Item *item) {
  std::ostringstream oss;
  MiniZinc::Printer p(oss, 80, false);
  p.print(item);
  rewrite = oss.str();
}

void LintResult::set_depends_on_instance() {
  depends_on_instance = true;
  emplace_subresult("This result depends on the current values of some parameters");
}

void LintResult::add_relevant_decl(const MiniZinc::Expression *e) {
  if (e == nullptr)
    return;
  auto id = e->dynamicCast<MiniZinc::Id>();
  if (id == nullptr)
    return;
  auto decl = follow_id_to_decl(id);
  if (decl == nullptr || decl->eid() != MiniZinc::Expression::E_VARDECL)
    return;
  const auto &loc = decl->loc();
  emplace_subresult("relevant variable declaration", FileContents::Type::MultiLine, loc);
}

} // namespace LZN
