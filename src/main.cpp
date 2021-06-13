#include "argparse.hpp"
#include <iostream>
#include <linter/registry.hpp>
#include <linter/stdoutprinter.hpp>
#include <minizinc/file_utils.hh>
#include <minizinc/parser.hh>
#include <minizinc/typecheck.hh>

int main(int argc, char *argv[]) {
  const LZN::ArgRes res = LZN::parse_args(argc, argv);
  if (auto err = std::get_if<LZN::ArgError>(&res); err != nullptr) {
    std::cerr << err->msg << std::endl;
    std::cerr << "print usage information with '--help'" << std::endl;
    return EXIT_FAILURE;
  }

  if (std::holds_alternative<LZN::PrintHelp>(res)) {
    LZN::print_help_msg();
    return EXIT_SUCCESS;
  }

  const LZN::Arguments args = std::get<LZN::Arguments>(res);
  std::vector<std::string> filenames = {args.model_filename};

  // parse and typecheck
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

  // run linter
  LZN::LintEnv lenv(m, env, includePaths);
  for (auto rule : LZN::Registry::iter()) {
    if (!LZN::is_rule_ignored(args, *rule))
      rule->run(lenv);
  }

  LZN::stdout_print(lenv.results());

  return EXIT_SUCCESS;
}
