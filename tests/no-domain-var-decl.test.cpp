#include <catch2/catch.hpp>
#include <linter/registry.hpp>

TEST_CASE("No domain on variable declarations", "[rule]") {
    const LZN::LintRule *rule;
    REQUIRE_NOTHROW(rule = LZN::Registry::get(42));
}
