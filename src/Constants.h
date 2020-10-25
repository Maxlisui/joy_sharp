#pragma once

#include <filesystem>
#include <string>

typedef std::string string;
typedef std::filesystem::path path;
typedef std::filesystem::directory_entry directory_entry;
typedef std::filesystem::directory_iterator directory_iterator;

namespace Helper {
namespace Constants {
static const std::string VS_FOLDER_ID = "2150E333-8FDC-42A3-9474-1A3956D46DE8";
}
} // namespace Helper
