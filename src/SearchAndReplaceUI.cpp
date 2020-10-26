#include "SearchAndReplaceUI.h"
#include "../vendor/imgui/imgui.h"

namespace UI {
void SearchAndReplaceUI::show(SearchLocation location) {
  m_location = location;
  m_locationInt = (int)location;
  m_searchText = "";
  m_replaceText = "";
}

void SearchAndReplaceUI::render() {
  ImGui::Begin("Search and Replace");

  ImGui::InputText("Search", m_searchText.data(), m_searchText.size());
  ImGui::InputText("Replace", m_replaceText.data(), m_replaceText.size());

  ImGui::Combo("Search Location", &m_locationInt, m_searchLocationText, IM_ARRAYSIZE(m_searchLocationText));

  ImGui::End();
}
}
