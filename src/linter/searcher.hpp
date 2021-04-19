#pragma once
#include <linter/rustiter.hpp>
#include <minizinc/ast.hh>
#include <minizinc/model.hh>
#include <stack>
#include <variant>

namespace LZN {

using ExprFilterFun = bool (*)(const MiniZinc::Expression *root, const MiniZinc::Expression *child);

bool filter_out_annotations(const MiniZinc::Expression *root, const MiniZinc::Expression *child);
bool filter_out_vardecls(const MiniZinc::Expression *root, const MiniZinc::Expression *child);
bool filter_arrayaccess_name(const MiniZinc::Expression *root, const MiniZinc::Expression *child);
bool filter_arrayaccess_idx(const MiniZinc::Expression *root, const MiniZinc::Expression *child);
bool filter_comprehension_expr(const MiniZinc::Expression *root, const MiniZinc::Expression *child);

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
  bool iters_pushed;
  std::stack<std::pair<MiniZinc::Model::const_iterator, MiniZinc::Model::const_iterator>> iters;
  std::size_t item_child;

  ModelSearcher(const MiniZinc::Model *m, const Search &search);

public:
  ModelSearcher(const ModelSearcher &) = delete;
  ModelSearcher &operator=(const ModelSearcher &) = delete;
  ModelSearcher(ModelSearcher &&) = default;
  ModelSearcher &operator=(ModelSearcher &&) = delete;

protected:
  bool next_starting_point();
  bool next_item();
  bool is_items_only() const noexcept;
  void advance_iters();
  const MiniZinc::Item *iters_top() const;
  void iters_push(const MiniZinc::Model *);
};

} // namespace LZN::Impl

namespace LZN {

class Search {
  std::vector<Impl::SearchNode> nodes;
  Impl::SearchLocs locations;
  std::size_t numcaptures;
  std::vector<ExprFilterFun> global_filters;
  const std::vector<std::string> *includePath;
  bool recursive;

  Search(std::vector<Impl::SearchNode> nodes, Impl::SearchLocs locations, std::size_t numcaptures,
         std::vector<ExprFilterFun> global_filters, const std::vector<std::string> *includePath,
         bool recursive)
      : nodes(std::move(nodes)), locations(std::move(locations)), numcaptures(numcaptures),
        global_filters(std::move(global_filters)), includePath(includePath), recursive(recursive) {}

  friend class SearchBuilder;
  friend class Impl::ModelSearcher;

public:
  class ModelSearcher : private Impl::ModelSearcher {
    friend Search;

    using Impl::ModelSearcher::ModelSearcher;

  public:
    bool next();
    const MiniZinc::Item *cur_item() const noexcept;
    const MiniZinc::Expression *capture(std::size_t n) const;
    void skip_item();

    template <typename T>
    const T *capture_cast(std::size_t n) const {
      return capture(n)->cast<T>();
    }
  };

  class ExpressionSearcher : private Impl::ExprSearcher {
    friend Search;

  private:
    ExpressionSearcher(const std::vector<Impl::SearchNode> &nodes,
                       const std::vector<ExprFilterFun> *global_filters,
                       const MiniZinc::Expression *e)
        : Impl::ExprSearcher(nodes, global_filters) {
      new_search(e);
    }

  public:
    bool next() { return Impl::ExprSearcher::next(); }
    const MiniZinc::Expression *capture(std::size_t n) const {
      return Impl::ExprSearcher::capture(n);
    }
    template <typename T>
    const T *capture_cast(std::size_t n) const {
      return capture(n)->cast<T>();
    }
  };

  ModelSearcher search(const MiniZinc::Model *m) const & { return ModelSearcher(m, *this); }
  ModelSearcher search(const MiniZinc::Model *) && = delete;
  ExpressionSearcher search(const MiniZinc::Expression *e) const & {
    return ExpressionSearcher(nodes, &global_filters, e);
  }
  ExpressionSearcher search(const MiniZinc::Expression *) && = delete;

  bool is_user_defined_include(const MiniZinc::IncludeI *) const noexcept;
  bool is_recursive() const noexcept;
  bool is_user_defined_only() const noexcept { return includePath != nullptr; }
  const std::vector<std::string> *include_path() const noexcept { return includePath; };
};

class SearchBuilder {
  std::vector<Impl::SearchNode> nodes;
  Impl::SearchLocs locations;
  std::size_t numcaptures = 0;
  std::vector<ExprFilterFun> global_filters;
  const std::vector<std::string> *includePath = nullptr;
  bool _recursive = false;

  using Attach = Impl::SearchNode::Attachement;

public:
  using ExpressionId = MiniZinc::Expression::ExpressionId;
  using BinOpType = MiniZinc::BinOpType;
  using UnOpType = MiniZinc::UnOpType;

  SearchBuilder() = default;

  SearchBuilder &only_user_defined(const std::vector<std::string> &standard_lib_include_path) {
    includePath = &standard_lib_include_path;
    return *this;
  }

  SearchBuilder &recursive(bool r = true) {
    _recursive = r;
    return *this;
  }

  SearchBuilder &in_include(bool visit = true) {
    locations.use_ii = visit;
    return *this;
  }
  SearchBuilder &in_constraint(bool visit = true) {
    locations.use_ci = visit;
    return *this;
  }
  SearchBuilder &in_function_body(bool visit = true) {
    locations.use_fi_body = visit;
    return *this;
  }
  SearchBuilder &in_function_params(bool visit = true) {
    locations.use_fi_params = visit;
    return *this;
  }
  SearchBuilder &in_function_return(bool visit = true) {
    locations.use_fi_return = visit;
    return *this;
  }
  SearchBuilder &in_function(bool visit = true) {
    return in_function_body(visit).in_function_params(visit).in_function_return(visit);
  }
  SearchBuilder &in_vardecl(bool visit = true) {
    locations.use_vdi = visit;
    return *this;
  }
  SearchBuilder &in_assign_rhs(bool visit = true) {
    locations.use_ai_rhs = visit;
    return *this;
  }
  SearchBuilder &in_assign_decl(bool visit = true) {
    locations.use_ai_decl = visit;
    return *this;
  }
  SearchBuilder &in_assign(bool visit = true) { return in_assign_rhs(visit).in_assign_decl(visit); }
  SearchBuilder &in_solve(bool visit = true) {
    locations.use_si = visit;
    return *this;
  }
  SearchBuilder &in_output(bool visit = true) {
    locations.use_oi = visit;
    return *this;
  }
  SearchBuilder &in_everywhere() {
    return in_include()
        .in_constraint()
        .in_function()
        .in_vardecl()
        .in_assign()
        .in_solve()
        .in_output();
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
    return Search(std::move(nodes), std::move(locations), numcaptures, std::move(global_filters),
                  includePath, _recursive);
  }
};
} // namespace LZN
