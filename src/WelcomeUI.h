#pragma once

#include "VSProject.h"

namespace UI {

class WelcomeUI {
public:
  static Project::VSSolution *tryGetSln();
  static void render();
};
} // namespace UI
