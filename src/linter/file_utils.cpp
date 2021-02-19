#include "file_utils.hpp"
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
} // namespace LZN