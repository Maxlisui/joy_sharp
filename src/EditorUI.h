#pragma once

#include "Constants.h"
#include "../vendor/imgui/imgui.h"
#include "SearchAndReplaceUI.h"
#include <chrono>
#include <vector>

namespace UI {
  struct EditorUI {
    EditorUI()
      : textStartPixel(30.f),
    tabSize(4),
    lineSpacing(1.f),
    startTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
              .count()),
    override(false),
    lastClick(-1.f),
    cursoPositionChanged(false),
    readOnly(false),
    textChanged(false),
    selectionMode(SelectionMode::Normal),
    searchAndReplace(nullptr),
    showSearchAndReplace(false),
    currentSearchItem(0),
    lastSearchString("") {}
    
    struct Glypth {
      Glypth(uint8_t character) : m_char(character) {}
      uint8_t m_char;
    };
    
    struct Coordinate {
      Coordinate() : column(0), line(0) {}
      Coordinate(int line, int column) : line(line), column(column) {}
      
      bool operator==(const Coordinate &o) const {
        return line == o.line && column == o.column;
      }
      
      bool operator!=(const Coordinate &o) const {
        return line != o.line || column != o.column;
      }
      
      bool operator<(const Coordinate &o) const {
        if (line != o.line)
          return line < o.line;
        return column < o.column;
      }
      
      bool operator>(const Coordinate &o) const {
        if (line != o.line)
          return line > o.line;
        return column > o.column;
      }
      
      bool operator<=(const Coordinate &o) const {
        if (line != o.line)
          return line < o.line;
        return column <= o.column;
      }
      
      bool operator>=(const Coordinate &o) const {
        if (line != o.line)
          return line > o.line;
        return column >= o.column;
      }
      
      int line;
      int column;
    };
    
    struct EditorState {
      Coordinate selectionStart;
      Coordinate selectionEnd;
      Coordinate cursorPosition;
    };
    
    struct SelectionRange {
      Coordinate start;
      Coordinate end;
    };
    
    enum class SelectionMode { Normal, Word, Line };
    
    typedef std::vector<Glypth> Line;
    typedef std::vector<Line> Lines;
    
    void setText(const string &text);
    void render();
    void setSearchAndReplace(SearchAndReplaceUI *search);
    float textDistanceToLineStart(const EditorUI::Coordinate &from) const;
    int getCharacterIndex(const EditorUI::Coordinate &from) const;
    int getLineMaxColumn(int line) const;
    int getPageSize() const;
    bool hasSelection() const;
    void handleMouseInput();
    Coordinate screenPosToCoordinates(const ImVec2 &position) const;
    Coordinate sanitizeCoordinates(const Coordinate &value) const;
    void setSelection(const Coordinate &start, const Coordinate &end,
                      SelectionMode mode);
    void setCursorPosition(const Coordinate& pos);
    void handleKeyboardInput();
    void deleteSelection();
    void deleteRange(const Coordinate& from, const Coordinate& to);
    void deleteLine(int start, int end);
    void deleteLine(int index);
    void moveUp(int amount, bool shift);
    void moveDown(int amount, bool shift);
    void moveLeft(int amount, bool shift, bool ctrl);
    void moveRight(int amount, bool shift, bool ctrl);
    void moveEnd(bool shift);
    void moveHome(bool shift);
    void moveTop(bool shift);
    void moveBottom(bool shift);
    void copy();
    void paste();
    void cut();
    void selectAll();
    void backspace();
    void remove();
    void ensureCursorVisible();
    void handleEscape();
    void createUIRange(const Coordinate& from, const Coordinate& to, Coordinate& lineStart, Coordinate& lineEnd, float& start, float& end, int lineNo);
    Coordinate getActualCursorCoordinates() const;
    std::string getSelectedText() const;
    std::string getText(const Coordinate& from, const Coordinate& to) const;
    int getCharacterColumn(int lineIndex, int index) const;
    Coordinate findWordStart(const Coordinate &from) const;
    Coordinate findWordEnd(const Coordinate &From) const;
    Coordinate findNextWord(const Coordinate &aFrom) const;
    bool isOnWordBoundary(const Coordinate &at) const;
    void insertText(const std::string& value);
    void insertText(const char *value);
    int insertTextAt(Coordinate &pos, const char *value);
    Line &insertLine(int index);
    void insertCharacter(ImWchar c, bool shift);
    void setFindResult(const string& str);
    void findNext(const string& next);
    void findPrev(const string& prev);
    void replaceAll(const string& searchText, const string& replaceText);
    
    Lines lines;
    float lineSpacing;
    float textStartPixel;
    int tabSize;
    string lineBuffer;
    EditorState editorState;
    uint64_t startTime;
    Coordinate interactiveStart;
    Coordinate interactiveEnd;
    bool override;
    float lastClick;
    ImVec2 charAdvance;
    SelectionMode selectionMode;
    bool cursoPositionChanged;
    bool readOnly;
    bool textChanged;
    SearchAndReplaceUI *searchAndReplace;
    bool showSearchAndReplace;
    std::vector<SelectionRange> searchResults;
    int currentSearchItem;
    string lastSearchString;
  };
  
} // namespace UI
