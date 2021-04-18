#include "rules.hpp"
#include <linter/file_utils.hpp>
#include <linter/overload.hpp>
#include <linter/utils.hpp>
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
                                           unsigned int endcol) noexcept
    : line(line), startcol(startcol), endcol(endcol) {}

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

void LintEnv::add_result(LintResult lr) {
  _results.push_back(std::move(lr));
}

const LintEnv::ECMap &LintEnv::equal_constrained() {
  return lazy_value(_equal_constrained, [this]() {
    const auto s = get_builder()
                       .global_filter(filter_out_annotations)
                       .direct(MiniZinc::BinOpType::BOT_EQ)
                       .capture()
                       .direct(MiniZinc::Expression::E_ID)
                       .build();

    LintEnv::ECMap ids;
    for (auto con : constraints()) {
      auto ms = s.search(con);

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
    }
    return ids;
  });
}

const LintEnv::VDVec &LintEnv::user_defined_variable_declarations() {
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  return lazy_value(_vardecls, [this, model = _model]() {
    const auto s = get_builder()
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

    {
      const auto s = get_builder()
                         .direct(MiniZinc::Expression::E_CALL)
                         .capture()
                         .direct(MiniZinc::Expression::E_COMP)
                         .capture()
                         .filter(filter_comprehension_expr)
                         .direct(MiniZinc::BinOpType::BOT_EQ)
                         .capture()
                         .direct(MiniZinc::Expression::E_ARRAYACCESS)
                         .capture()
                         .filter(filter_arrayaccess_name)
                         .direct(MiniZinc::Expression::E_ID)
                         .capture()
                         .build();

      for (auto con : constraints()) {
        auto forallsearcher = s.search(con);

        while (forallsearcher.next()) {
          const auto forall = forallsearcher.capture_cast<MiniZinc::Call>(0);
          if (forall->id() != MiniZinc::constants().ids.forall)
            continue;

          const auto comp = forallsearcher.capture_cast<MiniZinc::Comprehension>(1);
          const auto eq = forallsearcher.capture_cast<MiniZinc::BinOp>(2);
          const auto access = forallsearcher.capture_cast<MiniZinc::ArrayAccess>(3);
          const auto decl = forallsearcher.capture_cast<MiniZinc::Id>(4)->decl();
          assert(decl != nullptr);
          const MiniZinc::Expression *rhs = other_side(eq, access);
          assert(rhs != nullptr);

          map.emplace(decl, AECValue{access, rhs, comp});
        }
      }
    }

    {
      const auto s = get_builder()
                         .direct(MiniZinc::BinOpType::BOT_EQ)
                         .capture()
                         .direct(MiniZinc::Expression::E_ARRAYACCESS)
                         .capture()
                         .filter(filter_arrayaccess_name)
                         .direct(MiniZinc::Expression::E_ID)
                         .capture()
                         .build();

      for (auto con : constraints()) {
        auto individualsearcher = s.search(con);

        while (individualsearcher.next()) {
          const auto eq = individualsearcher.capture_cast<MiniZinc::BinOp>(0);
          const auto access = individualsearcher.capture_cast<MiniZinc::ArrayAccess>(1);
          const auto decl = individualsearcher.capture_cast<MiniZinc::Id>(2)->decl();
          const MiniZinc::Expression *rhs = other_side(eq, access);
          assert(decl != nullptr);
          assert(rhs != nullptr);
          map.emplace(decl, AECValue{access, rhs, nullptr});
        }
      }
    }

    return map;
  });
}

const LintEnv::UDFVec &LintEnv::user_defined_functions() {
  return lazy_value(_user_defined_funcs, [this, model = _model]() {
    const auto s = get_builder().in_function().build();
    auto ms = s.search(model);
    LintEnv::UDFVec vec;
    while (ms.next()) {
      auto fi = ms.cur_item()->cast<MiniZinc::FunctionI>();
      vec.push_back(fi);
    }
    return vec;
  });
}

const MiniZinc::SolveI *LintEnv::solve_item() {
  return lazy_value(_solve_item, [this, model = _model]() -> const MiniZinc::SolveI * {
    const auto s = get_builder().in_solve().build();
    auto ms = s.search(model);
    while (ms.next()) {
      return ms.cur_item()->cast<MiniZinc::SolveI>();
    }
    return nullptr;
  });
}

const LintEnv::ExprVec &LintEnv::constraints() {
  return lazy_value(_constraints, [this, model = _model]() {
    LintEnv::ExprVec vec;

    { // constraints in let
      const auto s = get_builder()
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
      const auto s = get_builder().in_constraint().build();
      auto ms = s.search(model);
      while (ms.next()) {
        auto con = ms.cur_item()->cast<MiniZinc::ConstraintI>();
        vec.push_back(con->e());
      }
    }

    return vec;
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

SearchBuilder LintEnv::get_builder() const {
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
  emplace_subresult("relevant variable declaration", loc.filename().c_str(),
                    FileContents::MultiLine(loc));
}

} // namespace LZN
