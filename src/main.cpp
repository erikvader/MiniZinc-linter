#include <iostream>
#include <linter/registry.hpp>
#include <linter/stdoutprinter.hpp>
#include <minizinc/file_utils.hh>
#include <minizinc/parser.hh>
#include <minizinc/typecheck.hh>

int main(int argc, const char *argv[]) {
  std::vector<std::string> filenames;
  if (argc > 1) {
    filenames.push_back(argv[1]);
  } else {
    std::cerr << "no file" << std::endl;
    return EXIT_FAILURE;
  }

  MiniZinc::GCLock lock;
  std::vector<std::string> includePaths = {
      MiniZinc::FileUtils::file_path(MiniZinc::FileUtils::share_directory()) + "/std/"};
  std::stringstream errstream;
  MiniZinc::Env env;
  MiniZinc::Model *m = MiniZinc::parse(env, filenames, {}, "", "", includePaths, false, false,
                                       false, false, errstream);

  char empty_check;
  if (errstream.readsome(&empty_check, 1) == 1) {
    std::cerr << "parse errors:" << std::endl;
    std::cerr << empty_check;
    errstream >> std::cerr.rdbuf();
  }
  if (m == nullptr)
    return EXIT_FAILURE;

  std::vector<MiniZinc::TypeError> typeErrors;
  try {
    MiniZinc::typecheck(env, m, typeErrors, true, false);
  } catch (MiniZinc::TypeError &te) {
    typeErrors.push_back(te);
    ;
  }
  if (!typeErrors.empty()) {
    std::cerr << "type errors:" << std::endl;
    for (auto &te : typeErrors) {
      std::cerr << te.loc() << ":" << std::endl;
      std::cerr << te.what() << ": " << te.msg() << std::endl;
    }
    return EXIT_FAILURE;
  }

  LZN::LintEnv lenv(m);
  for (auto rule : LZN::Registry::iter()) {
    rule->run(lenv);
  }

  LZN::stdout_print(lenv.results());

  return EXIT_SUCCESS;
}
