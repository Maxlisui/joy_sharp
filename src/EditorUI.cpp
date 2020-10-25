#include "EditorUI.h"
#include "Tooling.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_internal.h"
#include "../vendor/stb/stb_sprintf.h"

namespace UI {
  
  static inline int ImTextCharToUtf8(char* buf, int buf_size, unsigned int c) {
    if (c < 0x80)
    {
      buf[0] = (char)c;
      return 1;
    }
    if (c < 0x800)
    {
      if (buf_size < 2) return 0;
      buf[0] = (char)(0xc0 + (c >> 6));
      buf[1] = (char)(0x80 + (c & 0x3f));
      return 2;
    }
    if (c < 0x10000)
    {
      if (buf_size < 3) return 0;
      buf[0] = (char)(0xe0 + (c >> 12));
      buf[1] = (char)(0x80 + ((c >> 6) & 0x3f));
      buf[2] = (char)(0x80 + ((c ) & 0x3f));
      return 3;
    }
    if (c <= 0x10FFFF)
    {
      if (buf_size < 4) return 0;
      buf[0] = (char)(0xf0 + (c >> 18));
      buf[1] = (char)(0x80 + ((c >> 12) & 0x3f));
      buf[2] = (char)(0x80 + ((c >> 6) & 0x3f));
      buf[3] = (char)(0x80 + ((c ) & 0x3f));
      return 4;
    }
    // Invalid code point, the max unicode is 0x10FFFF
    return 0;
  }
  
  static inline bool isANWord(char c) {
    if (isalnum(c)) {
      return true;
    }
    
    if (c == '_') {
      return true;
    }
    return false;
  }
  
  static inline int UTF8CharLength(uint8_t c) {
    if ((c & 0xFE) == 0xFC) {
      return 6;
    }
    if ((c & 0xFC) == 0xF8) {
      return 5;
    }
    
    if ((c & 0xF8) == 0xF0) {
      return 4;
    }
    if ((c & 0xF0) == 0xE0) {
      return 3;
    }
    if ((c & 0xE0) == 0xC0) {
      return 2;
    }
    
    return 1;
  }
  
  static inline bool isUTF8Sequence(char c) {
    return (c & 0xC0) == 0x80;
  }
  
  inline bool EditorUI::hasSelection() const {
    return m_editorState.m_selectionEnd > m_editorState.m_selectionStart;
  }
  
  inline std::string EditorUI::getSelectedText() const {
    return getText(m_editorState.m_selectionStart, m_editorState.m_selectionEnd);
  }
  
  int EditorUI::getLineMaxColumn(int l) const {
    PROFILE_START;
    if (l >= m_lines.size()) return 0;
    auto& line = m_lines[l];
    int col = 0;
    for (unsigned i = 0; i < line.size();) {
      auto c = line[i].m_char;
      if (c == '\t')
        col = (col / m_tabSize) * m_tabSize + m_tabSize;
      else
        col++;
      i += UTF8CharLength(c);
    }
    return col;
  }
  
  inline int EditorUI::getPageSize() const {
    float height = ImGui::GetWindowHeight() - 20.f;
    return (int)floor(height / m_charAdvance.y);
  }
  
  int EditorUI::getCharacterIndex(const EditorUI::Coordinate& from) const {
    PROFILE_START;
    if (from.m_line >= m_lines.size()) return -1;
    auto& line = m_lines[from.m_line];
    int c = 0;
    int i = 0;
    for (; i < line.size() && c < from.m_column;) {
      if (line[i].m_char == '\t') {
        c = (c / m_tabSize) * m_tabSize + m_tabSize;
      } else {
        ++c;
      }
      i += UTF8CharLength(line[i].m_char);
    }
    return i;
  }
  
  
  
  float EditorUI::textDistanceToLineStart(const EditorUI::Coordinate& from) const {
    PROFILE_START;
    auto& line = m_lines[from.m_line];
    
    float distance = 0.0f;
    float spaceSize = ImGui::GetFont()
      ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f,
                      " ", nullptr, nullptr)
      .x;
    
    int colIndex = getCharacterIndex(from);
    
