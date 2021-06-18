#include "test_common.hpp"

TEST_CASE("unused variables and functions", "[rule]") {
  LZN_TEST_CASE_INIT(1);

  SECTION("one unused") {
    LZN_MODEL("var int: x;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 10));
  }

  SECTION("one used and outputted") {
    LZN_MODEL("var int: x;\n"
              "output [show(x)];");
    LZN_EXPECTED();
  }

  SECTION("one used") {
    LZN_MODEL("var int: x;"
              "constraint x = 2;");
    LZN_EXPECTED();
  }

  SECTION("one unused func") {
    LZN_MODEL("function int: f() = 2;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 21));
  }

  SECTION("one used func") {
    LZN_MODEL("function int: f() = 2;"
              "solve minimize f();");
    LZN_EXPECTED();
  }

  SECTION("variable in unused function") {
    LZN_MODEL("int: x = 2;\n"
              "function int: f() = x;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 6), LZN_ONELINE(2, 1, 21));
  }

  SECTION("variable in unused function but used itself") {
    LZN_MODEL("int: x = 2;\n"
              "function int: f() = x;\n"
              "solve maximize x;");
    LZN_EXPECTED(LZN_ONELINE(2, 1, 21));
  }

  SECTION("unused function using function") {
    LZN_MODEL("function int: f() = 2;\n"
              "function int: g() = f();\n");
    LZN_EXPECTED(LZN_ONELINE(2, 1, 23), LZN_ONELINE(1, 1, 21));
  }

  SECTION("unused functions using each other") {
    LZN_MODEL("function int: f() = g();\n"
              "function int: g() = f();\n");
    LZN_EXPECTED(LZN_ONELINE(2, 1, 23), LZN_ONELINE(1, 1, 23));
  }

  // TODO: the typechecker (probably) is folding away g, ie. g() is referring directly to f
  // SECTION("used function using function") {
  //   LZN_MODEL("function int: f() = 2;\n"
  //             "function int: g() = f();\n"
  //             "output [show(g())];");
  //   LZN_EXPECTED();
  // }

  SECTION("used function using function") {
    LZN_MODEL("function int: f() = 2;\n"
              "function int: g() = f()+1;\n"
              "solve maximize g();");
    LZN_EXPECTED();
  }

  SECTION("unused par using function using function") {
    LZN_MODEL("function int: f() = 2;\n"
              "function int: g() = f();\n"
              "int: x = g();");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 21), LZN_ONELINE(2, 1, 23), LZN_ONELINE(3, 1, 6));
  }

  SECTION("par par using function using function") {
    LZN_MODEL("function int: f() = 2;\n"
              "function int: g() = f()+1;\n"
              "int: x = g();\n"
              "solve maximize x;");
    LZN_EXPECTED();
  }

  SECTION("two used functions using each other") {
    LZN_MODEL("function int: f() = g()+1;\n"
              "function int: g() = f()+1;\n"
              "solve maximize g();");
    LZN_EXPECTED();
  }

  SECTION("two unused functions using each other") {
    LZN_MODEL("function int: f() = g()+1;\n"
              "function int: g() = f()+1;\n");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 25), LZN_ONELINE(2, 1, 25));
  }

  SECTION("function using itself") {
    LZN_MODEL("function int: f() = f()+1;\n");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 25));
  }

  SECTION("using function using itself") {
    LZN_MODEL("function int: f() = f()+1;\n"
              "solve maximize f();");
    LZN_EXPECTED();
  }

  SECTION("unused variable in let") {
    LZN_MODEL("int: x = let {int: y = 2} in 1;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 6), LZN_ONELINE(1, 15, 24));
  }

  SECTION("used variable in unused let") {
    LZN_MODEL("int: x = let {int: y = 2} in y;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 6));
  }

  SECTION("used variable in used let") {
    LZN_MODEL("int: x = let {int: y = 2} in y;\n"
              "solve maximize x;");
    LZN_EXPECTED();
  }

  SECTION("let in unused function") {
    LZN_MODEL("function int: f() = let {int: x = 2} in x;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 41));
  }

  SECTION("unused function with unused var argument") {
    LZN_MODEL("function var int: f(var int: y) = 2;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 35), LZN_ONELINE(1, 21, 30));
  }

  SECTION("unused function with used var argument") {
    LZN_MODEL("function var int: f(var int: y) = y;");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 35));
  }

  SECTION("used function with unused var argument") {
    LZN_MODEL("predicate f(var int: y, var int: z) = y = 1;\n"
              "var int: asdasd;\n"
              "constraint f(asdasd, asdasd)");
    LZN_EXPECTED(LZN_ONELINE(1, 25, 34));
  }

  SECTION("par used as domain") {
    LZN_MODEL("int: K;\n"
              "var 0..K: x;\n"
              "constraint x = 0;");
    LZN_EXPECTED();
  }

  SECTION("par used as domain on unused var") {
    LZN_MODEL("int: K;\n"
              "var 0..K: x;\n");
    LZN_EXPECTED(LZN_ONELINE(1, 1, 6), LZN_ONELINE(2, 1, 11));
  }

  SECTION("par used as range on array") {
    LZN_MODEL("int: K = 5;\n"
              "array[1..K] of var 0..1: x;\n"
              "constraint x[1] = 0;");
    LZN_EXPECTED();
  }

  SECTION("ignore duplicated function") {
    // There will be two slightly different definitions of f. `i` will be marked as unused in the
    // copy, but it should be ignored.
    LZN_MODEL("function var int: f(var int: x) = sum(i in 1..2)(i);\n"
              "constraint f(1) = 0;");
    LZN_EXPECTED(LZN_ONELINE(1, 21, 30));
  }

  LZN_TEST_CASE_END
}
