#pragma once

#include "Constants.h"
#include <functional>

namespace UI {
  struct SearchAndReplaceUI {
    SearchAndReplaceUI()
      : searchLocationText{"Current Project", "Entire Solution"},
    location(0),
    showCalled(false),
    matchCase(false),
    editorMode(false) {}
    
    void show();
    void render(bool& show);
    void handleKeyBoardInput(bool& show);
    
    string searchText;
    string replaceText;
    char* searchLocationText[4];
    char* searchLocationTextSlim[2];
    int location;
    bool showCalled;
    bool matchCase;
    bool editorMode;
    std::function<void(const string)> onFindNext;
    std::function<void(const string)> onFindPrev;
    std::function<void(const string, const string)> onReplaceAll;
  };
}  // namespace UI