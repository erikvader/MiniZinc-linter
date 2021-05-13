#include "file_utils.hpp"
#include <algorithm>
#include <errno.h>
#include <fstream>
#include <system_error>

namespace LZN {
std::vector<std::string> lines_of_file(const std::string &filename) {
  std::ifstream f(filename);
  if (!f.is_open()) {
    throw std::system_error(errno, std::generic_category(), filename);
  }
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(f, line)) {
    if (!line.empty() && line[line.length() - 1] == '\r')
      line.pop_back();
    lines.push_back(std::move(line));
  }
  if (f.bad()) {
    throw std::system_error(errno, std::generic_category());
  }
  return lines;
}

bool path_included_from(const std::vector<std::string> &includePath, MiniZinc::ASTString path) {
  if (path.size() == 0)
    return false;

  return std::any_of(includePath.begin(), includePath.end(),
                     [path](const std::string &incpath) { return path.beginsWith(incpath); });
}

void CachedFileReader::read_to_cache(const CachedFileReader::FilePath &filename) {
  if (cache.find(filename) == cache.end()) {
    cache.emplace(filename, lines_of_file(filename));
  }
}

CachedFileReader::FileIter CachedFileReader::read(const CachedFileReader::FilePath &filename,
                                                  unsigned int startline, unsigned int endline) {
  assert(endline >= startline && endline > 0 && startline > 0);
  read_to_cache(filename);
  const auto &contents = cache.at(filename);
  auto beg =
      startline > contents.size() ? contents.end() : std::next(contents.begin(), startline - 1);
  auto end = endline > contents.size() ? contents.end() : std::next(contents.begin(), endline);
  return std::make_pair(beg, end);
}
} // namespace LZN
