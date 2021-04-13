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
void print_code(const std::string &filename, const LintResult::Region &region,
                CachedFileReader &reader, bool is_subresult = false) {

  bool first_prefix = true;
  auto prefix = [&first_prefix, is_subresult]() {
    if (is_subresult && first_prefix) {
      first_prefix = false;
      return "   ^     ";
    }
    return "   |     ";
  };

  if (std::holds_alternative<std::monostate>(region)) {
    std::cout << prefix() << std::endl;
    return;
  }

  auto output_error = [](auto &err) {
    std::cerr << rang::fgB::red << rang::style::bold << "Couldn't read file because '" << err.what()
              << "'" << rang::style::reset << std::endl;
  };

  std::visit(overload{
                 [](const std::monostate &) { std::abort(); },
                 [&](const LintResult::MultiLine &ml) {
                   CachedFileReader::FileIter iter;
                   try {
                     iter = reader.read(filename, ml.startline, ml.endline);
                   } catch (std::system_error &err) {
                     output_error(err);
                     return;
                   }
                   for (; iter.first != iter.second; ++iter.first) {
                     std::cout << prefix() << *iter.first << std::endl;
                   }
                   std::cout << prefix() << std::endl;
                 },
                 [&](const LintResult::OneLineMarked &olm) {
                   CachedFileReader::FileIter iter;
                   try {
                     iter = reader.read(filename, olm.line, olm.line);
                   } catch (std::system_error &err) {
                     output_error(err);
                     return;
                   }
                   if (iter.first == iter.second)
                     return;
                   const auto &line = *iter.first;
                   std::cout << prefix() << line << std::endl;
                   std::cout << prefix() << (is_subresult ? rang::fgB::cyan : rang::fgB::yellow)
                             << rang::style::bold;
                   print_marker(olm.startcol,
                                std::min(olm.endcol, static_cast<unsigned int>(line.length())));
                   std::cout << rang::style::reset << std::endl;
                 },
             },
             region);
}

void file_position(std::ostream &stream, const LintResult::Region &region) {
  std::visit(overload{
                 [](const std::monostate &) {},
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

void print_subresults(const LintResult &lintrule, CachedFileReader &reader) {
  for (auto &r : lintrule.sub_results) {
    std::cout << rang::style::bold << r.filename << ':';
    file_position(std::cout, r.region);
    std::cout << rang::style::reset << ' ' << r.message << std::endl;
    print_code(r.filename, r.region, reader, true);
  }
}

void print_one_result(const LintResult &r, CachedFileReader &reader) {
  std::cout << rang::style::bold << r.filename << ':';
  file_position(std::cout, r.region);
  std::cout << rang::style::reset << ' ' << r.message << rang::fgB::magenta << rang::style::bold
            << " [" << r.rule->name << '(' << r.rule->id << ")]" << rang::style::reset << std::endl;
  print_code(r.filename, r.region, reader);
  print_subresults(r, reader);
}
} // namespace

namespace LZN {
void stdout_print(const std::vector<LintResult> &results) {
  CachedFileReader reader;
  for (auto &r : results) {
    print_one_result(r, reader);
  }
}
} // namespace LZN
