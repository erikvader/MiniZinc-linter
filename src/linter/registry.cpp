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

std::size_t Registry::size() noexcept {
  return getMap().size();
}

Registry::iterator Registry::Iter::begin() const noexcept {
  return iterator(getMap().begin());
}

Registry::iterator Registry::Iter::end() const noexcept {
  return iterator(getMap().end());
}

Registry::iterator &Registry::iterator::operator++() {
  ++it;
  return *this;
}

Registry::iterator Registry::iterator::operator++(int) {
  iterator retval = *this;
  ++(*this);
  return retval;
}

bool Registry::iterator::operator==(Registry::iterator other) const {
  return it == other.it;
}

bool Registry::iterator::operator!=(iterator other) const {
  return !(*this == other);
}

Registry::iterator::reference Registry::iterator::operator*() const {
  return it->second;
}

Registry::Iter Registry::iter() noexcept {
  return Iter();
}

} // namespace LZN