    for (size_t i = 0u; i < line.size() && i < colIndex;) {
      if (line[i].m_char == '\t') {
        distance = (1.f + std::floor((1.f + distance) /
                                     (float(m_tabSize) * spaceSize))) *
          (float(m_tabSize) * spaceSize);
        i++;
      } else {
        // TODO: Cache this
        auto d = UTF8CharLength(line[i].m_char);
        char tempCString[7];
        int j = 0;
        for (; j < 6 && d-- > 0 && i < (int)line.size(); i++, j++) {
          tempCString[j] = line[i].m_char;
        }
        tempCString[j] = '\0';
        distance += ImGui::GetFont()
          ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f,
                          tempCString, nullptr, nullptr)
          .x;
      }
    }
    return distance;
  }
  
  void inline EditorUI::deleteLine(int start, int end) {
    PROFILE_START;
    m_lines.erase(m_lines.begin() + start, m_lines.begin() + end);
    m_textChanged = true;
  }

  void inline EditorUI::deleteLine(int index) {
    m_lines.erase(m_lines.begin() + index);
    m_textChanged = true;
  }
  
  void EditorUI::deleteRange(const Coordinate& from, const Coordinate& to) {
    PROFILE_START;
    if (m_readOnly) {
      return;
    }
    
    if (from == to) {
      return;
    }
    
    auto start = getCharacterIndex(from);
    auto end = getCharacterIndex(to);
    
    if (from.m_line == to.m_line) {
      auto& line = m_lines[from.m_line];
      auto n = getLineMaxColumn(from.m_line);
      if (to.m_column >= n) {
        line.erase(line.begin() + start, line.end());
      } else {
        line.erase(line.begin() + start, line.begin() + end);
      }
    } else {
      auto& firstLine = m_lines[from.m_line];
      auto& lastLine = m_lines[to.m_line];
      
      firstLine.erase(firstLine.begin() + start, firstLine.end());
      lastLine.erase(lastLine.begin(), lastLine.begin() + end);
      
      if (from.m_line < to.m_line) {
        firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());
        deleteLine(from.m_line + 1, to.m_line + 1);
      }
    }
    m_textChanged = true;
  }
  
  std::string EditorUI::getText(const Coordinate& from, const Coordinate& to) const {
    PROFILE_START;
    std::string result;
    
    int lStart = from.m_line;
    int lEnd = to.m_line;
    int cIndexStart = getCharacterIndex(from);
    int cIndexEnd = getCharacterIndex(to);
    
    size_t s = 0;
    
    for (size_t i = lStart; i < lEnd; i++) {
      s += m_lines[i].size();
    }
    
    result.reserve(s + s / 8);
    
    while (cIndexStart < cIndexEnd || lStart < lEnd) {
      if(lStart >= (int)m_lines.size()) {
        break;
      }
      
      auto& line = m_lines[lStart];
      if(cIndexStart < (int)line.size()) {
        result += line[cIndexStart].m_char;
        cIndexStart++;
      } else {
        cIndexStart = 0;
        ++lStart;
        result+= "\n";
      }
    }
    
    return result;
  }
  
  void EditorUI::setText(const string& text) {
    PROFILE_START;
    m_lines.clear();
    m_lines.emplace_back(Line());
    for (auto character : text) {
      if (character == '\r') {
      } else if (character == '\n') {
        m_lines.emplace_back(Line());
      } else {
        m_lines.back().emplace_back(Glypth(character));
      }
    }
  }
  
  EditorUI::Coordinate EditorUI::sanitizeCoordinates(const Coordinate& value) const {
    PROFILE_START;
    int line = value.m_line;
    int column = value.m_column;
    if (line >= (int)m_lines.size()) {
      if (m_lines.empty()) {
        line = 0;
        column = 0;
      } else {
        line = (int)m_lines.size() - 1;
        column = getLineMaxColumn(line);
      }
      return {line, column};
    }
    column = m_lines.empty() ? 0 : std::min(column, getLineMaxColumn(line));
    return {line, column};
  }
  
  EditorUI::Coordinate EditorUI::screenPosToCoordinates(const ImVec2& position) const {
    PROFILE_START;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 local(position.x - origin.x, position.y - origin.y);
    
    int lineNo = std::max(0, (int)floor(local.y / m_charAdvance.y));
    
    int columnCoord = 0;
    
    if (lineNo >= 0 && lineNo < (int)m_lines.size()) {
      auto& line = m_lines.at(lineNo);
      
      int columnIndex = 0;
      float columnX = 0.0f;
      
      while ((size_t)columnIndex < line.size()) {
        float columnWidth = 0.0f;
        
        if (line[columnIndex].m_char == '\t') {
          float spaceSize =
            ImGui::GetFont()
            ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, " ")
            .x;
          float oldX = columnX;
          float newColumnX = (1.0f + std::floor((1.0f + columnX) /
                                                (float(m_tabSize) * spaceSize))) *
            (float(m_tabSize) * spaceSize);
          columnWidth = newColumnX - oldX;
          if (m_textStartPixel + columnX + columnWidth * 0.5f > local.x) break;
          columnX = newColumnX;
          columnCoord = (columnCoord / m_tabSize) * m_tabSize + m_tabSize;
          columnIndex++;
        } else {
          char buf[7];
          auto d = UTF8CharLength(line[columnIndex].m_char);
          int i = 0;
          while (i < 6 && d-- > 0) buf[i++] = line[columnIndex++].m_char;
          buf[i] = '\0';
          columnWidth =
            ImGui::GetFont()
            ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf)
            .x;
          if (m_textStartPixel + columnX + columnWidth * 0.5f > local.x) break;
          columnX += columnWidth;
          columnCoord++;
        }
      }
    }
    
    return sanitizeCoordinates(Coordinate(lineNo, columnCoord));
  }
  
  EditorUI::Coordinate EditorUI::findWordEnd(const Coordinate& from) const {
    PROFILE_START;
    if (from.m_line >= (int)m_lines.size()) {
      return from;
    }
    
    auto& line = m_lines[from.m_line];
    auto cindex = getCharacterIndex(from);
    
    if (cindex >= (int)line.size()) {
      return from;
    }
    
    bool prevSpace = (bool)isspace(line[cindex].m_char);
    
    while (cindex < (int)line.size()) {
      auto c = line[cindex].m_char;
      auto d = UTF8CharLength(c);
      
      if (prevSpace != !!isspace(c)) {
        if (isspace(c)) {
          while (cindex < (int)line.size() && isspace(line[cindex].m_char)) {
            ++cindex;
          }
          
          break;
        }
        cindex += d;
      }
    }
    
    return {from.m_line, getCharacterColumn(from.m_line, cindex)};
  }
  
  EditorUI::Coordinate EditorUI::findWordStart(const Coordinate& from) const {
    PROFILE_START;
    if (from.m_line >= (int)m_lines.size()) {
      return from;
    }
    
    auto& line = m_lines[from.m_line];
    auto cindex = getCharacterIndex(from);
    
    if (cindex >= (int)line.size()) {
      return from;
    }
    
    bool moved = false;
    while (cindex > 0 && isspace(line[cindex].m_char)) {
      cindex--;
      moved = true;
    }
    
    if(moved) {
      return {from.m_line, getCharacterColumn(from.m_line, cindex)};
    }
    
    moved = false;
    
    while(cindex > 0 && !isANWord(line[cindex].m_char)) {
      cindex--;
      moved = true;
    }
    
    if(moved) {
      return {from.m_line, getCharacterColumn(from.m_line, cindex + 1)};
    }
    
    while (true) {
      if(cindex <= 0) {
        return {from.m_line, getCharacterColumn(from.m_line, cindex)};
      }
      
      auto c = line[cindex].m_char;
      
      if(isANWord(c)) {
        cindex--;
      } else {
        break;
      }
    }
    
    return {from.m_line, getCharacterColumn(from.m_line, cindex)};
  }
  
  bool EditorUI::isOnWordBoundary(const Coordinate& at) const {
    PROFILE_START;
    if (at.m_line >= (int)m_lines.size() || at.m_column == 0) {
      return true;
    }
    
    auto& line = m_lines[at.m_line];
    auto cindex = getCharacterIndex(at);
    
    if (cindex >= (int)line.size()) {
      return true;
    }
    
    return isspace(line[cindex].m_char) != isspace(line[cindex - 1].m_char);
  }
  
  void EditorUI::setSelection(const Coordinate& start, const Coordinate& end,
                              SelectionMode mode) {
    PROFILE_START;
    auto oldSelStart = m_editorState.m_selectionStart;
    auto oldSelEnd = m_editorState.m_selectionEnd;
    
    m_editorState.m_selectionStart = sanitizeCoordinates(start);
    m_editorState.m_selectionEnd = sanitizeCoordinates(end);
    if (m_editorState.m_selectionStart > m_editorState.m_selectionEnd) {
      std::swap(m_editorState.m_selectionStart, m_editorState.m_selectionEnd);
    }
    
    switch (mode) {
      case SelectionMode::Normal:
      break;
      case SelectionMode::Word: {
        m_editorState.m_selectionStart =
          findWordStart(m_editorState.m_selectionStart);
        
        if (!isOnWordBoundary(m_editorState.m_selectionEnd)) {
          m_editorState.m_selectionEnd =
            findWordEnd(findWordStart(m_editorState.m_selectionEnd));
        }
        break;
      }
      case SelectionMode::Line: {
        const auto lineNo = m_editorState.m_selectionEnd.m_line;
        const auto lineSize =
          (size_t)lineNo < m_lines.size() ? m_lines[lineNo].size() : 0;
        m_editorState.m_selectionStart =
          Coordinate(m_editorState.m_selectionStart.m_line, 0);
        m_editorState.m_selectionEnd =
          Coordinate(lineNo, getLineMaxColumn(lineNo));
        break;
      }
      default:
      break;
    }
    
    if (m_editorState.m_selectionStart != oldSelStart ||
        m_editorState.m_selectionEnd != oldSelEnd) {
      m_cursoPositionChanged = true;
    }
  }
  
  void EditorUI::handleMouseInput() {
    PROFILE_START;
    ImGuiIO& io = ImGui::GetIO();
    auto shift = io.KeyShift;
    auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;
    
    if (ImGui::IsWindowHovered()) {
      if (!shift && !alt) {
        auto click = ImGui::IsMouseClicked(0);
        auto doubleClick = ImGui::IsMouseDoubleClicked(0);
        auto t = ImGui::GetTime();
        auto tripleClick =
          click && !doubleClick &&
          (m_lastClick != -1.0f && (t - m_lastClick) < io.MouseDoubleClickTime);
        
        if (tripleClick) {
          if (!ctrl) {
            m_editorState.m_cursorPosition = m_interactiveEnd =
              m_interactiveStart = screenPosToCoordinates(ImGui::GetMousePos());
            m_selectionMode = SelectionMode::Line;
            setSelection(m_interactiveStart, m_interactiveEnd, m_selectionMode);
          }
          
          m_lastClick = -1.0f;
        } else if (doubleClick) {
          if (!ctrl) {
            m_editorState.m_cursorPosition = m_interactiveEnd =
              m_interactiveStart = screenPosToCoordinates(ImGui::GetMousePos());
            if (m_selectionMode == SelectionMode::Line) {
              m_selectionMode = SelectionMode::Normal;
            } else {
              m_selectionMode = SelectionMode::Word;
            }
            setSelection(m_interactiveStart, m_interactiveEnd, m_selectionMode);
          }
          
          m_lastClick = (float)ImGui::GetTime();
        } else if (click) {
          m_editorState.m_cursorPosition = m_interactiveEnd = m_interactiveStart =
            screenPosToCoordinates(ImGui::GetMousePos());
          if (ctrl) {
            m_selectionMode = SelectionMode::Word;
          } else {
            m_selectionMode = SelectionMode::Normal;
          }
          setSelection(m_interactiveStart, m_interactiveEnd, m_selectionMode);
          
          m_lastClick = (float)ImGui::GetTime();
        } else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
          io.WantCaptureMouse = true;
          m_editorState.m_cursorPosition = m_interactiveEnd =
            screenPosToCoordinates(ImGui::GetMousePos());
          setSelection(m_interactiveStart, m_interactiveEnd, m_selectionMode);
        }
      }
    }
  }
  
  EditorUI::Coordinate EditorUI::getActualCursorCoordinates() const {
    PROFILE_START;
    return sanitizeCoordinates(m_editorState.m_cursorPosition);
  }
  
  void EditorUI::ensureCursorVisible() {
    PROFILE_START;
    float scrollX = ImGui::GetScrollX();
    float scrollY = ImGui::GetScrollY();
    
    auto height = ImGui::GetWindowHeight();
    auto width = ImGui::GetWindowWidth();
    
    auto top = 1 + (int)ceil(scrollY / m_charAdvance.y);
    auto bottom = (int)ceil((scrollY + height) / m_charAdvance.y);
    
    auto left = (int)ceil(scrollX / m_charAdvance.x);
    auto right = (int)ceil((scrollX + width) / m_charAdvance.x);
    
    auto pos = getActualCursorCoordinates();
    auto len = textDistanceToLineStart(pos);
    
    if (pos.m_line < top) {
      ImGui::SetScrollY(std::max(0.0f, (pos.m_line - 1) * m_charAdvance.y));
    }
    if (pos.m_line > bottom - 4) {
      ImGui::SetScrollY(
                        std::max(0.0f, (pos.m_line + 4) * m_charAdvance.y - height));
    }
    if (len + m_textStartPixel < left + 4) {
      ImGui::SetScrollX(std::max(0.0f, len + m_textStartPixel - 4));
    }
    if (len + m_textStartPixel > right - 4) {
      ImGui::SetScrollX(std::max(0.0f, len + m_textStartPixel + 4 - width));
    }
  }
  
  void EditorUI::setCursorPosition(const Coordinate& pos) {
    PROFILE_START;
    if (m_editorState.m_cursorPosition == pos) {
      return;
    }
    
    m_editorState.m_cursorPosition = pos;
    m_cursoPositionChanged = true;
    ensureCursorVisible();
  }
  
  int EditorUI::getCharacterColumn(int lineIndex, int index) const {
    PROFILE_START;
    if (lineIndex >= m_lines.size()) {
      return 0;
    }
    auto& line = m_lines[lineIndex];
    int col = 0;
    int i = 0;
    while (i < index && i < (int)line.size()) {
      auto c = line[i].m_char;
      i += UTF8CharLength(c);
      if (c == '\t')
        col = (col / m_tabSize) * m_tabSize + m_tabSize;
      else
        col++;
    }
    return col;
  }
  
  void EditorUI::moveUp(int amount, bool shift) {
    PROFILE_START;
    auto oldPos = m_editorState.m_cursorPosition;
    m_editorState.m_cursorPosition.m_line =
      std::max(0, m_editorState.m_cursorPosition.m_line - amount);
    
    if (oldPos != m_editorState.m_cursorPosition) {
      if (shift) {
        if (oldPos == m_interactiveStart) {
          m_interactiveStart = m_editorState.m_cursorPosition;
        } else if (oldPos == m_interactiveEnd) {
          m_interactiveEnd = m_editorState.m_cursorPosition;
        } else {
          m_interactiveStart = m_editorState.m_cursorPosition;
          m_interactiveEnd = oldPos;
        }
      } else {
        m_interactiveStart = m_interactiveEnd = m_editorState.m_cursorPosition;
      }
      setSelection(m_interactiveStart, m_interactiveEnd, SelectionMode::Normal);
      ensureCursorVisible();
    }
  }
  
  void EditorUI::moveDown(int amount, bool shift) {
    PROFILE_START;
    auto oldPos = m_editorState.m_cursorPosition;
    m_editorState.m_cursorPosition.m_line =
      std::max(0, std::min((int)m_lines.size() - 1,
                           m_editorState.m_cursorPosition.m_line + amount));
    
    if (m_editorState.m_cursorPosition != oldPos) {
      if (shift) {
        if (oldPos == m_interactiveEnd) {
          m_interactiveEnd = m_editorState.m_cursorPosition;
        } else if (oldPos == m_interactiveStart) {
          m_interactiveStart = m_editorState.m_cursorPosition;
        } else {
          m_interactiveStart = oldPos;
          m_interactiveEnd = m_editorState.m_cursorPosition;
        }
      } else {
        m_interactiveStart = m_interactiveEnd = m_editorState.m_cursorPosition;
      }
      setSelection(m_interactiveStart, m_interactiveEnd, SelectionMode::Normal);
      ensureCursorVisible();
    }
  }
  
  void EditorUI::moveLeft(int amount, bool shift, bool ctrl) {
    PROFILE_START;
    if (m_lines.empty()) {
      return;
    }
    
    auto oldPos = m_editorState.m_cursorPosition;
    m_editorState.m_cursorPosition = getActualCursorCoordinates();
    
    auto line = m_editorState.m_cursorPosition.m_line;
    auto cindex = getCharacterIndex(m_editorState.m_cursorPosition);
    
    while (amount-- > 0) {
      if (cindex == 0) {
        if (line > 0) {
          --line;
          if (m_lines.size() > line) {
            cindex = (int)m_lines[line].size();
          } else {
            cindex = 0;
          }
        }
      } else {
        --cindex;
        if (cindex > 0) {
          if ((int)m_lines.size() > line) {
            while (cindex > 0 && isUTF8Sequence(m_lines[line][cindex].m_char)) {
              --cindex;
            }
          }
        }
      }
      
      m_editorState.m_cursorPosition =
        Coordinate(line, getCharacterColumn(line, cindex));
      
      if (ctrl) {
        m_editorState.m_cursorPosition =
          findWordStart(m_editorState.m_cursorPosition);
        cindex = getCharacterIndex(m_editorState.m_cursorPosition);
      }
    }
    
    m_editorState.m_cursorPosition =
      Coordinate(line, getCharacterColumn(line, cindex));
    
    if (shift) {
      if (oldPos == m_interactiveStart) {
        m_interactiveStart = m_editorState.m_cursorPosition;
      } else if (oldPos == m_interactiveEnd) {
        m_interactiveEnd = m_editorState.m_cursorPosition;
      } else {
        m_interactiveStart = m_editorState.m_cursorPosition;
        m_interactiveEnd = oldPos;
      }
    } else {
      m_interactiveStart = m_interactiveEnd = m_editorState.m_cursorPosition;
    }
    
    setSelection(m_interactiveStart, m_interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  EditorUI::Coordinate EditorUI::findNextWord(const EditorUI::Coordinate& from) const {
    PROFILE_START;
    Coordinate at = from;
    
    if (at.m_line >= (int)m_lines.size()) {
      return at;
    }
    
    auto cindex = getCharacterIndex(at);
    auto& line = m_lines[at.m_line];
    bool moved = false;
    
    while (cindex < (int)line.size() && isspace(line[cindex].m_char)) {
      cindex++;
      moved = true;
    }
    
    if (moved) {
      return {at.m_line, getCharacterColumn(at.m_line, cindex)};
    }
    
    moved = false;
    
    while (cindex < (int)line.size() && !isANWord(line[cindex].m_char)) {
      cindex++;
      moved = true;
    }
    
    if (moved) {
      return {at.m_line, getCharacterColumn(at.m_line, cindex)};
    }
    
    while (true) {
      if (cindex >= line.size()) {
        return {at.m_line, getCharacterColumn(at.m_line, cindex)};
      }
      
      if (isANWord(line[cindex].m_char)) {
        cindex++;
      } else if (isspace(line[cindex].m_char)) {
        cindex++;
        break;
      } else {
        break;
      }
    }
    
    return {at.m_line, getCharacterColumn(at.m_line, cindex)};
  }
  
  void EditorUI::moveRight(int amount, bool shift, bool ctrl) {
    PROFILE_START;
    auto oldPos = m_editorState.m_cursorPosition;
    
    if (m_lines.empty() || oldPos.m_line >= m_lines.size()) {
      return;
    }
    
    auto cindex = getCharacterIndex(oldPos);
    
    while (amount-- > 0) {
      auto lineIndex = m_editorState.m_cursorPosition.m_line;
      auto& line = m_lines[lineIndex];
      
      if (cindex >= line.size()) {
        if (m_editorState.m_cursorPosition.m_line < m_lines.size() - 1) {
          m_editorState.m_cursorPosition.m_line =
            std::max(0, std::min((int)m_lines.size() - 1,
                                 m_editorState.m_cursorPosition.m_line + 1));
          m_editorState.m_cursorPosition.m_column = 0;
        } else {
          return;
        }
      } else {
        if (ctrl) {
          m_editorState.m_cursorPosition =
            findNextWord(m_editorState.m_cursorPosition);
        } else {
          cindex += UTF8CharLength(line[cindex].m_char);
          m_editorState.m_cursorPosition =
            Coordinate(lineIndex, getCharacterColumn(lineIndex, cindex));
        }
      }
    }
    
    if (shift) {
      if (oldPos == m_interactiveStart) {
        m_interactiveStart = m_editorState.m_cursorPosition;
      } else if (oldPos == m_interactiveEnd) {
        m_interactiveEnd = sanitizeCoordinates(m_editorState.m_cursorPosition);
      } else {
        m_interactiveStart = oldPos;
        m_interactiveEnd = m_editorState.m_cursorPosition;
      }
    } else {
      m_interactiveStart = m_interactiveEnd = m_editorState.m_cursorPosition;
    }
    
    setSelection(m_interactiveStart, m_interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::moveEnd(bool shift) {
    PROFILE_START;
    if (m_lines.empty()) {
      return;
    }
    
    int lineNo = m_editorState.m_cursorPosition.m_line;
    auto& line = m_lines[lineNo];
    
    if(line.empty()) {
      return;
    }
    
    auto old = m_editorState.m_cursorPosition;
    
    m_editorState.m_cursorPosition = Coordinate(lineNo, getLineMaxColumn(lineNo));
    
    if(shift) {
      if(hasSelection()) {
        m_interactiveEnd = m_editorState.m_cursorPosition;
        m_interactiveStart = m_editorState.m_selectionStart;
      } else {
        m_interactiveStart = old;
        m_interactiveEnd = m_editorState.m_cursorPosition;
      }
    } else {
      m_interactiveStart = m_interactiveEnd = m_editorState.m_cursorPosition;
    }
    
    setSelection(m_interactiveStart, m_interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::moveHome(bool shift) {
    PROFILE_START;
    if (m_lines.empty()) {
      return;
    }
    
    int lineNo = m_editorState.m_cursorPosition.m_line;
    
    int oldCindex = getCharacterIndex(m_editorState.m_cursorPosition);
    
    auto& line = m_lines[lineNo];
    
    if(line.empty()) {
      return;
    }
    auto old = m_editorState.m_cursorPosition;
    
    Coordinate beginning = Coordinate(lineNo, 0);
    beginning = findNextWord(beginning);
    m_editorState.m_cursorPosition = beginning;
    
    if(shift) {
      if(hasSelection()) {
        m_interactiveStart = m_editorState.m_cursorPosition;
        m_interactiveEnd = m_editorState.m_selectionEnd;
      } else {
        m_interactiveStart = m_editorState.m_cursorPosition;
        m_interactiveEnd = old;
      }
    } else {
      m_interactiveStart = m_interactiveEnd = m_editorState.m_cursorPosition; 
    }
    
    setSelection(m_interactiveStart, m_interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::moveTop(bool shift) {
    PROFILE_START;
    if (m_lines.empty()) {
      return;
    }
    
    auto old = m_editorState.m_cursorPosition;
    m_editorState.m_cursorPosition = Coordinate(0, 0);
    
    if(shift) {
      if (hasSelection()) {
        m_interactiveEnd = m_editorState.m_selectionStart;
      } else {
        m_interactiveEnd = old;
      }
      m_interactiveStart = m_editorState.m_cursorPosition;
    }
    
    setSelection(m_interactiveStart, m_interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::moveBottom(bool shift) {
    PROFILE_START;
    if (m_lines.empty()) {
      return;
    }
    
    auto old = m_editorState.m_cursorPosition;
    m_editorState.m_cursorPosition = Coordinate((int)m_lines.size() - 1, 0);
    
    if(shift) {
      if (hasSelection()) {
        m_interactiveStart = m_editorState.m_selectionStart;
      } else {
        m_interactiveStart = old;
      }
      m_interactiveEnd = m_editorState.m_cursorPosition;
    }
    
    setSelection(m_interactiveStart, m_interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::deleteSelection() {
    PROFILE_START;
    if (!hasSelection()) {
      return;
    }
    
    if (m_editorState.m_selectionStart == m_editorState.m_selectionEnd) {
      return;
    }
    
    deleteRange(m_editorState.m_selectionStart, m_editorState.m_selectionEnd);
    setCursorPosition(m_editorState.m_selectionStart);
  }
  
  void EditorUI::copy() {
    if(hasSelection()) {
      ImGui::SetClipboardText(getSelectedText().c_str());
    } else if(!m_lines.empty()) {
      std::string result;
      auto& line = m_lines[getActualCursorCoordinates().m_line];
      for (auto &g : line) {
        result.push_back(g.m_char);
      }
      ImGui::SetClipboardText(result.c_str());
    }
  }
  
  inline EditorUI::Line &EditorUI::insertLine(int index) {
    return *m_lines.insert(m_lines.begin() + index, Line());
  }
  
  int EditorUI::insertTextAt(Coordinate &pos, const char *value) {
    PROFILE_START;
    int cindex = getCharacterIndex(pos);
    int totalLines = 0;
    while (*value != '\0') {
      if (*value == '\r') {
        ++value;
      } else if (*value == '\n') {
        if (cindex < (int)m_lines[pos.m_line].size()) {
          auto& newLine = insertLine(pos.m_line + 1);
          auto& line = m_lines[pos.m_line];
          newLine.insert(newLine.begin(), line.end() + cindex, line.end());
          line.erase(line.begin() + cindex, line.end());
        } else {
          insertLine(pos.m_line + 1);
        }
        pos.m_line++;
        pos.m_column = 0;
        ++totalLines;
        ++value;
      } else {
        auto& line = m_lines[pos.m_line];
        auto d = UTF8CharLength(*value);
        while (d-- > 0 && *value != '\0') {
          line.insert(line.begin() + cindex++, Glypth(*value++));
        }
        pos.m_column++;
      }
      m_textChanged = true;
    }
    return totalLines;
  }
  
  void EditorUI::insertText(const char *value) {
    if (!value) {
      return;
    }
    
    auto pos = getActualCursorCoordinates();
    auto start = std::min(pos, m_editorState.m_selectionStart);
    int totalLines = pos.m_line - start.m_line;
    
    totalLines += insertTextAt(pos, value);
    
    setSelection(pos, pos, SelectionMode::Normal);
    setCursorPosition(pos);
  }
  
  void EditorUI::insertText(const std::string& value) {
    insertText(value.c_str());
  }
  
  void EditorUI::paste() {
    if (m_readOnly) {
      return;
    }
    
    auto clipText = ImGui::GetClipboardText();
    
    if (clipText != nullptr && strlen(clipText) > 0) {
      if(hasSelection()) {
        deleteSelection();
      }
      insertText(clipText);
    }
  }
  
  void EditorUI::cut() {
    copy();
    if (hasSelection()) {
      deleteSelection();
    } else {
      int lineNo = m_editorState.m_cursorPosition.m_line;
      bool lastLine = lineNo == (int)m_lines.size() - 1;
      deleteLine(lineNo, lineNo + 1);
      if (lastLine) {
        insertLine(lineNo);
        m_editorState.m_cursorPosition = Coordinate(lineNo, 0);
      }
    }
  }
  
  void EditorUI::selectAll() {
    PROFILE_START;
    if (m_lines.size() == 0) {
      return;
    }
    
    m_interactiveStart = Coordinate(0, 0);
    int lastLineNo = m_lines.size() - 1;
    Coordinate end = Coordinate(lastLineNo, getLineMaxColumn(lastLineNo));
    m_interactiveEnd = m_editorState.m_cursorPosition = end;
    
    setSelection(m_interactiveStart, m_interactiveEnd, SelectionMode::Normal);
  }
  
  void EditorUI::insertCharacter(ImWchar c, bool shift) {
    PROFILE_START;
    if (hasSelection()) {
      if (c == '\t' && m_editorState.m_selectionStart.m_line == m_editorState.m_selectionEnd.m_line) {
        auto start = m_editorState.m_selectionStart;
        auto end = m_editorState.m_selectionEnd;
        auto originalEnd = end;
        
        if (start > end) {
          std::swap(start, end);
        }
        
        start.m_column = 0;
        if (end.m_column == 0 && end.m_line > 0) {
          --end.m_line;
        }
        if (end.m_line >= (int)m_lines.size()) {
          end.m_line = m_lines.empty() ? 0 : (int)m_lines.size() - 1;
        }
        end.m_column = getLineMaxColumn(end.m_line);
        
        bool modified = false;
        
        for (int i = start.m_line; i <= end.m_line; i++) {
          auto& line = m_lines[i];
          if (shift) {
            if (!line.empty()) {
              if (line.front().m_char == '\t') {
                line.erase(line.begin());
                modified = true;
              } else {
                for (int j = 0; j < m_tabSize && !line.empty() && line.front().m_char == ' '; j++) {
                  line.erase(line.begin());
                  modified = true;
                }
              }
            }
          } else {
            line.insert(line.begin(), Glypth('\t'));
            modified = true;
          }
        }
        
        if (modified) {
          start = Coordinate(start.m_line, getCharacterColumn(start.m_line, 0));
          Coordinate rangeEnd;
          if (originalEnd.m_column != 0) {
            end = Coordinate(end.m_line, getLineMaxColumn(end.m_line));
            rangeEnd = end;
          } else {
            end = Coordinate(originalEnd.m_line, 0);
            rangeEnd = Coordinate(end.m_line - 1, getLineMaxColumn(end.m_line - 1));
          }
          m_textChanged = true;
          ensureCursorVisible();
        }
        return;
      } else {
        deleteSelection();
      }
    }
    
    auto coord = getActualCursorCoordinates();
    
    if (c == '\n') {
      insertLine(coord.m_line + 1);
      auto &line = m_lines[coord.m_line];
      auto &newLine = m_lines[coord.m_line + 1];
      
      // TODO(Maxlisui): Auto Indentation
      
      const size_t whiteSpaceSize = newLine.size();
      auto cindex = getCharacterIndex(coord);
      newLine.insert(newLine.begin(), line.begin() + cindex, line.end());
      line.erase(line.begin() + cindex, line.begin() + line.size());
      setCursorPosition(Coordinate(coord.m_line + 1, getCharacterColumn(coord.m_line + 1, (int)whiteSpaceSize)));
    } else {
      char buf[7];
      int e = ImTextCharToUtf8(buf, 7, c);
      
      if (e <= 0) {
        return;
      }
      
      buf[e] = '\0';
      auto &line = m_lines[coord.m_line];
      auto cindex = getCharacterIndex(coord);
      
      if (m_override && cindex < (int)line.size()) {
        auto d = UTF8CharLength(line[cindex].m_char);
        
        while (d-- > 0 && cindex < (int)line.size()) {
          line.erase(line.begin() + cindex);
        }
      }
      
      for (auto p = buf; *p != '\0'; p++, cindex++) {
        line.insert(line.begin() + cindex, Glypth(*p));
      }
      
      setCursorPosition(Coordinate(coord.m_line, getCharacterColumn(coord.m_line, cindex)));
    }
    
    m_textChanged = true;
    ensureCursorVisible();
  }

  void EditorUI::backspace() {
    if (m_lines.empty()) {
      return;
    }

    if (hasSelection()) {
      deleteSelection();
    } else {
      auto pos = getActualCursorCoordinates();
      setCursorPosition(pos);

      if (m_editorState.m_cursorPosition.m_column == 0) {
        if (m_editorState.m_cursorPosition.m_line == 0) {
          return;
        }

        int lineNo = m_editorState.m_cursorPosition.m_line;
        auto& line = m_lines[lineNo];
        auto& prevLine = m_lines[lineNo];
        auto prevSize = getLineMaxColumn(lineNo - 1);

        prevLine.insert(prevLine.end(), line.begin(), line.end());

        deleteLine(lineNo);
        --m_editorState.m_cursorPosition.m_line;
        m_editorState.m_cursorPosition.m_column = prevSize;
      } else {
        auto& line = m_lines[m_editorState.m_cursorPosition.m_line];
        auto cindex = getCharacterIndex(pos) - 1;
        auto cend = cindex + 1;
        while (cindex > 0 && isUTF8Sequence(line[cindex].m_char)) {
          cindex--;
        }

        --m_editorState.m_cursorPosition.m_column;

        while (cindex < line.size() && cend-- > cindex) {
          line.erase(line.begin() + cindex);
        }
      }

      m_textChanged = true;
      ensureCursorVisible();
    }
  }

  void EditorUI::remove() {
    if (m_lines.empty()) {
      return;
    }

    if (hasSelection()) {
      deleteSelection();
    }

    auto pos = getActualCursorCoordinates();
    setCursorPosition(pos);
    auto& line = m_lines[pos.m_line];

    if (pos.m_column == getLineMaxColumn(pos.m_line)) {
      if (pos.m_line == (int)m_lines.size() - 1) {
        return;
      }

      auto& nextLine = m_lines[pos.m_line + 1];
      line.insert(line.end(), nextLine.begin(), nextLine.end());
      deleteLine(pos.m_line + 1);
    } else {
      auto cindex = getCharacterIndex(pos);
      auto d = UTF8CharLength(line[cindex].m_char);
      while (d-- > 0 && cindex < (int)line.size()) {
        line.erase(line.begin() + cindex);
      }
    }

    m_textChanged = true;
  }
  
  void EditorUI::handleKeyboardInput() {
    PROFILE_START;
    ImGuiIO& io = ImGui::GetIO();
    auto shift = io.KeyShift;
    auto ctrl = io.ConfigMacOSXBehaviors ? io.KeySuper : io.KeyCtrl;
    auto alt = io.ConfigMacOSXBehaviors ? io.KeyCtrl : io.KeyAlt;
    
    if (ImGui::IsWindowFocused()) {
      if (ImGui::IsWindowHovered()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_TextInput);
      }
      
      io.WantCaptureKeyboard = true;
      io.WantTextInput = true;
      
      if (!ctrl && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_UpArrow))) {
        moveUp(1, shift);
      } else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageUp))) {
        moveUp(getPageSize() - 4, shift);
      } else if (!ctrl && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_DownArrow))) {
        moveDown(1, shift);
      } else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_PageDown))) {
        moveDown(getPageSize() - 4, shift);
      } else if (!alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_LeftArrow))) {
        moveLeft(1, shift, ctrl);
      } else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_RightArrow))) {
        moveRight(1, shift, ctrl);
      } else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End))) {
        moveEnd(shift);
      } else if (!alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home))) {
        moveHome(shift);
      } else if (!alt && !ctrl && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_End))) {
        moveBottom(shift);
      } else if (!alt && !ctrl && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Home))) {
        moveTop(shift);
      } else if (!m_readOnly && !ctrl && !shift && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
        remove();
      } else if (!m_readOnly && !ctrl && !shift && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace))) {
        backspace();
      } else if (ctrl && !shift && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert))) {
        copy();

      } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C))) {
        copy();
      } else if (!m_readOnly && !ctrl && shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert))) {
        paste();
      } else if (!m_readOnly && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_V))) {
        paste();
      } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_X))) {
        cut();
      } else if (!ctrl && shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
        cut();
      } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A))) {
        selectAll();
      } else if (!m_readOnly && !ctrl && !shift && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) {
        insertCharacter('\n', false);
      } else if (!m_readOnly && !ctrl && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab))) {
        insertCharacter('\n', shift);
      }

      if (!m_readOnly && !io.InputQueueCharacters.empty()) {
        for (int i = 0; i < io.InputQueueCharacters.Size; i++) {
          auto c = io.InputQueueCharacters[i];
          if (c != 0 && (c == '\n' || c >= 32)) {
            insertCharacter(c, shift);
          }
        }
        io.InputQueueCharacters.resize(0);
      }
    }
  }
  
  void EditorUI::render() {
    PROFILE_START;
    
    m_cursoPositionChanged = false;
    
    handleKeyboardInput();
    handleMouseInput();
    
    const float fontSize = ImGui::GetFont()
      ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX,
                      -1.f, "#", nullptr, nullptr)
      .x;
    
    m_charAdvance =
      ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * m_lineSpacing);
    
    auto contentSize = ImGui::GetWindowContentRegionMax();
    auto drawList = ImGui::GetWindowDrawList();
    
    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    auto scrollX = ImGui::GetScrollX();
    auto scrollY = ImGui::GetScrollY();
    float longest = m_textStartPixel;
    
    m_lineBuffer.clear();
    
    auto lineNo = (int)floor(scrollY / m_charAdvance.y);
    
    auto lineMax = std::max(
                            0, std::min(
                                        (int)m_lines.size(),
                                        lineNo + (int)floor((scrollY + contentSize.y) / m_charAdvance.y)));
    
    if (!m_lines.empty()) {
      float spaceSize = ImGui::GetFont()
        ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f,
                        " ", nullptr, nullptr)
        .x;
      
      char buf[16];
      
      while (lineNo < lineMax) {
        auto lineStartScreenPos = ImVec2(
                                         cursorScreenPos.x, cursorScreenPos.y + lineNo * m_charAdvance.y);
        auto textScreenPos =
          ImVec2(lineStartScreenPos.x + m_textStartPixel, lineStartScreenPos.y);
        
        auto& line = m_lines[lineNo];
        
        auto lineMaxColumn = getLineMaxColumn(lineNo);
        
        longest = std::max(m_textStartPixel + textDistanceToLineStart(Coordinate(
                                                                                 lineNo, lineMaxColumn)),
                           longest);
        
        Coordinate lineStartCoord = Coordinate(lineNo, 0);
        Coordinate lineEndCoord = Coordinate(lineNo, getLineMaxColumn(lineNo));
        
        float sstart = -1.f;
        float ssend = -1.f;
        
        if (m_editorState.m_selectionStart <= lineEndCoord) {
          sstart = m_editorState.m_selectionStart > lineStartCoord
            ? textDistanceToLineStart(m_editorState.m_selectionStart)
            : 0.0f;
        }
        if (m_editorState.m_selectionEnd > lineStartCoord) {
          ssend =
            textDistanceToLineStart(m_editorState.m_selectionEnd < lineEndCoord
                                    ? m_editorState.m_selectionEnd
                                    : lineEndCoord);
        }
        
        if (m_editorState.m_selectionEnd.m_line > lineNo) {
          ssend += m_charAdvance.x;
        }
        
        if (sstart != -1 && ssend != -1 && sstart < ssend) {
          ImVec2 vstart(lineStartScreenPos.x + m_textStartPixel + sstart,
                        lineStartScreenPos.y);
          ImVec2 vend(lineStartScreenPos.x + m_textStartPixel + ssend,
                      lineStartScreenPos.y + m_charAdvance.y);
          drawList->AddRectFilled(vstart, vend, 0x80a06020);
        }
        
        auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);
        
        stbsp_snprintf(buf, 16, "%d ", lineNo + 1);
        
        auto lineNoWidth = ImGui::GetFont()
          ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX,
                          -1.0f, buf, nullptr, nullptr)
          .x;
        drawList->AddText(
                          ImVec2(lineStartScreenPos.x + m_textStartPixel - lineNoWidth,
                                 lineStartScreenPos.y),
                          0xff707000, buf);
        
        if (m_editorState.m_cursorPosition.m_line == lineNo) {
          auto focused = ImGui::IsWindowFocused();
          
          if (!hasSelection()) {
            auto end = ImVec2(start.x + contentSize.x + scrollX,
                              start.y + m_charAdvance.y);
            drawList->AddRectFilled(start, end,
                                    focused ? 0x40000000 : 0x40808080);
            drawList->AddRect(start, end, 0x40a0a0a0, 1.0f);
          }
          
          if (focused) {
            auto timeEnd =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                    std::chrono::system_clock::now().time_since_epoch())
              .count();
            auto elapsed = timeEnd - m_startTime;
            
            if (elapsed > 400) {
              float width = 1.0f;
              auto cindex = getCharacterIndex(m_editorState.m_cursorPosition);
              float cx = textDistanceToLineStart(m_editorState.m_cursorPosition);
              
              if (m_override && cindex < (int)line.size()) {
                auto c = line[cindex].m_char;
                if (c == '\t') {
                  auto x = (1.0f + std::floor((1.0f + cx) /
                                              (float(m_tabSize) * spaceSize))) *
                    (float(m_tabSize) * spaceSize);
                  width = x - cx;
                } else {
                  char buf2[2];
                  buf2[0] = line[cindex].m_char;
                  buf2[1] = '\0';
                  width = ImGui::GetFont()
                    ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX,
                                    -1.0f, buf2)
                    .x;
                }
              }
              
              ImVec2 cstart(textScreenPos.x + cx, lineStartScreenPos.y);
              ImVec2 cend(textScreenPos.x + cx + width,
                          lineStartScreenPos.y + m_charAdvance.y);
              drawList->AddRectFilled(cstart, cend, 0xffe0e0e0);
              
              if (elapsed > 800) {
                m_startTime = timeEnd;
              }
            }
          }
        }
        
        auto columnNo = 0;
        
        ImVec2 bufferOffset;
        
        // Render Text
        for (int i = 0; i < line.size();) {
          auto& glyph = line[i];
          auto l = UTF8CharLength(glyph.m_char);
          while (l-- > 0) {
            m_lineBuffer.push_back(line[i++].m_char);
          }
        }
        
        if (!m_lineBuffer.empty()) {
          const ImVec2 newOffset(textScreenPos.x + bufferOffset.x,
                                 textScreenPos.y + bufferOffset.y);
          drawList->AddText(newOffset, 0xffffffff, m_lineBuffer.c_str());
          m_lineBuffer.clear();
        }
        
        lineNo++;
      }
    }
    
    auto win = ImGui::GetCurrentWindow();
    float bottomLineHeight = 20.f;
    if (win->ScrollbarX) {
      auto scrollBarRect = ImGui::GetWindowScrollbarRect(win, ImGuiAxis_X);
      
      auto bottomMax = scrollBarRect.Max;
      auto bottomMin = scrollBarRect.Min;
      bottomMax.y = bottomMin.y;
      bottomMin.y -= bottomLineHeight;
      
      drawList->AddRectFilled(bottomMin, bottomMax, 0xFF3E3E42);
      
      char buf[256];
      stbsp_snprintf(buf, 256, "Ln: %i Ch:%i",
                     m_editorState.m_cursorPosition.m_line,
                     m_editorState.m_cursorPosition.m_column);
      
      auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr);
      
      auto lineColPos =
        ImVec2(bottomMax.x - textSize.x - 5.f,
               bottomMin.y + ((bottomLineHeight - textSize.y) / 2));
      
      drawList->AddText(lineColPos, 0xFFFFFFFF, buf);
      
      if(hasSelection()) {
        char buf2[256];
        
        stbsp_snprintf(buf, 256, "Sel: %i %i - %i %i", m_editorState.m_selectionStart.m_line, 
                       m_editorState.m_selectionStart.m_column,
                       m_editorState.m_selectionEnd.m_line,
                       m_editorState.m_selectionEnd.m_column);
        
        textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr);
        lineColPos = ImVec2(lineColPos.x - 5.f - textSize.x, lineColPos.y);
        drawList->AddText(lineColPos, 0xFFFFFFFF, buf);
      }
      
    }
    ImGui::Dummy(ImVec2((longest + 2),
                        (m_lines.size() * m_charAdvance.y) + bottomLineHeight));
  }
}  // namespace UI
