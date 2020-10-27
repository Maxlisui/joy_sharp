#pragma once

#include "VSProject.h"

namespace UI {

struct MainUI {
  static void render();
  static void setup(Project::VSSolution *sln);
  static void shutdown();
};

} // namespace UI
