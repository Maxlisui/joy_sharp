#include "SearchAndReplaceUI.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/misc/cpp/imgui_stdlib.cpp"

namespace UI {
  void SearchAndReplaceUI::show() {
    searchText = "";
    replaceText = "";
    showCalled = true;
  }
  
  void SearchAndReplaceUI::render() {
    ImGui::SetNextWindowSize(ImVec2(0.f, 0.f), ImGuiCond_Appearing);
    
    ImGui::Begin("Search and Replace");
    
    if (showCalled) {
      ImGui::SetKeyboardFocusHere(0);
      showCalled = false;
    }
    
    ImGui::InputText("Search", &searchText);
    ImGui::InputText("Replace", &replaceText);
    
    if (!editorMode) {
      ImGui::Combo("Search Location", &location, searchLocationText, IM_ARRAYSIZE(searchLocationText));
    }
    ImGui::Checkbox("Match Case", &matchCase);
    
    if (editorMode) {
      if (ImGui::Button("Find Previous")) {
      }
      
      ImGui::SameLine();
      
      if (ImGui::Button("Find Next")) {
        if (onFindNext) {
          onFindNext(searchText);
        }
      }
    } else {
      if (ImGui::Button("Find All")) {
      }
    }
    
    if (ImGui::Button("Replace All")) {
    }
    
    ImGui::End();
  }
}
