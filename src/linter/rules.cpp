#include "rules.hpp"
#include <linter/rules/no-domain-var-decl.hpp>

namespace {
using namespace LZN;

#define RULE(t, n)                                                                                 \
  constexpr t n;                                                                                   \
  static_assert(std::is_trivially_destructible_v<t>, "A lint rule is not trivially destructible");

RULE(NoDomainVarDecl, NODOMAINVARDECL)
constexpr const LintRule *const ALL_RULES[NUM_RULES] = {&NODOMAINVARDECL};

// https://peter.bloomfield.online/how-to-check-that-a-function-is-constexpr/
static_assert(ALL_RULES[0]->id >= 0 || true, "ALL_RULES not evaluated at compile time");

} // namespace

namespace LZN {

const LintRule *const *all_rules() {
  return ALL_RULES;
}

} // namespace LZN
