#include <catch2/catch.hpp>
#include <linter/rustiter.hpp>
#include <optional>

class RangeIter {
  int upto;
  int x;

public:
  using Item = int;
  RangeIter(int upto) : upto(upto), x(0) {}

  std::optional<int> next() {
    if (x < upto) {
      return x++;
    }
    return std::nullopt;
  }
};

TEST_CASE("rust iterator", "[util]") {
  auto range = RangeIter(3);
  REQUIRE(range.next().value() == 0);
  REQUIRE(range.next().value() == 1);
  REQUIRE(range.next().value() == 2);
  REQUIRE(!range.next().has_value());
}

TEST_CASE("rust iterator adapter", "[util]") {
  const int max = 10;
  auto range = RangeIter(max);
  auto adapter = RustIterator<RangeIter>(max);

  for (int val : adapter) {
    auto next = range.next();
    REQUIRE(next.has_value());
    REQUIRE(*next == val);
  }

  REQUIRE(!range.next().has_value());
}

TEST_CASE("empty rust iterator adapter", "[util]") {
  auto adapter = RustIterator<RangeIter>(0);
  REQUIRE(adapter.begin() == adapter.end());
}
