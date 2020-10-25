#pragma once

#include "Constants.h"
#include "../vendor/imgui/imgui.h"
#include <chrono>
#include <vector>

namespace UI {
  class EditorUI {
    public:
    EditorUI()
      : m_textStartPixel(30.f),
    m_tabSize(4),
    m_lineSpacing(1.f),
    m_startTime(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch())
                .count()),
    m_override(false),
    m_lastClick(-1.f),
    m_cursoPositionChanged(false),
    m_readOnly(false),
    m_textChanged(false) {}
    
    void setText(const string &text);
    void render();
    float getLineSpacing() { return m_lineSpacing; }
    void setLineSpacing(float lineSpacing) { m_lineSpacing = lineSpacing; }
    float getTextStartPixel() { return m_textStartPixel; }
    void setTextStartPixel(float textStartPixel) {
      m_textStartPixel = textStartPixel;
    }
    int getTabSize() { return m_tabSize; }
    void setTabSize(int tabSize) { m_tabSize = tabSize; }
    
    struct Glypth {
      Glypth(uint8_t character) : m_char(character) {}
      uint8_t m_char;
    };
    
    struct Coordinate {
      Coordinate() : m_column(0), m_line(0) {}
      Coordinate(int line, int column) : m_line(line), m_column(column) {}
      
      bool operator==(const Coordinate &o) const {
        return m_line == o.m_line && m_column == o.m_column;
      }
      
      bool operator!=(const Coordinate &o) const {
        return m_line != o.m_line || m_column != o.m_column;
      }
      
      bool operator<(const Coordinate &o) const {
        if (m_line != o.m_line)
          return m_line < o.m_line;
        return m_column < o.m_column;
      }
      
      bool operator>(const Coordinate &o) const {
        if (m_line != o.m_line)
          return m_line > o.m_line;
        return m_column > o.m_column;
      }
      
      bool operator<=(const Coordinate &o) const {
        if (m_line != o.m_line)
          return m_line < o.m_line;
        return m_column <= o.m_column;
      }
      
      bool operator>=(const Coordinate &o) const {
        if (m_line != o.m_line)
          return m_line > o.m_line;
        return m_column >= o.m_column;
      }
      
      int m_line;
      int m_column;
    };
    
    struct EditorState {
      Coordinate m_selectionStart;
      Coordinate m_selectionEnd;
      Coordinate m_cursorPosition;
    };
    
    enum class SelectionMode { Normal, Word, Line };
    
    private:
    typedef std::vector<Glypth> Line;
    typedef std::vector<Line> Lines;
    
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
    
    Lines m_lines;
    float m_lineSpacing;
    float m_textStartPixel;
    int m_tabSize;
    string m_lineBuffer;
    EditorState m_editorState;
    uint64_t m_startTime;
    Coordinate m_interactiveStart;
    Coordinate m_interactiveEnd;
    bool m_override;
    float m_lastClick;
    ImVec2 m_charAdvance;
    SelectionMode m_selectionMode;
    bool m_cursoPositionChanged;
    bool m_readOnly;
    bool m_textChanged;
  };
  
} // namespace UI
