#pragma once
#include <linter/rustiter.hpp>
#include <minizinc/ast.hh>
#include <minizinc/model.hh>
#include <variant>

namespace LZN {

using ExprFilterFun = bool (*)(const MiniZinc::Expression *root, const MiniZinc::Expression *child);

bool filter_out_annotations(const MiniZinc::Expression *root, const MiniZinc::Expression *child);

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

  using ExpressionId = MiniZinc::Expression::ExpressionId;
  using BinOpType = MiniZinc::BinOpType;
  using UnOpType = MiniZinc::UnOpType;

private:
  Attachement att;
  ExpressionId target;
  std::variant<std::monostate, BinOpType, UnOpType> sub_target;
  bool be_captured;
  std::optional<ExprFilterFun> filter_fun;

public:
  SearchNode(Attachement attachement, ExpressionId target, bool be_captured = false)
      : att(attachement), target(target), sub_target(std::monostate()), be_captured(be_captured) {}
  SearchNode(Attachement attachement, BinOpType bin_target, bool be_captured = false)
      : att(attachement), target(ExpressionId::E_BINOP), sub_target(bin_target),
        be_captured(be_captured) {}
  SearchNode(Attachement attachement, UnOpType un_target, bool be_captured = false)
      : att(attachement), target(ExpressionId::E_UNOP), sub_target(un_target),
        be_captured(be_captured) {}

  bool match(const MiniZinc::Expression *) const;
  bool capturable() const noexcept { return be_captured; }
  void capturable(bool b) noexcept { be_captured = b; }
  bool is_direct() const noexcept { return att == Attachement::direct; }
  bool is_under() const noexcept { return !is_direct(); }
  void filter(ExprFilterFun f) noexcept { filter_fun = f; }
  bool run_filter(const MiniZinc::Expression *p, const MiniZinc::Expression *child) const;
};

class ExprSearcher {
  const std::vector<SearchNode> &nodes;
  const std::vector<ExprFilterFun> *global_filters;
  std::vector<const MiniZinc::Expression *> path;
  std::vector<const MiniZinc::Expression *> dfs_stack;
  std::vector<const MiniZinc::Expression *> hits; // TODO: heap allocated array instead?
  std::size_t nodes_pos;

public:
  ExprSearcher(const std::vector<SearchNode> &nodes,
               const std::vector<ExprFilterFun> *global_filters = nullptr)
      : nodes(nodes), global_filters(global_filters), nodes_pos(0) {
    assert(!nodes.empty());
    hits.reserve(nodes.size());
  }
  bool has_result() const noexcept;
  bool is_searching() const noexcept;
  const MiniZinc::Expression *capture(std::size_t n) const;
  void new_search(const MiniZinc::Expression *);
  void abort();
  bool next();

private:
  void queue_children_of(const MiniZinc::Expression *cur);
};

class ModelSearcher {
protected:
  const MiniZinc::Model *model;
  const Search &search;

  std::optional<ExprSearcher> expr_searcher;
  std::optional<MiniZinc::Model::const_iterator> item_queue;
  const MiniZinc::Model::const_iterator item_queue_end;
  std::size_t item_child;

protected:
  ModelSearcher(const MiniZinc::Model *m, const Search &search);
  // TODO: implement this
  ModelSearcher(const MiniZinc::Expression *, const Search &search)
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
  std::vector<ExprFilterFun> global_filters;

  Search(std::vector<Impl::SearchNode> nodes, Impl::SearchLocs locations, std::size_t numcaptures,
         std::vector<ExprFilterFun> global_filters)
      : nodes(std::move(nodes)), locations(std::move(locations)), numcaptures(numcaptures),
        global_filters(std::move(global_filters)) {}

  bool should_visit(const MiniZinc::Expression *p, const MiniZinc::Expression *child) const;

  friend class SearchBuilder;
  friend class Impl::ModelSearcher;

public:
  class ModelSearcher : private Impl::ModelSearcher {
    friend Search;

    template <typename... Args>
    ModelSearcher(Args &&...args) : Impl::ModelSearcher(std::forward<Args>(args)...) {}

  public:
    bool next();
    const MiniZinc::Item *cur_item() const noexcept;
    const MiniZinc::Expression *capture(std::size_t n) const;

    template <typename F>
    void for_each(F f) {
      while (next()) {
        f(*this);
      }
    }
  };

  ModelSearcher search(const MiniZinc::Model *m) & { return ModelSearcher(m, *this); }
  // TODO: have this? For chaining?
  ModelSearcher search(const MiniZinc::Expression *e) & { return ModelSearcher(e, *this); }
};

class SearchBuilder {
  std::vector<Impl::SearchNode> nodes;
  Impl::SearchLocs locations;
  std::size_t numcaptures;
  std::vector<ExprFilterFun> global_filters;

  using Attach = Impl::SearchNode::Attachement;

public:
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  using BinOpType = MiniZinc::BinOpType;
  using UnOpType = MiniZinc::UnOpType;

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

  SearchBuilder &global_filter(ExprFilterFun f) {
    global_filters.push_back(f);
    return *this;
  }

  SearchBuilder &filter(ExprFilterFun f) {
    if (nodes.empty())
      throw std::logic_error("there is nothing to add a filter to");
    nodes.back().filter(f);
    return *this;
  }

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

  Search build() {
    return Search(std::move(nodes), std::move(locations), numcaptures, std::move(global_filters));
  }
};
} // namespace LZN
