#include "rules.hpp"
#include <linter/overload.hpp>

namespace LZN {
std::ostream &operator<<(std::ostream &os, const LintResult &value) {
  os << "(" << value.rule->name << ")";
  std::visit(overload{[&](const LintResult::None &) { os << "None"; },
                      [&](const LintResult::OneLineMarked &olm) {
                        os << "OLM{" << olm.line << "," << olm.startcol << "," << olm.endcol << "}";
                      },
                      [&](const LintResult::MultiLine &ml) {
                        os << "ML{" << ml.startline << "," << ml.endline << "}";
                      }},
             value.region);
  return os;
}
} // namespace LZN
