#pragma once

#include "VSProject.h"

namespace UI {

struct WelcomeUI {
  static Project::VSSolution *tryGetSln();
  static void render();
};
} // namespace UI
