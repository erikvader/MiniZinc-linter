#include <catch2/catch.hpp>
#include <linter/searcher.hpp>
#include <minizinc/ast.hh>
#include <minizinc/gc.hh>
#include <minizinc/parser.hh>

// TODO: remove
#include <minizinc/prettyprinter.hh>

namespace {
using LZN::Search;
using LZN::SearchBuilder;
using LZN::Impl::ExprSearcher;
using LZN::Impl::SearchNode;
using Attachement = LZN::Impl::SearchNode::Attachement;
using MiniZinc::Expression;
using ExpressionId = MiniZinc::Expression::ExpressionId;
using MiniZinc::BinOp;
using MiniZinc::BinOpType;
using MiniZinc::IntLit;
using MiniZinc::IntVal;
using MiniZinc::UnOp;
using MiniZinc::UnOpType;

const MiniZinc::Location nowhere;

MiniZinc::Model *parse(const char *file_contents) {
  std::stringstream errstream;
  MiniZinc::Env env;
  MiniZinc::Model *m = MiniZinc::parse(env, {}, {}, file_contents, "model_name", {}, false, true,
                                       false, false, errstream);
  UNSCOPED_INFO("MiniZinc::parse printed some error");
  char buf;
  REQUIRE(errstream.readsome(&buf, 1) == 0);
  return m;
}

template <typename T>
std::size_t number_of_results(T &es) {
  std::size_t count = 0;
  while (es.next()) {
    ++count;
  }
  return count;
}

template <typename T, std::size_t Inner, std::size_t Outer>
using array2D = std::array<std::array<T, Inner>, Outer>;

template <std::size_t NumRes, std::size_t NumCaps>
void expect_these(ExprSearcher &searcher, array2D<const Expression *, NumCaps, NumRes> expected) {
  array2D<const Expression *, NumCaps, NumRes> results;

  for (std::size_t i = 0; i < NumRes; ++i) {
    REQUIRE(searcher.next());
    for (std::size_t j = 0; j < NumCaps; ++j) {
      results[i][j] = searcher.capture(j);
    }
  }

  CHECK(!searcher.next());
  std::sort(expected.begin(), expected.end());
  std::sort(results.begin(), results.end());
  CHECK(expected == results);
}
} // namespace

TEST_CASE("ExprSearcher single node tree", "[util]") {
  std::vector<SearchNode> path;
  SECTION("direct") { path.emplace_back(Attachement::direct, ExpressionId::E_INTLIT); }
  SECTION("under") { path.emplace_back(Attachement::under, ExpressionId::E_INTLIT); }
  ExprSearcher searcher(path);

  CHECK(!searcher.has_result());
  CHECK(!searcher.is_searching());

  Expression *e = IntLit::a(IntVal(42));
  searcher.new_search(e);
  CHECK(!searcher.has_result());
  CHECK(searcher.is_searching());

  CHECK(searcher.next());
  CHECK(searcher.has_result());
  CHECK(searcher.is_searching());
  CHECK_THROWS_WITH(searcher.capture(0), "n is larger than the number of captures");
  CHECK_THROWS_WITH(searcher.capture(1), "n is larger than the number of captures");

  CHECK(!searcher.next());
  CHECK(!searcher.has_result());
  CHECK(!searcher.is_searching());

  CHECK(!searcher.next());
  CHECK(!searcher.has_result());
  CHECK(!searcher.is_searching());

  searcher.abort();
  CHECK(!searcher.has_result());
  CHECK(!searcher.is_searching());
}

TEST_CASE("ExprSearcher non-matching", "[util]") {
  std::vector<SearchNode> path;
  path.emplace_back(Attachement::under, ExpressionId::E_UNOP);
  ExprSearcher searcher(path);

  Expression *e = nullptr;
  {
    MiniZinc::GCLock lock;
    SECTION("one int") { e = IntLit::a(IntVal(42)); }
    SECTION("one addition") {
      Expression *i1 = IntLit::a(IntVal(1));
      Expression *i2 = IntLit::a(IntVal(2));
      e = new BinOp(nowhere, i1, BinOpType::BOT_PLUS, i2);
    }
  }
  searcher.new_search(e);
  CHECK(!searcher.has_result());
  CHECK(searcher.is_searching());

  CHECK(!searcher.next());
  CHECK(!searcher.has_result());
  CHECK(!searcher.is_searching());
}

