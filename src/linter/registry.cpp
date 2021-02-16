#include "registry.hpp"
#include <stdexcept>

namespace LZN {

Registry::RuleMap &Registry::getMap() noexcept {
  static RuleMap map;
  return map;
}

bool Registry::add(const LintRule *lr) {
  assert(lr != nullptr);
  auto success = getMap().insert(std::make_pair(lr->id, lr));
  if (!success.second) {
    std::ostringstream msg;
    msg << "Rule with id " << lr->id << " registered twice";
    throw std::logic_error(msg.str());
  }
  return true;
}

const LintRule *Registry::get(lintId id) {
  return getMap().at(id);
}

Registry::RuleMap::const_iterator Registry::begin() noexcept {
  return getMap().begin();
}

Registry::RuleMap::const_iterator Registry::end() noexcept {
  return getMap().end();
}

std::size_t Registry::size() noexcept {
  return getMap().size();
}

} // namespace LZN
