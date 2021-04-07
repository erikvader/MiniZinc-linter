#include "test_common.hpp"

template <typename T>
std::optional<const MiniZinc::VarDecl *> find_first_array(const T &vec) {
  for (auto vd : vec) {
    if (vd->ti()->isarray())
      return vd;
  }
  return std::nullopt;
}

#define RUN_EVERY_INDEX(expect)                                                                    \
  auto vardecls = lenv.variable_declarations();                                                    \
  auto arr = find_first_array(vardecls);                                                           \
  REQUIRE(arr);                                                                                    \
  CHECK(lenv.is_every_index_touched(arr.value()) == expect);

TEST_CASE("every_index_touched", "[lintenv]") {
  LZN_MODEL_INIT;

  SECTION("one dimension") {
    LZN_ONLY_PARSE("array[1..3] of var int: arr;\n"
                   "constraint forall(i in 1..3)(arr[i] = 1)");
    RUN_EVERY_INDEX(true);
  }

  SECTION("one dimension as par") {
    LZN_ONLY_PARSE("int: n = 5;\n"
                   "array[1..n] of var int: arr;\n"
                   "constraint forall(i in 1..n)(arr[i] = 1)");
    RUN_EVERY_INDEX(true);
  }

  SECTION("one dimension as set of int") {
    LZN_ONLY_PARSE("set of int: ns = 4..5;\n"
                   "array[ns] of var int: arr;\n"
                   "constraint forall(i in ns)(arr[i] = 1)");
    RUN_EVERY_INDEX(true);
  }

  SECTION("one dimension as set of int, value equal") {
    LZN_ONLY_PARSE("set of int: ns = 4..5;\n"
                   "array[ns] of var int: arr;\n"
                   "constraint forall(i in 4..5)(arr[i] = 1)");
    RUN_EVERY_INDEX(false);
  }

  SECTION("two dimensions") {
    LZN_ONLY_PARSE("set of int: ns = 4..5;\n"
                   "array[ns, 1..3] of var int: arr;\n"
                   "constraint forall(i in ns, j in 1..3)(arr[i,j] = 1)");
    RUN_EVERY_INDEX(true);
  }

  SECTION("where clause") {
    LZN_ONLY_PARSE("array[1..3] of var int: arr;\n"
                   "constraint forall(i in 1..3 where true)(arr[i] = 1)");
    RUN_EVERY_INDEX(false);
  }

  SECTION("no constraint") {
    LZN_ONLY_PARSE("array[1..3] of var int: arr;\n");
    RUN_EVERY_INDEX(false);
  }

  SECTION("one assignment") {
    LZN_ONLY_PARSE("array[1..1] of var int: arr;\n"
                   "constraint arr[1] = 1;");
    RUN_EVERY_INDEX(false);
  }

  SECTION("wrong range") {
    LZN_ONLY_PARSE("array[1..10] of var int: arr;\n"
                   "constraint forall(i in 1..3)(arr[i] = 1);");
    RUN_EVERY_INDEX(false);
  }

  SECTION("split in two") {
    LZN_ONLY_PARSE("array[1..10] of var int: arr;\n"
                   "constraint forall(i in 1..3)(arr[i] = 1);\n"
                   "constraint forall(i in 4..10)(arr[i] = 2);");
    RUN_EVERY_INDEX(false);
  }

  LZN_TEST_CASE_END;
}
