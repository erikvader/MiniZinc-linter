#include "test_common.hpp"

TEST_CASE("non-functionally defined not in search hint", "[rule]") {
  LZN_TEST_CASE_INIT(9);

  SECTION("single variable") {
    LZN_MODEL("var int: a;\n");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 10));
  }

  SECTION("single variable in search hint") {
    LZN_MODEL("var int: a;\n"
              "solve :: int_search([a], input_order, indomain) satisfy;");
    LZN_EXPECTED();
  }

  SECTION("two assigned variables") {
    LZN_MODEL("var int: a;\n"
              "var int: b = a;\n");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 10));
  }

  SECTION("two assigned variables in search hint") {
    LZN_MODEL("var int: a;\n"
              "var int: b = a;\n"
              "solve :: int_search([a], input_order, indomain) satisfy;");
    LZN_EXPECTED();
  }

  SECTION("two variables constrained to each other") {
    LZN_MODEL("var int: a;\n"
              "var int: b;\n"
              "constraint a = b;");
    LZN_EXPECTED();
  }

  SECTION("two variables constrained") {
    LZN_MODEL("var int: a;\n"
              "var int: b;\n"
              "constraint a = b+1;");
    LZN_EXPECTED(LZN_ONELINE(2, 1, 10));
  }

  SECTION("three variables constrained") {
    LZN_MODEL("var int: a;\n"
              "var int: b;\n"
              "var int: c;\n"
              "constraint c = b+5 /\\ a = b+1;");
    LZN_EXPECTED(LZN_ONELINE(2, 1, 10));
  }

  SECTION("array single element constrained") {
    LZN_MODEL("array[1..5] of var int: as;\n"
              "constraint as[1] = 2;");
    LZN_EXPECTED();
  }

  SECTION("predicate") {
    LZN_MODEL("var int: a;\n"
              "predicate p(var int: x) = x = 42;"
              "constraint p(a);");
    LZN_EXPECTED();
  }

  SECTION("global_cardinality") {
    LZN_MODEL("array[1..5] of var int: xs;\n"
              "array[1..2] of int: to_count = [1,2];\n"
              "array[1..2] of var int: counts;\n"

              // shamelessly copied
              "predicate my_global_cardinality(array [$X] of var int: x,"
              "                                array [$Y] of int: cover,"
              "                                array [$Y] of var int: counts) ="
              "    assert(index_sets_agree(cover, counts),"
              "    \"global_cardinality: cover and counts must have identical index sets\","
              "    my_fzn_global_cardinality(array1d(x), array1d(cover), array1d(counts)));"

              "predicate my_fzn_global_cardinality(array[int] of var int: x, "
              "                                    array[int] of int: cover,"
              "                                    array[int] of var int: counts) ="
              "    forall(i in index_set(cover))(my_count(x, cover[i], counts[i]))"
              "    /\\ length(x) >= sum(counts);"

              "predicate my_count(array[$X] of var int: x,"
              "                   var int: y,"
              "                   var int: c) ="
              "    my_count_eq(array1d(x), y, c);"

              "predicate my_count_eq(array[$X] of var int: x,"
              "                      var int: y,"
              "                      var int: c) ="
              "    my_fzn_count_eq(array1d(x), y, c);"

              "predicate my_fzn_count_eq(array[int] of var int: x,"
              "                          var int: y,"
              "                          var int: c) ="
              "    c == sum(i in index_set(x))(bool2int(x[i] == y));"
              "\n"
              "constraint my_global_cardinality(xs, to_count, counts);");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 26));
  }

  LZN_TEST_CASE_END
}
