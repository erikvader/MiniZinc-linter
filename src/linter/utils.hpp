#pragma once

#include <algorithm>
#include <linter/searcher.hpp>
#include <minizinc/eval_par.hh>
#include <vector>

namespace LZN {
template <typename Cmp, typename It, typename ItModify>
bool unsorted_equal_cmp(ItModify modbeg, ItModify modend, It beg, It end, Cmp cmp) {
  ItModify first_unsure = modbeg;
  for (; beg != end; ++beg) {
    ItModify old_fu = first_unsure;

    for (ItModify i = first_unsure; i != modend; ++i) {
      if (cmp(*i, *beg)) {
        if (i != first_unsure)
          std::swap(*i, *first_unsure);
        ++first_unsure;
        break;
      }
    }

    if (old_fu == first_unsure)
      return false;
  }

  return first_unsure == modend;
}

inline const MiniZinc::Expression *follow_id(const MiniZinc::Expression *e) {
  return MiniZinc::follow_id(const_cast<MiniZinc::Expression *>(e));
}
inline const MiniZinc::Expression *follow_id_to_decl(const MiniZinc::Expression *e) {
  return MiniZinc::follow_id_to_decl(const_cast<MiniZinc::Expression *>(e));
}

inline MiniZinc::IntBounds compute_int_bounds(MiniZinc::EnvI &env, const MiniZinc::Expression *e) {
  // NOTE: doesn't modify `e` I think
  return MiniZinc::compute_int_bounds(env, const_cast<MiniZinc::Expression *>(e));
}

inline bool is_int_expr(const MiniZinc::Expression *e, long long int i) {
  assert(e != nullptr);
  if (auto intlit = e->dynamicCast<MiniZinc::IntLit>(); intlit != nullptr) {
    return intlit->v() == MiniZinc::IntVal(i);
  }
  return false;
}

inline bool is_float_expr(const MiniZinc::Expression *e, double f) {
  assert(e != nullptr);
  if (auto floatlit = e->dynamicCast<MiniZinc::FloatLit>(); floatlit != nullptr) {
    return floatlit->v() == MiniZinc::FloatVal(f);
  }
  return false;
}

// check if e uses any toplevel parameters
inline bool depends_on_instance(const MiniZinc::Expression *e) {
  if (e == nullptr)
    return false;
  // TODO: implement using EVisitor instead
  static const auto s = SearchBuilder().under(MiniZinc::Expression::E_ID).capture().build();
  auto ms = s.search(e);
  while (ms.next()) {
    auto id = ms.capture_cast<MiniZinc::Id>(0);
    // TODO: include pars in let-statements as well
    if (id->type().isPar() && id->decl() != nullptr && id->decl()->toplevel()) {
      return true;
    }
    // TODO: check declaration for array accesses
    // TODO: follow a chain of variables if `id` is set to another variable
    if (id->decl() != nullptr && depends_on_instance(id->decl()->ti()->domain()))
      return true;
  }
  return false;
}

inline const MiniZinc::Expression *other_side(const MiniZinc::BinOp *parent,
                                              const MiniZinc::Expression *side) {
  assert(parent != nullptr);
  return parent->lhs() == side ? parent->rhs() : (parent->rhs() == side ? parent->lhs() : nullptr);
}

inline bool comprehension_satisfies_array_access(const MiniZinc::Comprehension *comp,
                                                 const MiniZinc::ArrayAccess *access) {
  // NOTE: containsBoundVariable shouldn't modify
  auto mut_comp = const_cast<MiniZinc::Comprehension *>(comp);
  auto access_vars = access->idx();
  return std::all_of(
      access_vars.begin(), access_vars.end(),
      [mut_comp](MiniZinc::Expression *acc_id) { return mut_comp->containsBoundVariable(acc_id); });
}

inline bool is_array_access_simple(const MiniZinc::ArrayAccess *access) {
  auto access_vars = access->idx();
  return std::all_of(access_vars.begin(), access_vars.end(),
                     [](const MiniZinc::Expression *acc_id) {
                       return acc_id->eid() == MiniZinc::Expression::E_ID;
                     });
}

inline bool comprehension_covers_whole_array(const MiniZinc::Comprehension *comp,
                                             const MiniZinc::VarDecl *array) {
  auto array_domains = array->ti()->ranges();
  std::vector<const MiniZinc::Expression *> comp_domains;
  for (unsigned int gen = 0; gen < comp->numberOfGenerators(); ++gen) {
    auto in = comp->in(gen);
    for (unsigned int i = 0; i < comp->numberOfDecls(gen); ++i) {
      comp_domains.push_back(in);
    }
  }
  return unsorted_equal_cmp(comp_domains.begin(), comp_domains.end(), array_domains.begin(),
                            array_domains.end(),
                            [](const MiniZinc::Expression *c, const MiniZinc::TypeInst *a) {
                              return MiniZinc::Expression::equal(c, a->domain());
                            });
}

inline bool comprehension_contains_where(const MiniZinc::Comprehension *comp) {
  for (unsigned int gen = 0; gen < comp->numberOfGenerators(); ++gen) {
    if (comp->where(gen) != nullptr)
      return true;
  }
  return false;
}

inline bool operator==(const MiniZinc::Location &r1, const MiniZinc::Location &r2) {
  return r1.filename() == r2.filename() && r1.firstLine() == r2.firstLine() &&
         r1.firstColumn() == r2.firstColumn() && r1.lastLine() == r2.lastLine() &&
         r1.lastColumn() == r2.lastColumn() && r1.isIntroduced() == r2.isIntroduced() &&
         r1.isNonAlloc() == r2.isNonAlloc();
}
} // namespace LZN
