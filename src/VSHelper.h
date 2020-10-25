#pragma once

#include "VSProject.h"
#include <functional>
#include <string>

namespace Helper {
class VSHelper {
public:
  static Project::VSSolution *
  parseVSsln(const std::string &slnFilePath,
             const std::function<void(float, const char *)> &progessCallback);
};
} // namespace Helper
