#pragma once

#include "VSProject.h"

namespace UI {

class MainUI {
public:
  static void render();
  static void setup(Project::VSSolution *sln);
  static void shutdown();
};

} // namespace UI
