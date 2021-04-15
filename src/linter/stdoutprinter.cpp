#include "stdoutprinter.hpp"
#include <linter/file_utils.hpp>
#include <linter/overload.hpp>
#include <rang.hpp>
#include <variant>

namespace {
using namespace LZN;

constexpr const char *BAR_PREFIX = "   |     ";
constexpr const char *ARROW_PREFIX = "   ^     ";

void print_marker(unsigned int startcol, unsigned int endcol) {
  for (unsigned int i = 0; i < startcol - 1; i++) {
    std::cout << ' ';
  }
  std::cout << '^';
  for (unsigned int i = startcol + 1; i <= endcol; i++) {
    std::cout << '~';
  }
}

template <typename T, typename P>
void print_lines_prefixed(T begin, T end, P prefix) {
  for (; begin != end; ++begin) {
    std::cout << prefix() << *begin << std::endl;
  }
}

template <typename P>
void print_lines_in_string_prefixed(const std::string &str, P prefix) {
  if (str.empty())
    return;

  std::cout << prefix();
  for (auto begin = str.cbegin(); begin != str.cend(); ++begin) {
    if (*begin == '\n' && begin + 1 != str.cend()) {
      std::cout << *begin << prefix();
    } else {
      std::cout << *begin;
    }
  }
  std::cout << std::endl;
}

auto prefixer(bool use_arrow = false) {
  return [print_arrow = use_arrow]() mutable {
    if (print_arrow) {
      print_arrow = false;
      return ARROW_PREFIX;
    }
    return BAR_PREFIX;
  };
}

// TODO: remove indentation from the lines printed directly from the file
void print_code(const FileContents &contents, CachedFileReader &reader, bool is_subresult = false) {
  if (contents.is_empty())
    return;

  auto prefix = prefixer(is_subresult);

  auto output_error = [](auto &err) {
    std::cerr << rang::fgB::red << rang::style::bold << "Couldn't read file because '" << err.what()
              << "'" << rang::style::reset << std::endl;
  };

  std::visit(overload{
                 [&](const std::monostate &) { std::cout << prefix() << std::endl; },
                 [&](const FileContents::MultiLine &ml) {
                   CachedFileReader::FileIter iter;
                   try {
                     iter = reader.read(contents.filename, ml.startline, ml.endline);
                   } catch (std::system_error &err) {
                     output_error(err);
                     return;
                   }
                   print_lines_prefixed(iter.first, iter.second, prefix);
                   std::cout << prefix() << std::endl;
                 },
                 [&](const FileContents::OneLineMarked &olm) {
                   CachedFileReader::FileIter iter;
                   try {
                     iter = reader.read(contents.filename, olm.line, olm.line);
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
                                olm.endcol.value_or(static_cast<unsigned int>(line.length())));
                   std::cout << rang::style::reset << std::endl;
                 },
             },
             contents.region);
}

void file_position(const FileContents &contents) {
  if (contents.is_empty())
    return;

  std::cout << rang::style::bold << contents.filename << ':';
  std::visit(overload{
                 [](const std::monostate &) {},
                 [&](const FileContents::MultiLine &ml) {
                   std::cout << ml.startline << '-' << ml.endline << ':';
                 },
                 [&](const FileContents::OneLineMarked &olm) {
                   std::cout << olm.line << '.' << olm.startcol;
                   if (olm.endcol)
                     std::cout << '-' << olm.line << '.' << olm.endcol.value();
                   std::cout << ':';
                 },
             },
             contents.region);
}

void print_subresults(const LintResult &lintrule, CachedFileReader &reader) {
  for (auto &r : lintrule.sub_results) {
    file_position(r.content);
    std::cout << rang::style::reset;
    if (!r.content.is_empty())
      std::cout << ' ';
    std::cout << r.message << std::endl;
    print_code(r.content, reader, true);
  }
}

void print_one_result(const LintResult &r, CachedFileReader &reader) {
  file_position(r.content);
  std::cout << rang::style::reset;
  if (!r.content.is_empty())
    std::cout << ' ';
  std::cout << r.message << rang::fgB::magenta << rang::style::bold << " [" << r.rule->name << '('
            << r.rule->id << ")]" << rang::style::reset << std::endl;
  print_code(r.content, reader);
  if (r.rewrite) {
    std::cout << "rewrite as: " << std::endl;
    print_lines_in_string_prefixed(r.rewrite.value(), prefixer());
  }
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
