#pragma once

#include <minizinc/aststring.hh>
#include <string>
#include <vector>

namespace LZN {
std::vector<std::string> lines_of_file(const std::string &filename);

bool path_included_from(const std::vector<std::string> &includePath, MiniZinc::ASTString path);

class CachedFileReader {
public:
  using FileContents = std::vector<std::string>;
  using FilePath = std::string;
  using FileIter = std::pair<FileContents::const_iterator, FileContents::const_iterator>;

private:
  std::unordered_map<FilePath, FileContents> cache;
  void read_to_cache(const FilePath &filename);

public:
  FileIter read(const FilePath &filename, unsigned int startline, unsigned int endline);
};
} // namespace LZN
