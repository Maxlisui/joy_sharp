#pragma once

#include "VSProject.h"
#include <functional>
#include <string>

namespace Helper {
struct VSHelper {
  static Project::VSSolution *
  parseVSsln(const std::string &slnFilePath,
             const std::function<void(float, const char *)> &progessCallback);
};
} // namespace Helper
