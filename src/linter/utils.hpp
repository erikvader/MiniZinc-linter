#pragma once
#include <algorithm>
#include <linter/searcher.hpp>
#include <minizinc/ast.hh>
#include <minizinc/eval_par.hh>

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

// Variants that accept pointers to const expressions
const MiniZinc::Expression *follow_id(const MiniZinc::Expression *e);
const MiniZinc::Expression *follow_id_to_decl(const MiniZinc::Expression *e);
MiniZinc::IntBounds compute_int_bounds(MiniZinc::EnvI &env, const MiniZinc::Expression *e);

// Check wheter the expression is an IntLit with some value
bool is_int_expr(const MiniZinc::Expression *e, long long int i);
bool is_float_expr(const MiniZinc::Expression *e, double f);

// Check if e contains any top-level parameters
bool depends_on_instance(const MiniZinc::Expression *e);

// Return a pointer to the other side of a binary operation given one of its sides
const MiniZinc::Expression *other_side(const MiniZinc::BinOp *parent,
                                       const MiniZinc::Expression *side);

// Check whether all expressions in access is an id to a variable from comp
bool comprehension_satisfies_array_access(const MiniZinc::Comprehension *comp,
                                          const MiniZinc::ArrayAccess *access);

// Check whether all expressions in access are ids
bool is_array_access_simple(const MiniZinc::ArrayAccess *access);

// Check whether comp iterates over all values of array
bool comprehension_covers_whole_array(const MiniZinc::Comprehension *comp,
                                      const MiniZinc::VarDecl *array);

// Check whether comp contains an where
bool comprehension_contains_where(const MiniZinc::Comprehension *comp);

// Equality check for two Locations
bool operator==(const MiniZinc::Location &r1, const MiniZinc::Location &r2);

// Return the line start, line end, column start and column end of the region between two locations
std::optional<std::tuple<unsigned int, unsigned int, unsigned int, unsigned int>>
location_between(const MiniZinc::Location &left, const MiniZinc::Location &right);

// Returns true if the path is certainly not reified.
// Returns false if not sure.
// TODO: should this just be `is_conjunctive`?
template <typename T>
bool is_not_reified(T b, T e) {
  return std::all_of(b, e, [](const MiniZinc::Expression *e) {
    if (auto bo = e->dynamicCast<MiniZinc::BinOp>(); bo != nullptr) {
      return bo->op() == MiniZinc::BinOpType::BOT_AND;
    }
    return false;
  });
}

// Returns true if the path is conjunctive, i.e. consists of /\, forall([..|..]) and let.
// Assumes that only the bodies of comprehensions are on the path.
template <typename T>
bool is_conjunctive(T b, T e) {
  bool last_comp = false;
  return std::all_of(b, e,
                     [&last_comp](const MiniZinc::Expression *e) {
                       if (last_comp) {
                         last_comp = false;
                         auto call = e->dynamicCast<MiniZinc::Call>();
                         return call != nullptr && call->id() == MiniZinc::constants().ids.forall;
                       }
                       if (auto bo = e->dynamicCast<MiniZinc::BinOp>(); bo != nullptr) {
                         return bo->op() == MiniZinc::BinOpType::BOT_AND;
                       }
                       if (e->isa<MiniZinc::Let>()) {
                         return true;
                       }
                       if (auto comp = e->dynamicCast<MiniZinc::Comprehension>(); comp != nullptr) {
                         last_comp = true;
                         return true;
                       }
                       return false;
                     }) &&
         !last_comp;
}

inline const Search EQUAL_CONSTRAINED_VARIABLES =
    SearchBuilder()
        .global_filter(filter_out_annotations)
        .global_filter(filter_global_comprehension_body)
        .under(MiniZinc::BinOpType::BOT_EQ)
        .capture()
        .direct(MiniZinc::Expression::E_ID)
        .capture()
        .build();

template <typename T>
void equal_constrained_variables(const MiniZinc::Expression *e, T inserter) {
  auto ms = EQUAL_CONSTRAINED_VARIABLES.search(e);

  while (ms.next()) {
    auto [pathbegin, pathend] = ms.current_path();
    for (int i = 0; i < 2; ++i) {
      assert(pathbegin != pathend);
      ++pathbegin;
    }
    if (!is_conjunctive(pathbegin, pathend))
      continue;

    auto eq = ms.capture(0)->cast<MiniZinc::BinOp>();
    auto id = ms.capture(1)->cast<MiniZinc::Id>();
    std::invoke(inserter, eq, id);
  }
}

inline const Search EQUAL_CONSTRAINED_ACCESS // force clang-format to break here
    = SearchBuilder()
          .global_filter(filter_global_comprehension_body)
          .under(MiniZinc::BinOpType::BOT_EQ)
          .capture()
          .direct(MiniZinc::Expression::E_ARRAYACCESS)
          .capture()
          .filter(filter_arrayaccess_name)
          .direct(MiniZinc::Expression::E_ID)
          .capture()
          .build();

template <typename T>
void equal_constrained_access(const MiniZinc::Expression *e, T inserter) {
  auto ms = EQUAL_CONSTRAINED_ACCESS.search(e);

  while (ms.next()) {
    auto [pathbegin, pathend] = ms.current_path();
    for (int i = 0; i < 3; ++i) {
      assert(pathbegin != pathend);
      ++pathbegin;
    }
    if (!is_conjunctive(pathbegin, pathend))
      continue;

    const MiniZinc::Comprehension *comp = nullptr;
    for (int i = 0; i < 2 && pathbegin != pathend; ++pathbegin, ++i) {
      if (i == 0) {
        comp = (*pathbegin)->dynamicCast<MiniZinc::Comprehension>();
      } else {
        auto call = (*pathbegin)->dynamicCast<MiniZinc::Call>();
        if (call == nullptr || call->id() != MiniZinc::constants().ids.forall) {
          comp = nullptr;
        }
      }
    }

    const auto eq = ms.capture_cast<MiniZinc::BinOp>(0);
    const auto access = ms.capture_cast<MiniZinc::ArrayAccess>(1);
    const auto id = ms.capture_cast<MiniZinc::Id>(2);
    const MiniZinc::Expression *rhs = other_side(eq, access);
    std::invoke(inserter, eq, access, id, rhs, comp);
  }
}
} // namespace LZN
