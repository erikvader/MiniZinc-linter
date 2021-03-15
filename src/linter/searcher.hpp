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

class Search;

} // namespace LZN

namespace LZN::Impl {

struct SearchLocs {
  bool use_ii = false, use_vdi = false, use_ci = false, use_si = false, use_oi = false;
  bool use_fi_body = false, use_fi_params = false, use_fi_return = false;
  bool use_ai_rhs = false, use_ai_decl = false;

  bool should_visit(const MiniZinc::Item *i) const;
  bool any() const;
};

class SearchNode {
public:
  enum class Attachement { direct, under };

private:
  Attachement att;
  ExpressionId target;
  std::variant<std::monostate, BinOpType, UnOpType> sub_target;
  bool be_captured;

public:
  SearchNode(Attachement attachement, ExpressionId target)
      : att(attachement), target(target), sub_target(std::monostate()), be_captured(false) {}
  SearchNode(Attachement attachement, BinOpType bin_target)
      : att(attachement), target(ExpressionId::E_BINOP), sub_target(bin_target),
        be_captured(false) {}
  SearchNode(Attachement attachement, UnOpType un_target)
      : att(attachement), target(ExpressionId::E_UNOP), sub_target(un_target), be_captured(false) {}

  bool match(const MiniZinc::Expression *) const;
  bool capturable() const noexcept { return be_captured; }
  void capturable(bool b) noexcept { be_captured = b; }
  bool is_direct() const noexcept { return att == Attachement::direct; }
  bool is_under() const noexcept { return !is_direct(); }
};

class ExprSearcher {
  const std::vector<SearchNode> &nodes;
  std::vector<const MiniZinc::Expression *> path;
  std::vector<const MiniZinc::Expression *> dfs_stack;
  std::vector<const MiniZinc::Expression *> hits; // TODO: heap allocated array instead?
  std::size_t nodes_pos;

public:
  ExprSearcher(const std::vector<SearchNode> &nodes) : nodes(nodes), nodes_pos(0) {
    assert(!nodes.empty());
    hits.reserve(nodes.size());
  }
  bool has_result() const noexcept;
  bool is_searching() const noexcept;
  const MiniZinc::Expression &capture(std::size_t n) const;
  void new_search(const MiniZinc::Expression *);
  void abort();
  bool next();
};

class ModelSearcher {
protected:
  const MiniZinc::Model *model;
  const Search &search;

  std::optional<ExprSearcher> expr_searcher;
  MiniZinc::Model::const_iterator item_queue;
  const MiniZinc::Model::const_iterator item_queue_end;
  std::size_t item_child;

protected:
  ModelSearcher(const MiniZinc::Model *m, const Search &search);
  ModelSearcher(const MiniZinc::Expression *expr, const Search &search)
      : model(nullptr), search(search) {
    throw std::logic_error("not implemented");
  };

public:
  ModelSearcher(const ModelSearcher &) = delete;
  ModelSearcher &operator=(const ModelSearcher &) = delete;
  ModelSearcher(ModelSearcher &&) = default;

protected:
  bool next_starting_point();
  bool next_item();
  bool is_items_only() const noexcept;
};

} // namespace LZN::Impl

namespace LZN {

class Search {
  std::vector<Impl::SearchNode> nodes;
  Impl::SearchLocs locations;
  std::size_t numcaptures;

  Search(std::vector<Impl::SearchNode> nodes, Impl::SearchLocs locations, std::size_t numcaptures)
      : nodes(std::move(nodes)), locations(std::move(locations)), numcaptures(numcaptures) {}

  friend class SearchBuilder;
  friend class Impl::ModelSearcher;

  class ModelSearcher : private Impl::ModelSearcher {
    friend Search;

    template <typename... Args>
    ModelSearcher(Args &&...args) : Impl::ModelSearcher(std::forward<Args>(args)...) {}

  public:
    bool next();
    const MiniZinc::Item *cur_item() const noexcept;
    const MiniZinc::Expression &capture(std::size_t n) const;
  };

public:
  ModelSearcher search(const MiniZinc::Model *m) const { return ModelSearcher(m, *this); }
  // TODO: have this? For chaining?
  ModelSearcher search(const MiniZinc::Expression *e) const { return ModelSearcher(e, *this); }
};

class SearchBuilder {
  std::vector<Impl::SearchNode> nodes;
  Impl::SearchLocs locations;
  std::size_t numcaptures;

  using Attach = Impl::SearchNode::Attachement;

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
  SearchBuilder &in_function_body() {
    locations.use_fi_body = true;
    return *this;
  }
  SearchBuilder &in_function_params() {
    locations.use_fi_params = true;
    return *this;
  }
  SearchBuilder &in_function_return() {
    locations.use_fi_return = true;
    return *this;
  }
  SearchBuilder &in_function() {
    return in_function_body().in_function_params().in_function_return();
  }
  SearchBuilder &in_vardecl() {
    locations.use_vdi = true;
    return *this;
  }
  SearchBuilder &in_assign_rhs() {
    locations.use_ai_rhs = true;
    return *this;
  }
  SearchBuilder &in_assign_decl() {
    locations.use_ai_decl = true;
    return *this;
  }
  SearchBuilder &in_assign() { return in_assign_rhs().in_assign_decl(); }
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
    nodes.emplace_back(Attach::direct, eid);
    return *this;
  }
  SearchBuilder &direct(BinOpType bot) {
    nodes.emplace_back(Attach::direct, bot);
    return *this;
  }
  SearchBuilder &direct(UnOpType uot) {
    nodes.emplace_back(Attach::direct, uot);
    return *this;
  }

  SearchBuilder &under(ExpressionId eid) {
    nodes.emplace_back(Attach::under, eid);
    return *this;
  }
  SearchBuilder &under(BinOpType bot) {
    nodes.emplace_back(Attach::under, bot);
    return *this;
  }
  SearchBuilder &under(UnOpType uot) {
    nodes.emplace_back(Attach::under, uot);
    return *this;
  }

  SearchBuilder &capture() {
    if (nodes.empty())
      throw std::logic_error("there is nothing to capture");
    ++numcaptures;
    nodes.back().capturable(true);
    return *this;
  }

  Search build() { return Search(std::move(nodes), std::move(locations), numcaptures); }
};
} // namespace LZN
