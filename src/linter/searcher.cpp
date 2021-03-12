#include "searcher.hpp"
#include <minizinc/astiterator.hh>
#include <minizinc/model.hh>

// TODO: remove
#include <minizinc/prettyprinter.hh>

namespace {
using namespace LZN;

std::size_t children_of(const MiniZinc::Expression *root,
                        std::vector<MiniZinc::Expression *> &results) {
  assert(root != nullptr);

  struct : MiniZinc::EVisitor {
    std::vector<MiniZinc::Expression *> *results;
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

namespace LZN {

bool SearchLocs::visit_item(const MiniZinc::Item *i) const {
  assert(i != nullptr);
  using I = MiniZinc::Item;
  switch (i->iid()) {
  case I::II_ASN: return use_ai;
  case I::II_VD: return use_vdi;
  case I::II_CON: return use_ci;
  case I::II_FUN: return use_fi;
  case I::II_INC: return use_ii;
  case I::II_OUT: return use_oi;
  case I::II_SOL: return use_si;
  default: return false;
  };
}

bool SearchLocs::any() const {
  return use_ii || use_vdi || use_ai || use_ci || use_si || use_oi || use_fi;
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

bool Search::ModelSearcher::next_starting_point() {
  assert(item_queue != item_queue_end);
  using I = MiniZinc::Item;

  MiniZinc::Item *cur = *item_queue;
  MiniZinc::Expression *next = nullptr;

  switch (cur->iid()) {

  case I::II_FUN: {
    auto f = cur->cast<MiniZinc::FunctionI>();
    if (item_child == 0) {
      if (search.locations.fi_flags & SearchLocs::fi_body) {
        next = f->e();
      }
    } else if (item_child == 1) {
      if (search.locations.fi_flags & SearchLocs::fi_return || search.locations.fi_flags == 0) {
        next = f->ti();
      }
    } else if (item_child - 2 < f->params().size()) {
      if (search.locations.fi_flags & SearchLocs::fi_params) {
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
      if (search.locations.ai_flags & SearchLocs::ai_rhs || search.locations.ai_flags == 0) {
        next = f->e();
      }
    } else if (item_child == 1) {
      if (search.locations.ai_flags & SearchLocs::ai_decl) {
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
    dfs_stack.push_back(next);
    return true;
  }
}

bool Search::ModelSearcher::next_item() {
  assert(item_queue != item_queue_end);
  for (++item_queue, item_child = 0; item_queue != item_queue_end; ++item_queue, item_child = 0) {
    MiniZinc::Item *cur = *item_queue;
    if (search.locations.visit_item(cur))
      return true;
  }
  return false;
}

void Search::ModelSearcher::search_cur_item() {
  while (!dfs_stack.empty()) {
    MiniZinc::Expression *cur = dfs_stack.back();
    dfs_stack.pop_back();

    if (!path.empty() && path.back() == cur) {
      path.pop_back();
      if (!hits.empty() && hits.back() == cur) {
        hits.pop_back();
        --nodes_pos;
        if (search.nodes[nodes_pos].is_under()) {
          path.push_back(cur);
          dfs_stack.push_back(cur);
          children_of(cur, dfs_stack);
        }
      }
      continue;
    }

    const SearchNode &tar = search.nodes[nodes_pos];
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
    if (!all_hits() || search.nodes.back().is_under()) {
      children_of(cur, dfs_stack);
    }

    if (all_hits())
      return;
  }
}

bool Search::ModelSearcher::items_only() const noexcept {
  return search.nodes.empty();
}
bool Search::ModelSearcher::all_hits() const noexcept {
  assert(!items_only());
  return nodes_pos == search.nodes.size();
};

Search::ModelSearcher::ModelSearcher(const MiniZinc::Model *m, const Search &search)
    : model(m), search(search), item_queue(m->begin()), item_queue_end(m->end()), item_child(0),
      nodes_pos(0) {
  hits.reserve(search.nodes.size());
  for (; item_queue != item_queue_end && !search.locations.visit_item(*item_queue); ++item_queue) {}
}

bool Search::ModelSearcher::next() {
  if (item_queue == item_queue_end)
    return false;

  if (items_only()) {
    return next_item();
  }

  if (dfs_stack.empty()) {
    assert(path.empty());
    assert(hits.empty());
    assert(nodes_pos == 0);
    while (!next_starting_point()) {
      if (!next_item()) {
        return false;
      }
    }
  }

  search_cur_item();
  if (all_hits())
    return true;

  return next();
}

const MiniZinc::Item *Search::ModelSearcher::inside_item() const noexcept {
  assert(all_hits());
  if (item_queue == item_queue_end)
    return nullptr;
  return *item_queue;
}

const MiniZinc::Expression &Search::ModelSearcher::capture(std::size_t n) const {
  assert(hits.size() == search.nodes.size());
  assert(all_hits());
  assert(n >= 0 && n < hits.size());

  for (std::size_t i = 0; i < hits.size(); ++i) {
    if (search.nodes[i].capturable()) {
      if (n == 0) {
        return *hits[i];
      }
      --n;
    }
  }
  throw std::logic_error("n is larger than the number of captures");
}
} // namespace LZN
