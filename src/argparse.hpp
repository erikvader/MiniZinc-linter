#pragma once

#include <linter/rules.hpp>
#include <string>
#include <variant>
#include <vector>

namespace LZN {

// cmdline arguments are invalid
class ArgError {
public:
  std::string msg;
};

// A valid parse of the cmdline arguments
class Arguments {
public:
  std::string model_filename; // required
  std::vector<std::string> datafiles;
  std::vector<lintId> ignored_rules;
  std::vector<std::string> ignored_rule_names;
  std::vector<Category> ignored_categories;
};

// The printing of a long help message was requested
class PrintHelp {};

// Print a help message to stdout
void print_help_msg();

// The valid results from parsing cmdline arguments
using ArgRes = std::variant<Arguments, PrintHelp, ArgError>;

// Parse cmdline arguments as given to main and return either valid results, an error or a request
// to print a help message.
ArgRes parse_args(int argc, char *argv[]);

// Returns true if `rule` should be ignored, as given from `args`.
bool is_rule_ignored(const Arguments &args, const LintRule &rule);
} // namespace LZN
