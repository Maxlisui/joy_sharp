#pragma once

#include "VSProject.h"
#include "SearchAndReplaceUI.h"
#include "../vendor/imgui/imgui.h"

namespace UI {
  
  struct MainUI {
    MainUI () :
    showSearchAndReplace(false),
    searchAndReplace(nullptr) {}
    
    void render();
    void setup(Project::VSSolution *sln);
    void shutdown();
    void renderMain();
    void keyPress(const ImGuiIO& io);
    
    bool showSearchAndReplace;
    SearchAndReplaceUI* searchAndReplace;
  };
  
} // namespace UI
