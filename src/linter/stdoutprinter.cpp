#include "stdoutprinter.hpp"
#include <linter/file_utils.hpp>
#include <linter/overload.hpp>
#include <rang.hpp>
#include <variant>

namespace {
using namespace LZN;

void print_marker(unsigned int startcol, unsigned int endcol) {
  for (unsigned int i = 0; i < startcol - 1; i++) {
    std::cout << ' ';
  }
  std::cout << '^';
  for (unsigned int i = startcol + 1; i <= endcol; i++) {
    std::cout << '~';
  }
}

// TODO: remove indentation from the lines printed directly from the file
void print_code(const std::string &filename, const LintResult::Region &region) {
  constexpr const char *const prefix = "   |     ";

  if (std::holds_alternative<LintResult::None>(region)) {
    std::cout << prefix << std::endl;
    return;
  }

  // TODO: cache somehow so the same file isn't read multiple times
  auto readfile = [&filename](unsigned int startline,
                              unsigned int endline) -> std::vector<std::string> {
    std::vector<std::string> lines;
    try {
      lines = lines_of_file(filename, startline, endline);
    } catch (std::system_error &err) { std::cerr << err.what() << std::endl; }
    return lines;
  };

  std::visit(overload{
                 [](const LintResult::None &) { std::abort(); },
                 [&](const LintResult::MultiLine &ml) {
                   for (const auto &l : readfile(ml.startline, ml.endline)) {
                     std::cout << prefix << l << std::endl;
                   }
                   std::cout << prefix << std::endl;
                 },
                 [&](const LintResult::OneLineMarked &olm) {
                   std::vector<std::string> line = readfile(olm.line, olm.line);
                   if (line.empty())
                     return;
                   std::cout << prefix << line[0] << std::endl;
                   std::cout << prefix << rang::fgB::blue << rang::style::bold;
                   print_marker(olm.startcol, olm.endcol);
                   std::cout << rang::style::reset << std::endl;
                 },
             },
             region);
}

void file_position(std::ostream &stream, const LintResult::Region &region) {
  std::visit(overload{
                 [](const LintResult::None &) {},
                 [&](const LintResult::MultiLine &ml) {
                   stream << ml.startline << '-' << ml.endline << ':';
                 },
                 [&](const LintResult::OneLineMarked &olm) {
                   stream << olm.line << '.' << olm.startcol << '-' << olm.line << '.' << olm.endcol
                          << ':';
                 },
             },
             region);
}
} // namespace

namespace LZN {
void stdout_print(const std::vector<LintResult> &results) {
  for (auto &r : results) {
    std::cout << rang::style::bold << r.filename << ':';
    file_position(std::cout, r.region);
    std::cout << rang::style::reset << ' ' << r.message << rang::fgB::magenta << rang::style::bold
              << " [" << r.rule->name << '(' << r.rule->id << ")]" << rang::style::reset
              << std::endl;
    print_code(r.filename, r.region);
  }
}
} // namespace LZN
