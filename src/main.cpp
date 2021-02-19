#include <iostream>
#include <linter/registry.hpp>
#include <minizinc/file_utils.hh>
#include <minizinc/parser.hh>

int main(int argc, const char *argv[]) {
  std::vector<std::string> filenames;
  if (argc > 1) {
    filenames.push_back(argv[1]);
  } else {
    return 1;
  }

  std::vector<std::string> includePaths = {
      MiniZinc::FileUtils::file_path(MiniZinc::FileUtils::share_directory()) + "/std/"};
  std::stringstream errstream;
  MiniZinc::Env env;
  MiniZinc::Model *m = MiniZinc::parse(env, filenames, {}, "", "", includePaths, false, true, false,
                                       false, errstream);

  errstream >> std::cout.rdbuf();

  std::vector<LZN::LintResult> results;
  for (auto rule : LZN::Registry::iter()) {
    rule->run(m, results);
  }

  for (auto &r : results) {
    std::cout << r.message << std::endl;
  }

  return 0;
}
