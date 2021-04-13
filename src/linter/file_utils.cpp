#include "file_utils.hpp"
#include <algorithm>
#include <errno.h>
#include <fstream>
#include <system_error>

namespace LZN {
std::vector<std::string> lines_of_file(const std::string &filename, unsigned int start,
                                       unsigned int end) {
  std::ifstream f(filename);
  if (!f.is_open()) {
    throw std::system_error(errno, std::generic_category(), filename);
  }
  std::vector<std::string> lines;
  std::string line;
  unsigned int cur_line = 1;
  while (std::getline(f, line)) {
    if (cur_line >= start && cur_line <= end) {
      lines.push_back(std::move(line));
    }
    cur_line++;
    if (cur_line > end)
      break;
  }
  if (f.bad()) {
    throw std::system_error(errno, std::generic_category());
  }
  return lines;
}

bool path_included_from(const std::vector<std::string> &includePath, MiniZinc::ASTString path) {
  if (path.size() == 0)
    return false;

  return !std::any_of(includePath.begin(), includePath.end(),
                      [path](const std::string &incpath) { return path.beginsWith(incpath); });
}
} // namespace LZN
