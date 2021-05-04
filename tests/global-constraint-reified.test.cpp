#include "test_common.hpp"

TEST_CASE("global constraint reified", "[rule]") {
  LZN_TEST_CASE_INIT(17);

  SECTION("okay alldifferent") {
    LZN_MODEL("include \"globals.mzn\";\n"
              "array[1..5] of var int: xs;\n"
              "constraint alldifferent(xs);");
    LZN_EXPECTED();
  }

  SECTION("okay conjunctive alldifferent") {
    LZN_MODEL("include \"globals.mzn\";\n"
              "array[1..5] of var int: xs;\n"
              "constraint alldifferent(xs) /\\ alldifferent(xs);");
    LZN_EXPECTED();
  }

  SECTION("forall is fine") {
    LZN_MODEL("include \"globals.mzn\";\n"
              "array[1..5] of var bool: xs;\n"
              "constraint forall(xs);");
    LZN_EXPECTED();
  }

  SECTION("bad alldifferent") {
    LZN_MODEL("include \"globals.mzn\";\n"
              "array[1..5] of var int: xs;\n"
              "constraint alldifferent(xs) \\/ alldifferent(xs);");
    LZN_EXPECTED(LZN_ONELINE(3, 12, 27), LZN_ONELINE(3, 32, 47));
  }

  SECTION("assigned count") {
    LZN_MODEL("include \"globals.mzn\";\n"
              "array[1..5] of var int: xs;\n"
              "constraint 1 = count(xs, 1);");
    LZN_EXPECTED();
  }

  SECTION("user defined predicate") {
    LZN_MODEL("include \"globals.mzn\";\n"
              "predicate f(var int: x) = x = 1;\n"
              "array[1..5] of var int: xs;\n"
              "constraint f(xs[2]) \\/ f(xs[1])");
    LZN_EXPECTED();
  }

  LZN_TEST_CASE_END
}
