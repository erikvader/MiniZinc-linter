#pragma once

#include <linter/rules.hpp>
#include <string>
#include <variant>
#include <vector>

namespace LZN {

class ArgError {
public:
  std::string msg;
};

class Arguments {
public:
  std::string model_filename;
  std::vector<lintId> ignored_rules;
  std::vector<std::string> ignored_rule_names;
  std::vector<Category> ignored_categories;
};

class PrintHelp {};

void print_help_msg();

using ArgRes = std::variant<Arguments, PrintHelp, ArgError>;

ArgRes parse_args(int argc, char *argv[]);

bool is_rule_ignored(const Arguments &args, const LintRule &rule);
} // namespace LZN
