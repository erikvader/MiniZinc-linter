#include "stdoutprinter.hpp"
#include <linter/file_utils.hpp>
#include <linter/overload.hpp>
#include <rang.hpp>
#include <variant>

namespace {
using namespace LZN;
using Lines = std::vector<std::string>;
using LinesIter = Lines::const_iterator;

constexpr const char *BAR_PREFIX = "   |     ";
constexpr const char *ARROW_PREFIX = "   ^     ";
constexpr const std::size_t MAX_LINE = 200;

void print_marker(unsigned int startcol, unsigned int endcol) {
  assert(endcol >= startcol && startcol > 0 && endcol > 0);
  for (unsigned int i = 0; i < startcol - 1 && i < MAX_LINE; i++) {
    std::cout << ' ';
  }
  std::cout << '^';
  for (unsigned int i = startcol + 1; i <= endcol && i <= MAX_LINE; i++) {
    std::cout << '~';
  }
}

Lines split_lines(const std::string &s) {
  Lines lines;
  const char delim = '\n';
  std::size_t start = 0;
  while (start < s.length()) {
    const std::size_t found = s.find(delim, start);
    if (found == std::string::npos) {
      break;
    }
    lines.push_back(s.substr(start, found - start));
    start = found + 1;
  }
  if (start < s.length()) {
    lines.push_back(s.substr(start));
  }
  return lines;
}

std::size_t indentation(const std::string &s) {
  std::size_t i = 0;
  for (const auto c : s) {
    if (std::isspace(c))
      ++i;
    else
      break;
  }
  return i;
}

std::size_t largest_common_indentation(LinesIter beg, LinesIter end) {
  assert(beg != end);
  std::size_t lci = std::numeric_limits<std::size_t>::max();
  for (; beg != end; ++beg) {
    lci = std::min(lci, indentation(*beg));
  }
  return lci;
}

std::size_t print_line(const std::string &s, std::size_t start = 0,
                       std::size_t maximum = MAX_LINE) {
  constexpr const char *elips = "...";
  assert(maximum >= std::strlen(elips));
  if (start >= s.length())
    return 0;
  const bool too_long = s.length() - start > maximum;
  const std::size_t end = too_long ? maximum - std::strlen(elips) + start : s.length();
  // TODO: handle the case of multi-byte characters

  for (std::size_t i = start; i < end; ++i) {
    if (std::isspace(s[i]))
      std::cout << ' ';
    else
      std::cout << s[i];
  }

  if (too_long)
    std::cout << "...";

  return end - start + (too_long ? std::strlen(elips) : 0);
}

template <typename P>
void print_lines_prefixed(LinesIter begin, LinesIter end, P &prefix) {
  if (begin == end)
    return;

  const std::size_t lci = largest_common_indentation(begin, end);
  for (; begin != end; ++begin) {
    std::cout << prefix();
    print_line(*begin, lci);
    std::cout << std::endl;
  }
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

void print_code(const FileContents &contents, CachedFileReader &reader, bool is_subresult = false) {
  if (contents.is_empty())
    return;

  if (!contents.is_valid()) {
    std::cout << rang::fgB::red << rang::style::bold
              << "Couldn't print because file location is invalid" << rang::style::reset
              << std::endl;
    return;
  }

  auto prefix = prefixer(is_subresult);

  auto output_error = [](auto &err) {
    std::cout << rang::fgB::red << rang::style::bold << "Couldn't read file because '" << err.what()
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
                   const std::size_t ind = indentation(line);
                   std::cout << prefix();
                   std::size_t printed_len = print_line(line, ind);
                   std::cout << std::endl;
                   std::cout << prefix() << (is_subresult ? rang::fgB::cyan : rang::fgB::yellow)
                             << rang::style::bold;
                   const unsigned int start_col = olm.startcol - ind;
                   const unsigned int end_col = olm.endcol ? olm.endcol.value() - ind
                                                           : static_cast<unsigned int>(printed_len);
                   print_marker(start_col, end_col);
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

std::vector<const LintResult::Sub *> notes_first(const std::vector<LintResult::Sub> &subres) {
  std::vector<const LintResult::Sub *> res;
  res.reserve(subres.size());
  for (const auto &sub : subres) {
    res.push_back(&sub);
  }
  std::stable_partition(res.begin(), res.end(),
                        [](const LintResult::Sub *sub) { return sub->content.is_empty(); });
  return res;
}

void print_subresults(const LintResult &lintrule, CachedFileReader &reader) {
  for (const auto *r : notes_first(lintrule.sub_results)) {
    if (r->content.is_empty()) {
      std::cout << rang::fgB::green << "NOTE: " << rang::style::reset << r->message << std::endl;
    } else {
      file_position(r->content);
      std::cout << rang::style::reset;
      std::cout << ' ' << r->message << std::endl;
      print_code(r->content, reader, true);
    }
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
    auto prefix = prefixer();
    auto lines = split_lines(r.rewrite.value());
    print_lines_prefixed(lines.cbegin(), lines.cend(), prefix);
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
