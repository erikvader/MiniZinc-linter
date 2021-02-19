#pragma once

#include <string>
#include <vector>

namespace LZN {
std::vector<std::string> lines_of_file(const std::string &filename, unsigned int start,
                                       unsigned int end);
}
