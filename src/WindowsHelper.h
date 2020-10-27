#pragma once

#include <windows.h>

namespace Helper {

struct WindowsHelper {
  static bool tryGetDPIScale(float &dpi);
  static bool ensureInitialized();
};
}