TEST_CASE("ExprSearcher big tree", "[util]") {
  Expression *root = nullptr;
  Expression *int1 = IntLit::a(IntVal(1));
  Expression *int5 = IntLit::a(IntVal(5));
  Expression *int3 = IntLit::a(IntVal(3));
  Expression *int15 = IntLit::a(IntVal(15));
  Expression *int420 = IntLit::a(IntVal(420));
  Expression *plus1 = nullptr;
  Expression *plus2 = nullptr;
  Expression *plus3 = nullptr;
  Expression *neg1 = nullptr;
  Expression *equals1 = nullptr;
  {
    MiniZinc::GCLock lock;
    plus1 = new BinOp(nowhere, int1, BinOpType::BOT_PLUS, int5);
    plus2 = new BinOp(nowhere, int3, BinOpType::BOT_PLUS, int15);
    plus3 = new BinOp(nowhere, plus1, BinOpType::BOT_PLUS, plus2);
    neg1 = new UnOp(nowhere, UnOpType::UOT_MINUS, int420);
    equals1 = new BinOp(nowhere, plus3, BinOpType::BOT_EQ, neg1);
    root = equals1;
  }

  std::vector<SearchNode> path;

  SECTION("single top") {
    path.emplace_back(Attachement::direct, BinOpType::BOT_EQ, true);
    ExprSearcher searcher(path);
    searcher.new_search(root);
    REQUIRE(searcher.next());
    CHECK(searcher.capture(0) == root);
    CHECK(!searcher.next());
  }

  SECTION("single anywhere") {
    path.emplace_back(Attachement::under, BinOpType::BOT_PLUS, true);
    ExprSearcher searcher(path);
    searcher.new_search(root);
    CHECK(number_of_results(searcher) == 3);
  }

  SECTION("double direct direct") {
    path.emplace_back(Attachement::direct, BinOpType::BOT_EQ, true);
    path.emplace_back(Attachement::direct, BinOpType::BOT_PLUS, true);
    ExprSearcher searcher(path);
    searcher.new_search(root);
    REQUIRE(searcher.next());
    CHECK(searcher.capture(0) == root);
    CHECK(searcher.capture(1) == plus3);
    CHECK_THROWS_WITH(searcher.capture(2), "n is larger than the number of captures");
    CHECK(!searcher.next());
  }

  SECTION("double direct under") {
    path.emplace_back(Attachement::direct, BinOpType::BOT_EQ, true);
    path.emplace_back(Attachement::under, BinOpType::BOT_PLUS, true);
    ExprSearcher searcher(path);
    searcher.new_search(root);
    expect_these<3, 2>(searcher, {{{root, plus1}, {root, plus2}, {root, plus3}}});
  }

  SECTION("double under direct") {
    path.emplace_back(Attachement::under, BinOpType::BOT_PLUS, true);
    path.emplace_back(Attachement::direct, BinOpType::BOT_PLUS, true);
    ExprSearcher searcher(path);
    searcher.new_search(root);
    expect_these<2, 2>(searcher, {{{plus3, plus2}, {plus3, plus1}}});
  }

  SECTION("double under under") {
    path.emplace_back(Attachement::under, BinOpType::BOT_PLUS, true);
    path.emplace_back(Attachement::under, ExpressionId::E_INTLIT, true);
    ExprSearcher searcher(path);
    searcher.new_search(root);
    expect_these<8, 2>(searcher, {{{plus3, int1},
                                   {plus3, int3},
                                   {plus3, int5},
                                   {plus3, int15},
                                   {plus2, int15},
                                   {plus2, int3},
                                   {plus1, int1},
                                   {plus1, int5}}});
  }
}

TEST_CASE("model searcher items only", "[util]") {
  MiniZinc::Model *m = parse("var int: x;"
                             "constraint 1+2+3+4+5 = x;"
                             "constraint 1 = 2;"
                             "solve satisfy;");

  SECTION("constraints") {
    Search s = SearchBuilder().in_constraint().build();
    auto ms = s.search(m);
    CHECK(number_of_results(ms) == 2);
  }

  SECTION("vars") {
    Search s = SearchBuilder().in_vardecl().build();
    auto ms = s.search(m);
    CHECK(number_of_results(ms) == 1);
  }

  SECTION("all") {
    Search s = SearchBuilder().in_vardecl().in_solve().in_constraint().build();
    auto ms = s.search(m);
    CHECK(number_of_results(ms) == 4);
  }
}

TEST_CASE("model searcher with ExprSearcher", "[util]") {
  MiniZinc::Model *m = parse("constraint 1+2+3+4+5 = x;");
  Search s = SearchBuilder().in_constraint().direct(BinOpType::BOT_EQ).capture().build();
  auto ms = s.search(m);

  CHECK(ms.cur_item() == nullptr);

  REQUIRE(ms.next());
  CHECK(ms.cur_item() != nullptr);
  CHECK(ms.capture(0) != nullptr);

  REQUIRE(!ms.next());
  CHECK(ms.cur_item() == nullptr);
}

TEST_CASE("model searcher empty builder", "[util]") {
  MiniZinc::Model *m = parse("constraint 1+2+3+4+5 = x;");
  Search s = SearchBuilder().build();
  auto ms = s.search(m);
  CHECK(ms.cur_item() == nullptr);
  CHECK(!ms.next());
  CHECK(ms.cur_item() == nullptr);
}

TEST_CASE("model searcher function", "[util]") {
  MiniZinc::Model *m = parse("function var int: f(int: x, var int: y) = x = y;");

  SECTION("all") {
    Search s = SearchBuilder().in_function().under(ExpressionId::E_TI).build();
    auto ms = s.search(m);
    CHECK(number_of_results(ms) == 3);
  }

  SECTION("return") {
    Search s = SearchBuilder().in_function_return().under(ExpressionId::E_TI).build();
    auto ms = s.search(m);
    CHECK(number_of_results(ms) == 1);
  }

  SECTION("parameters") {
    Search s = SearchBuilder().in_function_params().under(ExpressionId::E_TI).build();
    auto ms = s.search(m);
    CHECK(number_of_results(ms) == 2);
  }

  SECTION("body") {
    Search s = SearchBuilder().in_function_body().under(ExpressionId::E_TI).build();
    auto ms = s.search(m);
    CHECK(number_of_results(ms) == 0);
  }
}
