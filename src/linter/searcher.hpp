#pragma once
#include <linter/rustiter.hpp>
#include <minizinc/ast.hh>
#include <minizinc/model.hh>
#include <variant>

namespace LZN {
using ExpressionId = MiniZinc::Expression::ExpressionId;
using MiniZinc::BinOpType;
using ItemId = MiniZinc::Item::ItemId;
using MiniZinc::UnOpType;

struct SearchLocs {
  bool use_ii = false, use_vdi = false, use_ai = false, use_ci = false, use_si = false,
       use_oi = false, use_fi = false;
  int fi_flags = 0, ai_flags = 0;

  enum {
    fi_body = 0x1,
    fi_params = 0x2,
    fi_return = 0x4,
    fi_all = fi_body | fi_params | fi_return
  };
  enum { ai_rhs = 0x1, ai_decl = 0x2, ai_all = ai_rhs | ai_decl };

  bool visit_item(const MiniZinc::Item *i) const;
  bool any() const;
};

class SearchNode {
  enum Attachement { a_direct, a_under };

  Attachement att;
  ExpressionId target;
  std::variant<std::monostate, BinOpType, UnOpType> sub_target;
  bool be_captured;

  SearchNode(Attachement attachement, ExpressionId target)
      : att(attachement), target(target), sub_target(std::monostate()), be_captured(false) {}
  SearchNode(Attachement attachement, BinOpType bin_target)
      : att(attachement), target(ExpressionId::E_BINOP), sub_target(bin_target),
        be_captured(false) {}
  SearchNode(Attachement attachement, UnOpType un_target)
      : att(attachement), target(ExpressionId::E_UNOP), sub_target(un_target), be_captured(false) {}

  friend class SearchBuilder;

public:
  bool match(const MiniZinc::Expression *) const;
  bool capturable() const noexcept { return be_captured; }
  bool is_direct() const noexcept { return att == Attachement::a_direct; }
  bool is_under() const noexcept { return !is_direct(); }
};

class Search {
  std::vector<SearchNode> nodes;
  SearchLocs locations;
  std::size_t numcaptures;

  Search(std::vector<SearchNode> nodes, SearchLocs locations, std::size_t numcaptures)
      : nodes(std::move(nodes)), locations(std::move(locations)), numcaptures(numcaptures) {}

  friend class SearchBuilder;
  friend class ModelSearcher;

public:
  class ModelSearcher {
  public: // TODO: remove
    const MiniZinc::Model *model;
    const Search &search;

    MiniZinc::Model::const_iterator item_queue;
    const MiniZinc::Model::const_iterator item_queue_end;
    std::size_t item_child;

    std::vector<MiniZinc::Expression *> path;
    std::vector<MiniZinc::Expression *> dfs_stack;

    std::vector<MiniZinc::Expression *> hits; // TODO: heap allocated array instead?
    std::size_t nodes_pos;

    friend Search;
    ModelSearcher(const MiniZinc::Model *m, const Search &search);
    ModelSearcher(const MiniZinc::Expression *expr, const Search &search)
        : model(nullptr), search(search) {
      throw std::logic_error("not implemented");
    };

    bool next_starting_point();
    bool next_item();
    void search_cur_item();
    bool items_only() const noexcept;
    bool all_hits() const noexcept;

  public:
    ModelSearcher(const ModelSearcher &) = delete;
    ModelSearcher &operator=(const ModelSearcher &) = delete;
    ModelSearcher(ModelSearcher &&) = default;

    bool next();
    const MiniZinc::Item *inside_item() const noexcept;
    const MiniZinc::Expression &capture(std::size_t n) const;
  };

  ModelSearcher search(const MiniZinc::Model *m) const { return ModelSearcher(m, *this); }
  // TODO: have this? For chaining?
  ModelSearcher search(const MiniZinc::Expression *e) const { return ModelSearcher(e, *this); }
};

class SearchBuilder {
  std::vector<SearchNode> nodes;
  SearchLocs locations;
  std::size_t numcaptures;

public:
  using ExprFilterFun =
      std::function<bool(const MiniZinc::Expression *, const MiniZinc::Expression *)>;

  SearchBuilder() : numcaptures(0) {}

  SearchBuilder &in_include() {
    locations.use_ii = true;
    return *this;
  }
  SearchBuilder &in_constraint() {
    locations.use_ci = true;
    return *this;
  }
  SearchBuilder &in_function(int flags = SearchLocs::fi_all) {
    locations.use_fi = true;
    locations.fi_flags = flags;
    return *this;
  }
  SearchBuilder &in_vardecl() {
    locations.use_vdi = true;
    return *this;
  }
  SearchBuilder &in_assign(int flags = SearchLocs::ai_all) {
    locations.use_ai = true;
    locations.ai_flags = flags;
    return *this;
  }
  SearchBuilder &in_solve() {
    locations.use_si = true;
    return *this;
  }
  SearchBuilder &in_output() {
    locations.use_oi = true;
    return *this;
  }

  // TODO: use to select more precisely what parts of all kinds of expressions to search in.
  SearchBuilder &filter(ExprFilterFun f) { return *this; }

  SearchBuilder &direct(ExpressionId eid) {
    nodes.push_back(SearchNode(SearchNode::a_direct, eid));
    return *this;
  }
  SearchBuilder &direct(BinOpType bot) {
    nodes.push_back(SearchNode(SearchNode::a_direct, bot));
    return *this;
  }
  SearchBuilder &direct(UnOpType uot) {
    nodes.push_back(SearchNode(SearchNode::a_direct, uot));
    return *this;
  }

  SearchBuilder &under(ExpressionId eid) {
    nodes.push_back(SearchNode(SearchNode::a_under, eid));
    return *this;
  }
  SearchBuilder &under(BinOpType bot) {
    nodes.push_back(SearchNode(SearchNode::a_under, bot));
    return *this;
  }
  SearchBuilder &under(UnOpType uot) {
    nodes.push_back(SearchNode(SearchNode::a_under, uot));
    return *this;
  }

  SearchBuilder &capture() {
    if (nodes.empty())
      throw std::logic_error("there is nothing to capture");
    ++numcaptures;
    nodes.back().be_captured = true;
    return *this;
  }

  Search build() { return Search(std::move(nodes), std::move(locations), numcaptures); }
};
} // namespace LZN
