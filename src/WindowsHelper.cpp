#include "WindowsHelper.h"

namespace Helper {

typedef UINT(*GDFS)();

static GDFS m_getDpiForSystem = nullptr;
static HMODULE m_user32_dll = nullptr;

bool WindowsHelper::tryGetDPIScale(float &dpi) {
  if (!ensureInitialized()) {
    return false;
  }

  dpi = m_getDpiForSystem() / 96.f;

  return true;
}

bool WindowsHelper::ensureInitialized() {
  if (!m_user32_dll) {
    m_user32_dll = GetModuleHandleW(L"user32.dll");
    if (m_user32_dll == INVALID_HANDLE_VALUE) {
      return false;
    }
  }

  if (!m_getDpiForSystem) {
    m_getDpiForSystem = (GDFS) GetProcAddress(m_user32_dll, "GetDpiForSystem");

    if (!m_getDpiForSystem) {
      return false;
    }
  }
  return true;
}

}