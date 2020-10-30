#include "SearchAndReplaceUI.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/misc/cpp/imgui_stdlib.cpp"

namespace UI {
  
  void SearchAndReplaceUI::handleKeyBoardInput(bool& show) {
    ImGuiIO& io = ImGui::GetIO();
    auto shift = io.KeyShift;
    auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;
    
    if (ImGui::IsWindowFocused()) {
      io.WantCaptureKeyboard = true;
      io.WantTextInput = true;
      
      if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
        show = false;
      } else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter)) && onFindNext) {
        onFindNext(searchText);
      }
    }
  }
  
  void SearchAndReplaceUI::show() {
    searchText = "";
    replaceText = "";
    showCalled = true;
  }
  
  void SearchAndReplaceUI::render(bool& show) {
    ImGui::SetNextWindowSize(ImVec2(0.f, 0.f), ImGuiCond_Appearing);
    
    if (show) {
      ImGui::Begin("Search and Replace", &show);
      
      handleKeyBoardInput(show);
      
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
          if (onFindPrev) {
            onFindPrev(searchText);
          }
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
        if (onReplaceAll) {
          onReplaceAll(searchText, replaceText);
        }
      }
      
      ImGui::End();
    }
  }
}
