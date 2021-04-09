#include "test_common.hpp"

TEST_CASE("variables in if and where", "[rule]") {
  LZN_TEST_CASE_INIT(26);

  SECTION("safe cases") {
    LZN_MODEL("constraint forall(i in 1..2 where i > 0)(true);"
              "constraint if 1 = 1 then true else false endif;");
    LZN_EXPECTED();
  }

  SECTION("bad generator") {
    LZN_MODEL("var int: x;\n"
              "constraint forall(i in 1..2 where i > x)(true);");
    LZN_EXPECTED(LZN_ONELINE(2, 35, 39));
  }

  SECTION("bad if") {
    LZN_MODEL("var int: x;\n"
              "constraint if 1 = x then true else false endif;");
    LZN_EXPECTED(LZN_ONELINE(2, 15, 19));
  }

  LZN_TEST_CASE_END
}
