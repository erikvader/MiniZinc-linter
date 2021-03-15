#include "searcher.hpp"
#include <minizinc/astiterator.hh>
#include <minizinc/model.hh>

namespace {
using namespace LZN;

std::size_t children_of(const MiniZinc::Expression *root,
                        std::vector<const MiniZinc::Expression *> &results) {
  assert(root != nullptr);

  struct : MiniZinc::EVisitor {
    std::vector<const MiniZinc::Expression *> *results;
    std::size_t new_children = 0;
    const MiniZinc::Expression *root;

    bool enter(MiniZinc::Expression *child) {
      if (child == root) {
        return true;
      } else {
        results->push_back(child);
        ++new_children;
        return false;
      }
    }
  } childExtractor;
  childExtractor.results = &results;
  childExtractor.root = root;

  // NOTE: Assume that top_down doesn't modify root
  MiniZinc::top_down(childExtractor, const_cast<MiniZinc::Expression *>(root));
  return childExtractor.new_children;
}
} // namespace

namespace LZN::Impl {

bool SearchLocs::should_visit(const MiniZinc::Item *i) const {
  assert(i != nullptr);
  using I = MiniZinc::Item;
  switch (i->iid()) {
  case I::II_ASN: return use_ai_rhs || use_ai_decl;
  case I::II_VD: return use_vdi;
  case I::II_CON: return use_ci;
  case I::II_FUN: return use_fi_body || use_fi_params || use_fi_return;
  case I::II_INC: return use_ii;
  case I::II_OUT: return use_oi;
  case I::II_SOL: return use_si;
  default: return false;
  };
}

bool SearchLocs::any() const {
  return use_ii || use_vdi || use_ai_rhs || use_ai_decl || use_ci || use_si || use_oi ||
         use_fi_body || use_fi_params || use_fi_return;
}

bool SearchNode::match(const MiniZinc::Expression *i) const {
  bool right_expr = i->eid() == target;
  if (right_expr && target == ExpressionId::E_BINOP) {
    return std::get<BinOpType>(sub_target) == i->cast<MiniZinc::BinOp>()->op();
  }
  if (right_expr && target == ExpressionId::E_UNOP) {
    return std::get<UnOpType>(sub_target) == i->cast<MiniZinc::UnOp>()->op();
  }
  return right_expr;
}

bool ExprSearcher::next() {
  while (!dfs_stack.empty()) {
    const MiniZinc::Expression *cur = dfs_stack.back();
    dfs_stack.pop_back();

    if (!path.empty() && path.back() == cur) {
      path.pop_back();
      if (!hits.empty() && hits.back() == cur) {
        hits.pop_back();
        --nodes_pos;
        if (nodes[nodes_pos].is_under()) {
          path.push_back(cur);
          dfs_stack.push_back(cur);
          children_of(cur, dfs_stack);
        }
      }
      continue;
    }

    const SearchNode &tar = nodes[nodes_pos];
    if (tar.match(cur)) {
      hits.push_back(cur);
      ++nodes_pos;
    } else {
      if (tar.is_direct()) {
        continue;
      }
    }

    path.push_back(cur);
    dfs_stack.push_back(cur);
    if (!has_result() || nodes.back().is_under()) {
      children_of(cur, dfs_stack);
    }

    if (has_result())
      return true;
  }
  return false;
}

const MiniZinc::Expression &ExprSearcher::capture(std::size_t n) const {
  assert(hits.size() == nodes.size());
  assert(has_result());
  assert(n < hits.size());

  for (std::size_t i = 0; i < hits.size(); ++i) {
    if (nodes[i].capturable()) {
      if (n == 0) {
        return *hits[i];
      }
      --n;
    }
  }
  throw std::logic_error("n is larger than the number of captures");
}

bool ExprSearcher::has_result() const noexcept {
  return nodes_pos == nodes.size();
}

bool ExprSearcher::is_searching() const noexcept {
  return !dfs_stack.empty();
}

void ExprSearcher::new_search(const MiniZinc::Expression *e) {
  abort();
  dfs_stack.push_back(e);
}

void ExprSearcher::abort() {
  dfs_stack.clear();
  path.clear();
  hits.clear();
  nodes_pos = 0;
}

bool ModelSearcher::next_starting_point() {
  assert(item_queue != item_queue_end);
  using I = MiniZinc::Item;

  MiniZinc::Item *cur = *item_queue;
  MiniZinc::Expression *next = nullptr;

  switch (cur->iid()) {

  case I::II_FUN: {
    auto f = cur->cast<MiniZinc::FunctionI>();
    if (item_child == 0) {
      if (search.locations.use_fi_body) {
        next = f->e();
      }
    } else if (item_child == 1) {
      if (search.locations.use_fi_return) {
        next = f->ti();
      }
    } else if (item_child - 2 < f->params().size()) {
      if (search.locations.use_fi_params) {
        next = f->params()[item_child - 2];
      }
    } else {
      return false;
    }
    break;
  }

  case I::II_ASN: {
    auto f = cur->cast<MiniZinc::AssignI>();
    if (item_child == 0) {
      if (search.locations.use_ai_rhs) {
        next = f->e();
      }
    } else if (item_child == 1) {
      if (search.locations.use_ai_decl) {
        next = f->decl();
      }
    } else {
      return false;
    }
    break;
  }

  case I::II_VD:
    if (item_child != 0) {
      return false;
    }
    next = cur->cast<MiniZinc::VarDeclI>()->e();
    break;
  case I::II_CON:
    if (item_child != 0) {
      return false;
    }
    next = cur->cast<MiniZinc::ConstraintI>()->e();
    break;
  case I::II_OUT:
    if (item_child != 0) {
      return false;
    }
    next = cur->cast<MiniZinc::OutputI>()->e();
    break;
  case I::II_SOL:
    if (item_child != 0) {
      return false;
    }
    next = cur->cast<MiniZinc::SolveI>()->e();
    break;

  default: return false;
  };

  ++item_child;
  if (next == nullptr) {
    return next_starting_point();
  } else {
    expr_searcher->new_search(next);
    return true;
  }
}

bool ModelSearcher::next_item() {
  assert(item_queue != item_queue_end);
  for (++item_queue, item_child = 0; item_queue != item_queue_end; ++item_queue, item_child = 0) {
    MiniZinc::Item *cur = *item_queue;
    if (search.locations.should_visit(cur))
      return true;
  }
  return false;
}

bool ModelSearcher::is_items_only() const noexcept {
  return search.nodes.empty();
}

ModelSearcher::ModelSearcher(const MiniZinc::Model *m, const Search &search)
    : model(m), search(search), item_queue(m->begin()), item_queue_end(m->end()), item_child(0) {
  for (; item_queue != item_queue_end && !search.locations.should_visit(*item_queue);
       ++item_queue) {}
  if (!search.nodes.empty()) {
    expr_searcher.emplace(search.nodes);
  }
}

} // namespace LZN::Impl

namespace LZN {

bool Search::ModelSearcher::next() {
  if (item_queue == item_queue_end)
    return false;

  if (is_items_only()) {
    return next_item();
  }

  assert(expr_searcher);

  if (!expr_searcher->is_searching()) {
    while (!next_starting_point()) {
      if (!next_item()) {
        return false;
      }
    }
  }

  expr_searcher->next();
  if (expr_searcher->has_result())
    return true;

  return next();
}

const MiniZinc::Item *Search::ModelSearcher::cur_item() const noexcept {
  if (item_queue == item_queue_end)
    return nullptr;
  return *item_queue;
}

const MiniZinc::Expression &Search::ModelSearcher::capture(std::size_t n) const {
  assert(!is_items_only());
  assert(expr_searcher);
  return expr_searcher->capture(n);
}
} // namespace LZN
