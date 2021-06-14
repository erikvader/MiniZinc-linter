#pragma once

#include <minizinc/aststring.hh>
#include <string>
#include <vector>

namespace LZN {
// Reads all lines of a file to a vector where each element is a line.
std::vector<std::string> lines_of_file(const std::string &filename);

// Returns true if `path` originates from a file in any directory from `includePath`.
bool path_included_from(const std::vector<std::string> &includePath, MiniZinc::ASTString path);

// Reads files and caches them to make a future read of the same file faster.
class CachedFileReader {
public:
  using FileContents = std::vector<std::string>;
  using FilePath = std::string;
  using FileIter = std::pair<FileContents::const_iterator, FileContents::const_iterator>;

private:
  // maps filepaths to their contents
  std::unordered_map<FilePath, FileContents> cache;
  // read `filename` and store it in `cache`.
  void read_to_cache(const FilePath &filename);

public:
  // Returns a pair of iterators to all lines in file `filename`, starting from `startline` to
  // `endline` (inclusive).
  FileIter read(const FilePath &filename, unsigned int startline, unsigned int endline);
};
} // namespace LZN
