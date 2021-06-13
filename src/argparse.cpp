#include "argparse.hpp"
#include <getopt.h>
#include <unistd.h>

namespace {
constexpr const struct option LONG_FLAGS[] = {
    {"ignore", required_argument, nullptr, 'i'},
    {"ignore-category", required_argument, nullptr, 'c'},
    {"help", no_argument, nullptr, 'h'},
    {0, 0, 0, 0},
};

void add_ignored(LZN::Arguments &results, const char *arg) {
  std::optional<LZN::lintId> lid;
  try {
    lid = std::stoi(arg);
  } catch (const std::invalid_argument &) {
  } catch (const std::out_of_range &) {}

  if (lid)
    results.ignored_rules.push_back(lid.value());
  else
    results.ignored_rule_names.push_back(arg);
}

bool add_ignored_category(LZN::Arguments &results, const char *arg) {
  int i = 0;
  for (const auto &name : LZN::CATEGORY_NAMES) {
    if (name == arg) {
      results.ignored_categories.push_back(static_cast<LZN::Category>(i));
      return true;
    }
    ++i;
  }
  return false;
}
} // namespace

namespace LZN {
void print_help_msg() {
  std::cout << //
      "Usage:\n"
      "  lzn [--help] [--ignore idOrName] [--ignore-category name] [--] modelfile\n"
      "\n"
      "Flags:\n"
      "  --help/-h                  Print this help message.\n"
      "  --ignore/-i idOrName       Don't run a rule with given id or name, flag is "
      "repeatable.\n"
      "  --ignore-category/-c name  Don't run a rules of given category name, flag is "
      "repeatable.\n"
      "                             Valid names include: ";

  for (std::size_t i = 0; i < CATEGORY_NAMES.size(); ++i) {
    if (i == CATEGORY_NAMES.size() - 1)
      std::cout << " and ";
    else if (i > 0)
      std::cout << ", ";
    std::cout << CATEGORY_NAMES[i];
  }
  std::cout << "." << std::endl;
}

ArgRes parse_args(int argc, char *argv[]) {
  if (argc <= 1)
    return ArgError{"no arguments given"};

  Arguments results;
  while (true) {
    int opt = getopt_long(argc, argv, "+:i:c:h", LONG_FLAGS, nullptr);
    if (opt == -1)
      break;

    switch (opt) {
    case 'i': add_ignored(results, optarg); break;
    case 'c':
      if (!add_ignored_category(results, optarg)) {
        return ArgError{"invalid category name"};
      };
      break;
    case 'h': return PrintHelp{};
    case ':': {
      std::string msg = "missing argument for flag: ";
      msg += optopt;
      return ArgError{msg};
    }
    default: { // '?'
      std::string msg = "unknown flag: ";
      msg += optopt;
      return ArgError{msg};
    }
    }
  }

  if (optind >= argc) {
    return ArgError{"missing required positional argument, namely the model file"};
  } else if (optind != argc - 1) {
    return ArgError{"only one model file is expected"};
  } else {
    // TODO: normalize filename?
    results.model_filename = argv[optind];
  }

  return results;
}

#define VEC_CONTAINS(vec, val) (std::find(vec.cbegin(), vec.cend(), val) != vec.cend())
bool is_rule_ignored(const Arguments &args, const LintRule &rule) {
  if (VEC_CONTAINS(args.ignored_rules, rule.id)) {
    return true;
  }

  if (VEC_CONTAINS(args.ignored_rule_names, rule.name)) {
    return true;
  }

  if (VEC_CONTAINS(args.ignored_categories, rule.category)) {
    return true;
  }

  return false;
}
} // namespace LZN
