#include <iostream>
#include <linter/rules.hpp>
#include <minizinc/file_utils.hh>
#include <minizinc/parser.hh>

int main(int argc, const char *argv[]) {
  std::vector<std::string> filenames;
  if (argc > 1) {
    filenames.push_back(argv[1]);
  }

  std::vector<std::string> includePaths = {
      MiniZinc::FileUtils::file_path(MiniZinc::FileUtils::share_directory()) + "/std/"};
  std::stringstream errstream;
  MiniZinc::Env env;
  MiniZinc::Model *m = MiniZinc::parse(
      env, filenames, {}, "", "", includePaths, false, true, false, false, errstream);

  errstream >> std::cout.rdbuf();

  std::vector<LZN::LintResult> results;
  for (unsigned int i = 0; i < LZN::NUM_RULES; i++) {
    LZN::all_rules()[i]->run(m, results);
  }

  for (auto &r : results) {
    std::cout << r.message << std::endl;
  }

  return 0;
}
