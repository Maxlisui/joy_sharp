#pragma once

#include <windows.h>

namespace Helper {

class WindowsHelper {

public:
  static bool tryGetDPIScale(float &dpi);

private:
  static bool ensureInitialized();
};
}
