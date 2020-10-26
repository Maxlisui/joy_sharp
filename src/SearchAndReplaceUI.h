#pragma once

#include "Constants.h"

namespace UI {
class SearchAndReplaceUI {
 public:
  enum class SearchLocation {
    CurrentDocument = 0,
    AllDocuments = 1,
    Project = 2,
    Solution = 3,
  };

  SearchAndReplaceUI()
      : m_searchLocationText{"Current File", "All Open Files",
                             "Current Project", "Entire Solution"},
        m_location{SearchLocation::CurrentDocument} {}

  void show(SearchLocation location);
  void render();

 private:
  string m_searchText;
  string m_replaceText;
  char* m_searchLocationText[4];
  SearchLocation m_location;
  int m_locationInt = 0;
};
}  // namespace UI