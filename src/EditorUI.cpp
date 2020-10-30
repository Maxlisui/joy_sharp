#include "EditorUI.h"
#include "Tooling.h"
#include "../vendor/imgui/imgui.h"
#include "../vendor/imgui/imgui_internal.h"
#include "../vendor/stb/stb_sprintf.h"
#include <iostream>

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
    return editorState.selectionEnd > editorState.selectionStart;
  }
  
  inline std::string EditorUI::getSelectedText() const {
    return getText(editorState.selectionStart, editorState.selectionEnd);
  }
  
  int EditorUI::getLineMaxColumn(int l) const {
    PROFILE_START;
    if (l >= lines.size()) return 0;
    auto& line = lines[l];
    int col = 0;
    for (unsigned i = 0; i < line.size();) {
      auto c = line[i].m_char;
      if (c == '\t')
        col = (col / tabSize) * tabSize + tabSize;
      else
        col++;
      i += UTF8CharLength(c);
    }
    return col;
  }
  
  inline int EditorUI::getPageSize() const {
    float height = ImGui::GetWindowHeight() - 20.f;
    return (int)floor(height / charAdvance.y);
  }
  
  int EditorUI::getCharacterIndex(const EditorUI::Coordinate& from) const {
    PROFILE_START;
    if (from.line >= lines.size()) return -1;
    auto& line = lines[from.line];
    int c = 0;
    int i = 0;
    for (; i < line.size() && c < from.column;) {
      if (line[i].m_char == '\t') {
        c = (c / tabSize) * tabSize + tabSize;
      } else {
        ++c;
      }
      i += UTF8CharLength(line[i].m_char);
    }
    return i;
  }
  
  float EditorUI::textDistanceToLineStart(const EditorUI::Coordinate& from) const {
    PROFILE_START;
    auto& line = lines[from.line];
    
    float distance = 0.0f;
    float spaceSize = ImGui::GetFont()
      ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f,
                      " ", nullptr, nullptr)
      .x;
    
    int colIndex = getCharacterIndex(from);
    
    for (size_t i = 0u; i < line.size() && i < colIndex;) {
      if (line[i].m_char == '\t') {
        distance = (1.f + std::floor((1.f + distance) /
                                     (float(tabSize) * spaceSize))) *
          (float(tabSize) * spaceSize);
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
  
  void EditorUI::createUIRange(const Coordinate& from, const Coordinate& to, Coordinate& lineStart, Coordinate& lineEnd, float& start, float& end, int lineNo) {
    if (from <= lineEnd) {
      start = from > lineStart
        ? textDistanceToLineStart(from)
        : 0.0f;
    }
    if (to > lineStart) {
      end =
        textDistanceToLineStart(to < lineEnd
                                ? to
                                : lineEnd);
    }
    
    if (to.line > lineNo) {
      end += charAdvance.x;
    }
  }
  
  void inline EditorUI::deleteLine(int start, int end) {
    PROFILE_START;
    lines.erase(lines.begin() + start, lines.begin() + end);
    textChanged = true;
  }
  
  void inline EditorUI::deleteLine(int index) {
    lines.erase(lines.begin() + index);
    textChanged = true;
  }
  
  void EditorUI::deleteRange(const Coordinate& from, const Coordinate& to) {
    PROFILE_START;
    if (readOnly) {
      return;
    }
    
    if (from == to) {
      return;
    }
    
    auto start = getCharacterIndex(from);
    auto end = getCharacterIndex(to);
    
    if (from.line == to.line) {
      auto& line = lines[from.line];
      auto n = getLineMaxColumn(from.line);
      if (to.column >= n) {
        line.erase(line.begin() + start, line.end());
      } else {
        line.erase(line.begin() + start, line.begin() + end);
      }
    } else {
      auto& firstLine = lines[from.line];
      auto& lastLine = lines[to.line];
      
      firstLine.erase(firstLine.begin() + start, firstLine.end());
      lastLine.erase(lastLine.begin(), lastLine.begin() + end);
      
      if (from.line < to.line) {
        firstLine.insert(firstLine.end(), lastLine.begin(), lastLine.end());
        deleteLine(from.line + 1, to.line + 1);
      }
    }
    textChanged = true;
  }
  
  std::string EditorUI::getText(const Coordinate& from, const Coordinate& to) const {
    PROFILE_START;
    std::string result;
    
    int lStart = from.line;
    int lEnd = to.line;
    int cIndexStart = getCharacterIndex(from);
    int cIndexEnd = getCharacterIndex(to);
    
    size_t s = 0;
    
    for (size_t i = lStart; i < lEnd; i++) {
      s += lines[i].size();
    }
    
    result.reserve(s + s / 8);
    
    while (cIndexStart < cIndexEnd || lStart < lEnd) {
      if(lStart >= (int)lines.size()) {
        break;
      }
      
      auto& line = lines[lStart];
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
    lines.clear();
    lines.emplace_back(Line());
    for (auto character : text) {
      if (character == '\r') {
      } else if (character == '\n') {
        lines.emplace_back(Line());
      } else {
        lines.back().emplace_back(Glypth(character));
      }
    }
  }
  
  EditorUI::Coordinate EditorUI::sanitizeCoordinates(const Coordinate& value) const {
    PROFILE_START;
    int line = value.line;
    int column = value.column;
    if (line >= (int)lines.size()) {
      if (lines.empty()) {
        line = 0;
        column = 0;
      } else {
        line = (int)lines.size() - 1;
        column = getLineMaxColumn(line);
      }
      return {line, column};
    }
    column = lines.empty() ? 0 : std::min(column, getLineMaxColumn(line));
    return {line, column};
  }
  
  EditorUI::Coordinate EditorUI::screenPosToCoordinates(const ImVec2& position) const {
    PROFILE_START;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 local(position.x - origin.x, position.y - origin.y);
    
    int lineNo = std::max(0, (int)floor(local.y / charAdvance.y));
    
    int columnCoord = 0;
    
    if (lineNo >= 0 && lineNo < (int)lines.size()) {
      auto& line = lines.at(lineNo);
      
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
                                                (float(tabSize) * spaceSize))) *
            (float(tabSize) * spaceSize);
          columnWidth = newColumnX - oldX;
          if (textStartPixel + columnX + columnWidth * 0.5f > local.x) break;
          columnX = newColumnX;
          columnCoord = (columnCoord / tabSize) * tabSize + tabSize;
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
          if (textStartPixel + columnX + columnWidth * 0.5f > local.x) break;
          columnX += columnWidth;
          columnCoord++;
        }
      }
    }
    
    return sanitizeCoordinates(Coordinate(lineNo, columnCoord));
  }
  
  EditorUI::Coordinate EditorUI::findWordEnd(const Coordinate& from) const {
    PROFILE_START;
    if (from.line >= (int)lines.size()) {
      return from;
    }
    
    auto& line = lines[from.line];
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
    
    return {from.line, getCharacterColumn(from.line, cindex)};
  }
  
  EditorUI::Coordinate EditorUI::findWordStart(const Coordinate& from) const {
    PROFILE_START;
    if (from.line >= (int)lines.size()) {
      return from;
    }
    
    auto& line = lines[from.line];
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
      return {from.line, getCharacterColumn(from.line, cindex)};
    }
    
    moved = false;
    
    while(cindex > 0 && !isANWord(line[cindex].m_char)) {
      cindex--;
      moved = true;
    }
    
    if(moved) {
      return {from.line, getCharacterColumn(from.line, cindex + 1)};
    }
    
    while (true) {
      if(cindex <= 0) {
        return {from.line, getCharacterColumn(from.line, cindex)};
      }
      
      auto c = line[cindex].m_char;
      
      if(isANWord(c)) {
        cindex--;
      } else {
        break;
      }
    }
    
    return {from.line, getCharacterColumn(from.line, cindex)};
  }
  
  bool EditorUI::isOnWordBoundary(const Coordinate& at) const {
    PROFILE_START;
    if (at.line >= (int)lines.size() || at.column == 0) {
      return true;
    }
    
    auto& line = lines[at.line];
    auto cindex = getCharacterIndex(at);
    
    if (cindex >= (int)line.size()) {
      return true;
    }
    
    return isspace(line[cindex].m_char) != isspace(line[cindex - 1].m_char);
  }
  
  void EditorUI::setSelection(const Coordinate& start, const Coordinate& end,
                              SelectionMode mode) {
    PROFILE_START;
    auto oldSelStart = editorState.selectionStart;
    auto oldSelEnd = editorState.selectionEnd;
    
    editorState.selectionStart = sanitizeCoordinates(start);
    editorState.selectionEnd = sanitizeCoordinates(end);
    if (editorState.selectionStart > editorState.selectionEnd) {
      std::swap(editorState.selectionStart, editorState.selectionEnd);
    }
    
    switch (mode) {
      case SelectionMode::Normal:
      break;
      case SelectionMode::Word: {
        editorState.selectionStart =
          findWordStart(editorState.selectionStart);
        
        if (!isOnWordBoundary(editorState.selectionEnd)) {
          editorState.selectionEnd =
            findWordEnd(findWordStart(editorState.selectionEnd));
        }
        break;
      }
      case SelectionMode::Line: {
        const auto lineNo = editorState.selectionEnd.line;
        const auto lineSize =
          (size_t)lineNo < lines.size() ? lines[lineNo].size() : 0;
        editorState.selectionStart =
          Coordinate(editorState.selectionStart.line, 0);
        editorState.selectionEnd =
          Coordinate(lineNo, getLineMaxColumn(lineNo));
        break;
      }
      default:
      break;
    }
    
    if (editorState.selectionStart != oldSelStart ||
        editorState.selectionEnd != oldSelEnd) {
      cursoPositionChanged = true;
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
          (lastClick != -1.0f && (t - lastClick) < io.MouseDoubleClickTime);
        
        if (tripleClick) {
          if (!ctrl) {
            editorState.cursorPosition = interactiveEnd =
              interactiveStart = screenPosToCoordinates(ImGui::GetMousePos());
            selectionMode = SelectionMode::Line;
            setSelection(interactiveStart, interactiveEnd, selectionMode);
          }
          
          lastClick = -1.0f;
        } else if (doubleClick) {
          if (!ctrl) {
            editorState.cursorPosition = interactiveEnd =
              interactiveStart = screenPosToCoordinates(ImGui::GetMousePos());
            if (selectionMode == SelectionMode::Line) {
              selectionMode = SelectionMode::Normal;
            } else {
              selectionMode = SelectionMode::Word;
            }
            setSelection(interactiveStart, interactiveEnd, selectionMode);
          }
          
          lastClick = (float)ImGui::GetTime();
        } else if (click) {
          editorState.cursorPosition = interactiveEnd = interactiveStart =
            screenPosToCoordinates(ImGui::GetMousePos());
          if (ctrl) {
            selectionMode = SelectionMode::Word;
          } else {
            selectionMode = SelectionMode::Normal;
          }
          setSelection(interactiveStart, interactiveEnd, selectionMode);
          
          lastClick = (float)ImGui::GetTime();
        } else if (ImGui::IsMouseDragging(0) && ImGui::IsMouseDown(0)) {
          io.WantCaptureMouse = true;
          editorState.cursorPosition = interactiveEnd =
            screenPosToCoordinates(ImGui::GetMousePos());
          setSelection(interactiveStart, interactiveEnd, selectionMode);
        }
      }
    }
  }
  
  EditorUI::Coordinate EditorUI::getActualCursorCoordinates() const {
    PROFILE_START;
    return sanitizeCoordinates(editorState.cursorPosition);
  }
  
  void EditorUI::ensureCursorVisible() {
    PROFILE_START;
    float scrollX = ImGui::GetScrollX();
    float scrollY = ImGui::GetScrollY();
    
    auto height = ImGui::GetWindowHeight();
    auto width = ImGui::GetWindowWidth();
    
    auto top = 1 + (int)ceil(scrollY / charAdvance.y);
    auto bottom = (int)ceil((scrollY + height) / charAdvance.y);
    
    auto left = (int)ceil(scrollX / charAdvance.x);
    auto right = (int)ceil((scrollX + width) / charAdvance.x);
    
    auto pos = getActualCursorCoordinates();
    auto len = textDistanceToLineStart(pos);
    
    if (pos.line < top) {
      ImGui::SetScrollY(std::max(0.0f, (pos.line - 1) * charAdvance.y));
    }
    if (pos.line > bottom - 4) {
      ImGui::SetScrollY(
                        std::max(0.0f, (pos.line + 4) * charAdvance.y - height));
    }
    if (len + textStartPixel < left + 4) {
      ImGui::SetScrollX(std::max(0.0f, len + textStartPixel - 4));
    }
    if (len + textStartPixel > right - 4) {
      ImGui::SetScrollX(std::max(0.0f, len + textStartPixel + 4 - width));
    }
  }
  
  void EditorUI::setCursorPosition(const Coordinate& pos) {
    PROFILE_START;
    if (editorState.cursorPosition == pos) {
      return;
    }
    
    editorState.cursorPosition = pos;
    cursoPositionChanged = true;
    ensureCursorVisible();
  }
  
  int EditorUI::getCharacterColumn(int lineIndex, int index) const {
    PROFILE_START;
    if (lineIndex >= lines.size()) {
      return 0;
    }
    auto& line = lines[lineIndex];
    int col = 0;
    int i = 0;
    while (i < index && i < (int)line.size()) {
      auto c = line[i].m_char;
      i += UTF8CharLength(c);
      if (c == '\t')
        col = (col / tabSize) * tabSize + tabSize;
      else
        col++;
    }
    return col;
  }
  
  void EditorUI::moveUp(int amount, bool shift) {
    PROFILE_START;
    auto oldPos = editorState.cursorPosition;
    editorState.cursorPosition.line =
      std::max(0, editorState.cursorPosition.line - amount);
    
    if (oldPos != editorState.cursorPosition) {
      if (shift) {
        if (oldPos == interactiveStart) {
          interactiveStart = editorState.cursorPosition;
        } else if (oldPos == interactiveEnd) {
          interactiveEnd = editorState.cursorPosition;
        } else {
          interactiveStart = editorState.cursorPosition;
          interactiveEnd = oldPos;
        }
      } else {
        interactiveStart = interactiveEnd = editorState.cursorPosition;
      }
      setSelection(interactiveStart, interactiveEnd, SelectionMode::Normal);
      ensureCursorVisible();
    }
  }
  
  void EditorUI::moveDown(int amount, bool shift) {
    PROFILE_START;
    auto oldPos = editorState.cursorPosition;
    editorState.cursorPosition.line =
      std::max(0, std::min((int)lines.size() - 1,
                           editorState.cursorPosition.line + amount));
    
    if (editorState.cursorPosition != oldPos) {
      if (shift) {
        if (oldPos == interactiveEnd) {
          interactiveEnd = editorState.cursorPosition;
        } else if (oldPos == interactiveStart) {
          interactiveStart = editorState.cursorPosition;
        } else {
          interactiveStart = oldPos;
          interactiveEnd = editorState.cursorPosition;
        }
      } else {
        interactiveStart = interactiveEnd = editorState.cursorPosition;
      }
      setSelection(interactiveStart, interactiveEnd, SelectionMode::Normal);
      ensureCursorVisible();
    }
  }
  
  void EditorUI::moveLeft(int amount, bool shift, bool ctrl) {
    PROFILE_START;
    if (lines.empty()) {
      return;
    }
    
    auto oldPos = editorState.cursorPosition;
    editorState.cursorPosition = getActualCursorCoordinates();
    
    auto line = editorState.cursorPosition.line;
    auto cindex = getCharacterIndex(editorState.cursorPosition);
    
    while (amount-- > 0) {
      if (cindex == 0) {
        if (line > 0) {
          --line;
          if (lines.size() > line) {
            cindex = (int)lines[line].size();
          } else {
            cindex = 0;
          }
        }
      } else {
        --cindex;
        if (cindex > 0) {
          if ((int)lines.size() > line) {
            while (cindex > 0 && isUTF8Sequence(lines[line][cindex].m_char)) {
              --cindex;
            }
          }
        }
      }
      
      editorState.cursorPosition =
        Coordinate(line, getCharacterColumn(line, cindex));
      
      if (ctrl) {
        editorState.cursorPosition =
          findWordStart(editorState.cursorPosition);
        cindex = getCharacterIndex(editorState.cursorPosition);
      }
    }
    
    editorState.cursorPosition =
      Coordinate(line, getCharacterColumn(line, cindex));
    
    if (shift) {
      if (oldPos == interactiveStart) {
        interactiveStart = editorState.cursorPosition;
      } else if (oldPos == interactiveEnd) {
        interactiveEnd = editorState.cursorPosition;
      } else {
        interactiveStart = editorState.cursorPosition;
        interactiveEnd = oldPos;
      }
    } else {
      interactiveStart = interactiveEnd = editorState.cursorPosition;
    }
    
    setSelection(interactiveStart, interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  EditorUI::Coordinate EditorUI::findNextWord(const EditorUI::Coordinate& from) const {
    PROFILE_START;
    Coordinate at = from;
    
    if (at.line >= (int)lines.size()) {
      return at;
    }
    
    auto cindex = getCharacterIndex(at);
    auto& line = lines[at.line];
    bool moved = false;
    
    while (cindex < (int)line.size() && isspace(line[cindex].m_char)) {
      cindex++;
      moved = true;
    }
    
    if (moved) {
      return {at.line, getCharacterColumn(at.line, cindex)};
    }
    
    moved = false;
    
    while (cindex < (int)line.size() && !isANWord(line[cindex].m_char)) {
      cindex++;
      moved = true;
    }
    
    if (moved) {
      return {at.line, getCharacterColumn(at.line, cindex)};
    }
    
    while (true) {
      if (cindex >= line.size()) {
        return {at.line, getCharacterColumn(at.line, cindex)};
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
    
    return {at.line, getCharacterColumn(at.line, cindex)};
  }
  
  void EditorUI::moveRight(int amount, bool shift, bool ctrl) {
    PROFILE_START;
    auto oldPos = editorState.cursorPosition;
    
    if (lines.empty() || oldPos.line >= lines.size()) {
      return;
    }
    
    auto cindex = getCharacterIndex(oldPos);
    
    while (amount-- > 0) {
      auto lineIndex = editorState.cursorPosition.line;
      auto& line = lines[lineIndex];
      
      if (cindex >= line.size()) {
        if (editorState.cursorPosition.line < lines.size() - 1) {
          editorState.cursorPosition.line =
            std::max(0, std::min((int)lines.size() - 1,
                                 editorState.cursorPosition.line + 1));
          editorState.cursorPosition.column = 0;
        } else {
          return;
        }
      } else {
        if (ctrl) {
          editorState.cursorPosition =
            findNextWord(editorState.cursorPosition);
        } else {
          cindex += UTF8CharLength(line[cindex].m_char);
          editorState.cursorPosition =
            Coordinate(lineIndex, getCharacterColumn(lineIndex, cindex));
        }
      }
    }
    
    if (shift) {
      if (oldPos == interactiveStart) {
        interactiveStart = editorState.cursorPosition;
      } else if (oldPos == interactiveEnd) {
        interactiveEnd = sanitizeCoordinates(editorState.cursorPosition);
      } else {
        interactiveStart = oldPos;
        interactiveEnd = editorState.cursorPosition;
      }
    } else {
      interactiveStart = interactiveEnd = editorState.cursorPosition;
    }
    
    setSelection(interactiveStart, interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::moveEnd(bool shift) {
    PROFILE_START;
    if (lines.empty()) {
      return;
    }
    
    int lineNo = editorState.cursorPosition.line;
    auto& line = lines[lineNo];
    
    if(line.empty()) {
      return;
    }
    
    auto old = editorState.cursorPosition;
    
    editorState.cursorPosition = Coordinate(lineNo, getLineMaxColumn(lineNo));
    
    if(shift) {
      if(hasSelection()) {
        interactiveEnd = editorState.cursorPosition;
        interactiveStart = editorState.selectionStart;
      } else {
        interactiveStart = old;
        interactiveEnd = editorState.cursorPosition;
      }
    } else {
      interactiveStart = interactiveEnd = editorState.cursorPosition;
    }
    
    setSelection(interactiveStart, interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::moveHome(bool shift) {
    PROFILE_START;
    if (lines.empty()) {
      return;
    }
    
    int lineNo = editorState.cursorPosition.line;
    
    int oldCindex = getCharacterIndex(editorState.cursorPosition);
    
    auto& line = lines[lineNo];
    
    if(line.empty()) {
      return;
    }
    auto old = editorState.cursorPosition;
    
    Coordinate beginning = Coordinate(lineNo, 0);
    beginning = findNextWord(beginning);
    editorState.cursorPosition = beginning;
    
    if(shift) {
      if(hasSelection()) {
        interactiveStart = editorState.cursorPosition;
        interactiveEnd = editorState.selectionEnd;
      } else {
        interactiveStart = editorState.cursorPosition;
        interactiveEnd = old;
      }
    } else {
      interactiveStart = interactiveEnd = editorState.cursorPosition; 
    }
    
    setSelection(interactiveStart, interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::moveTop(bool shift) {
    PROFILE_START;
    if (lines.empty()) {
      return;
    }
    
    auto old = editorState.cursorPosition;
    editorState.cursorPosition = Coordinate(0, 0);
    
    if(shift) {
      if (hasSelection()) {
        interactiveEnd = editorState.selectionStart;
      } else {
        interactiveEnd = old;
      }
      interactiveStart = editorState.cursorPosition;
    }
    
    setSelection(interactiveStart, interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::moveBottom(bool shift) {
    PROFILE_START;
    if (lines.empty()) {
      return;
    }
    
    auto old = editorState.cursorPosition;
    editorState.cursorPosition = Coordinate((int)lines.size() - 1, 0);
    
    if(shift) {
      if (hasSelection()) {
        interactiveStart = editorState.selectionStart;
      } else {
        interactiveStart = old;
      }
      interactiveEnd = editorState.cursorPosition;
    }
    
    setSelection(interactiveStart, interactiveEnd, SelectionMode::Normal);
    ensureCursorVisible();
  }
  
  void EditorUI::deleteSelection() {
    PROFILE_START;
    if (!hasSelection()) {
      return;
    }
    
    if (editorState.selectionStart == editorState.selectionEnd) {
      return;
    }
    
    deleteRange(editorState.selectionStart, editorState.selectionEnd);
    setCursorPosition(editorState.selectionStart);
  }
  
  void EditorUI::copy() {
    if(hasSelection()) {
      ImGui::SetClipboardText(getSelectedText().c_str());
    } else if(!lines.empty()) {
      std::string result;
      auto& line = lines[getActualCursorCoordinates().line];
      for (auto &g : line) {
        result.push_back(g.m_char);
      }
      ImGui::SetClipboardText(result.c_str());
    }
  }
  
  inline EditorUI::Line &EditorUI::insertLine(int index) {
    return *lines.insert(lines.begin() + index, Line());
  }
  
  int EditorUI::insertTextAt(Coordinate &pos, const char *value) {
    PROFILE_START;
    int cindex = getCharacterIndex(pos);
    int totalLines = 0;
    while (*value != '\0') {
      if (*value == '\r') {
        ++value;
      } else if (*value == '\n') {
        if (cindex < (int)lines[pos.line].size()) {
          auto& newLine = insertLine(pos.line + 1);
          auto& line = lines[pos.line];
          newLine.insert(newLine.begin(), line.end() + cindex, line.end());
          line.erase(line.begin() + cindex, line.end());
        } else {
          insertLine(pos.line + 1);
        }
        pos.line++;
        pos.column = 0;
        ++totalLines;
        ++value;
      } else {
        auto& line = lines[pos.line];
        auto d = UTF8CharLength(*value);
        while (d-- > 0 && *value != '\0') {
          line.insert(line.begin() + cindex++, Glypth(*value++));
        }
        pos.column++;
      }
      textChanged = true;
    }
    return totalLines;
  }
  
  void EditorUI::insertText(const char *value) {
    if (!value) {
      return;
    }
    
    auto pos = getActualCursorCoordinates();
    auto start = std::min(pos, editorState.selectionStart);
    int totalLines = pos.line - start.line;
    
    totalLines += insertTextAt(pos, value);
    
    setSelection(pos, pos, SelectionMode::Normal);
    setCursorPosition(pos);
  }
  
  void EditorUI::insertText(const std::string& value) {
    insertText(value.c_str());
  }
  
  void EditorUI::paste() {
    if (readOnly) {
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
      int lineNo = editorState.cursorPosition.line;
      bool lastLine = lineNo == (int)lines.size() - 1;
      deleteLine(lineNo, lineNo + 1);
      if (lastLine) {
        insertLine(lineNo);
        editorState.cursorPosition = Coordinate(lineNo, 0);
      }
    }
  }
  
  void EditorUI::selectAll() {
    PROFILE_START;
    if (lines.size() == 0) {
      return;
    }
    
    interactiveStart = Coordinate(0, 0);
    int lastLineNo = lines.size() - 1;
    Coordinate end = Coordinate(lastLineNo, getLineMaxColumn(lastLineNo));
    interactiveEnd = editorState.cursorPosition = end;
    
    setSelection(interactiveStart, interactiveEnd, SelectionMode::Normal);
  }
  
  void EditorUI::insertCharacter(ImWchar c, bool shift) {
    PROFILE_START;
    if (hasSelection()) {
      if (c == '\t' && editorState.selectionStart.line == editorState.selectionEnd.line) {
        auto start = editorState.selectionStart;
        auto end = editorState.selectionEnd;
        auto originalEnd = end;
        
        if (start > end) {
          std::swap(start, end);
        }
        
        start.column = 0;
        if (end.column == 0 && end.line > 0) {
          --end.line;
        }
        if (end.line >= (int)lines.size()) {
          end.line = lines.empty() ? 0 : (int)lines.size() - 1;
        }
        end.column = getLineMaxColumn(end.line);
        
        bool modified = false;
        
        for (int i = start.line; i <= end.line; i++) {
          auto& line = lines[i];
          if (shift) {
            if (!line.empty()) {
              if (line.front().m_char == '\t') {
                line.erase(line.begin());
                modified = true;
              } else {
                for (int j = 0; j < tabSize && !line.empty() && line.front().m_char == ' '; j++) {
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
          start = Coordinate(start.line, getCharacterColumn(start.line, 0));
          Coordinate rangeEnd;
          if (originalEnd.column != 0) {
            end = Coordinate(end.line, getLineMaxColumn(end.line));
            rangeEnd = end;
          } else {
            end = Coordinate(originalEnd.line, 0);
            rangeEnd = Coordinate(end.line - 1, getLineMaxColumn(end.line - 1));
          }
          textChanged = true;
          ensureCursorVisible();
        }
        return;
      } else {
        deleteSelection();
      }
    }
    
    auto coord = getActualCursorCoordinates();
    
    if (c == '\n') {
      insertLine(coord.line + 1);
      auto &line = lines[coord.line];
      auto &newLine = lines[coord.line + 1];
      
      // TODO(Maxlisui): Auto Indentation
      
      const size_t whiteSpaceSize = newLine.size();
      auto cindex = getCharacterIndex(coord);
      newLine.insert(newLine.begin(), line.begin() + cindex, line.end());
      line.erase(line.begin() + cindex, line.begin() + line.size());
      setCursorPosition(Coordinate(coord.line + 1, getCharacterColumn(coord.line + 1, (int)whiteSpaceSize)));
    } else {
      char buf[7];
      int e = ImTextCharToUtf8(buf, 7, c);
      
      if (e <= 0) {
        return;
      }
      
      buf[e] = '\0';
      auto &line = lines[coord.line];
      auto cindex = getCharacterIndex(coord);
      
      if (override && cindex < (int)line.size()) {
        auto d = UTF8CharLength(line[cindex].m_char);
        
        while (d-- > 0 && cindex < (int)line.size()) {
          line.erase(line.begin() + cindex);
        }
      }
      
      for (auto p = buf; *p != '\0'; p++, cindex++) {
        line.insert(line.begin() + cindex, Glypth(*p));
      }
      
      setCursorPosition(Coordinate(coord.line, getCharacterColumn(coord.line, cindex)));
    }
    
    textChanged = true;
    ensureCursorVisible();
  }
  
  void EditorUI::backspace() {
    if (lines.empty()) {
      return;
    }
    
    if (hasSelection()) {
      deleteSelection();
    } else {
      auto pos = getActualCursorCoordinates();
      setCursorPosition(pos);
      
      if (editorState.cursorPosition.column == 0) {
        if (editorState.cursorPosition.line == 0) {
          return;
        }
        
        int lineNo = editorState.cursorPosition.line;
        auto& line = lines[lineNo];
        auto& prevLine = lines[lineNo];
        auto prevSize = getLineMaxColumn(lineNo - 1);
        
        prevLine.insert(prevLine.end(), line.begin(), line.end());
        
        deleteLine(lineNo);
        --editorState.cursorPosition.line;
        editorState.cursorPosition.column = prevSize;
      } else {
        auto& line = lines[editorState.cursorPosition.line];
        auto cindex = getCharacterIndex(pos) - 1;
        auto cend = cindex + 1;
        while (cindex > 0 && isUTF8Sequence(line[cindex].m_char)) {
          cindex--;
        }
        
        --editorState.cursorPosition.column;
        
        while (cindex < line.size() && cend-- > cindex) {
          line.erase(line.begin() + cindex);
        }
      }
      
      textChanged = true;
      ensureCursorVisible();
    }
  }
  
  void EditorUI::remove() {
    if (lines.empty()) {
      return;
    }
    
    if (hasSelection()) {
      deleteSelection();
    }
    
    auto pos = getActualCursorCoordinates();
    setCursorPosition(pos);
    auto& line = lines[pos.line];
    
    if (pos.column == getLineMaxColumn(pos.line)) {
      if (pos.line == (int)lines.size() - 1) {
        return;
      }
      
      auto& nextLine = lines[pos.line + 1];
      line.insert(line.end(), nextLine.begin(), nextLine.end());
      deleteLine(pos.line + 1);
    } else {
      auto cindex = getCharacterIndex(pos);
      auto d = UTF8CharLength(line[cindex].m_char);
      while (d-- > 0 && cindex < (int)line.size()) {
        line.erase(line.begin() + cindex);
      }
    }
    
    textChanged = true;
  }
  
  void EditorUI::handleEscape() {
    if (showSearchAndReplace) {
      showSearchAndReplace = false;
      return;
    }
    
    if (!searchResults.empty()) {
      searchResults.clear();
    }
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
      } else if (!readOnly && !ctrl && !shift && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
        remove();
      } else if (!readOnly && !ctrl && !shift && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Backspace))) {
        backspace();
      } else if (ctrl && !shift && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert))) {
        copy();
        
      } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_C))) {
        copy();
      } else if (!readOnly && !ctrl && shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Insert))) {
        paste();
      } else if (!readOnly && ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_V))) {
        paste();
      } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_X))) {
        cut();
      } else if (!ctrl && shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Delete))) {
        cut();
      } else if (ctrl && !shift && !alt && ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_A))) {
        selectAll();
      } else if (!readOnly && !ctrl && !shift && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Enter))) {
        insertCharacter('\n', false);
      } else if (!readOnly && !ctrl && !alt &&
                 ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Tab))) {
        insertCharacter('\n', shift);
      } else if (ctrl && io.KeysDown[70]) {
        showSearchAndReplace = true;
        searchAndReplace->show();
      } else if (ImGui::IsKeyPressed(ImGui::GetKeyIndex(ImGuiKey_Escape))) {
        handleEscape();
      }
      
      if (!readOnly && !io.InputQueueCharacters.empty()) {
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
    
    cursoPositionChanged = false;
    
    handleKeyboardInput();
    handleMouseInput();
    
    searchAndReplace->render(showSearchAndReplace);
    
    const float fontSize = ImGui::GetFont()
      ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX,
                      -1.f, "#", nullptr, nullptr)
      .x;
    
    charAdvance =
      ImVec2(fontSize, ImGui::GetTextLineHeightWithSpacing() * lineSpacing);
    
    auto contentSize = ImGui::GetWindowContentRegionMax();
    auto drawList = ImGui::GetWindowDrawList();
    
    ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
    auto scrollX = ImGui::GetScrollX();
    auto scrollY = ImGui::GetScrollY();
    float longest = textStartPixel;
    
    lineBuffer.clear();
    
    auto lineNo = (int)floor(scrollY / charAdvance.y);
    
    auto lineMax = std::max(
                            0, std::min(
                                        (int)lines.size(),
                                        lineNo + (int)floor((scrollY + contentSize.y) / charAdvance.y)));
    
    if (!lines.empty()) {
      float spaceSize = ImGui::GetFont()
        ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f,
                        " ", nullptr, nullptr)
        .x;
      
      char buf[16];
      
      while (lineNo < lineMax) {
        auto lineStartScreenPos = ImVec2(
                                         cursorScreenPos.x, cursorScreenPos.y + lineNo * charAdvance.y);
        auto textScreenPos =
          ImVec2(lineStartScreenPos.x + textStartPixel, lineStartScreenPos.y);
        
        auto& line = lines[lineNo];
        
        auto lineMaxColumn = getLineMaxColumn(lineNo);
        
        longest = std::max(textStartPixel + textDistanceToLineStart(Coordinate(
                                                                               lineNo, lineMaxColumn)),
                           longest);
        
        Coordinate lineStartCoord = Coordinate(lineNo, 0);
        Coordinate lineEndCoord = Coordinate(lineNo, getLineMaxColumn(lineNo));
        
        float sstart = -1.f;
        float ssend = -1.f;
        
        createUIRange(editorState.selectionStart, editorState.selectionEnd, lineStartCoord, lineEndCoord, sstart, ssend, lineNo);
        
        if (sstart != -1 && ssend != -1 && sstart < ssend) {
          ImVec2 vstart(lineStartScreenPos.x + textStartPixel + sstart,
                        lineStartScreenPos.y);
          ImVec2 vend(lineStartScreenPos.x + textStartPixel + ssend,
                      lineStartScreenPos.y + charAdvance.y);
          drawList->AddRectFilled(vstart, vend, 0x80a06020);
        }
        
        for(auto &result : searchResults) {
          float srStart = -1.f;
          float srEnd = -1.f;
          
          createUIRange(result.start, result.end, lineStartCoord, lineEndCoord, srStart, srEnd, lineNo);
          
          if (srStart != -1 && srEnd != -1 && srStart < srEnd) {
            ImVec2 vstart(lineStartScreenPos.x + textStartPixel + srStart,
                          lineStartScreenPos.y);
            ImVec2 vend(lineStartScreenPos.x + textStartPixel + srEnd,
                        lineStartScreenPos.y + charAdvance.y);
            drawList->AddRectFilled(vstart, vend, 0x80b5b5b5);
          }
        }
        
        auto start = ImVec2(lineStartScreenPos.x + scrollX, lineStartScreenPos.y);
        
        stbsp_snprintf(buf, 16, "%d ", lineNo + 1);
        
        auto lineNoWidth = ImGui::GetFont()
          ->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX,
                          -1.0f, buf, nullptr, nullptr)
          .x;
        drawList->AddText(
                          ImVec2(lineStartScreenPos.x + textStartPixel - lineNoWidth,
                                 lineStartScreenPos.y),
                          0xff707000, buf);
        
        if (editorState.cursorPosition.line == lineNo) {
          auto focused = ImGui::IsWindowFocused();
          
          if (!hasSelection()) {
            auto end = ImVec2(start.x + contentSize.x + scrollX,
                              start.y + charAdvance.y);
            drawList->AddRectFilled(start, end,
                                    focused ? 0x40000000 : 0x40808080);
            drawList->AddRect(start, end, 0x40a0a0a0, 1.0f);
          }
          
          if (focused) {
            auto timeEnd =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                                                                    std::chrono::system_clock::now().time_since_epoch())
              .count();
            auto elapsed = timeEnd - startTime;
            
            if (elapsed > 400) {
              float width = 1.0f;
              auto cindex = getCharacterIndex(editorState.cursorPosition);
              float cx = textDistanceToLineStart(editorState.cursorPosition);
              
              if (override && cindex < (int)line.size()) {
                auto c = line[cindex].m_char;
                if (c == '\t') {
                  auto x = (1.0f + std::floor((1.0f + cx) /
                                              (float(tabSize) * spaceSize))) *
                    (float(tabSize) * spaceSize);
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
                          lineStartScreenPos.y + charAdvance.y);
              drawList->AddRectFilled(cstart, cend, 0xffe0e0e0);
              
              if (elapsed > 800) {
                startTime = timeEnd;
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
            lineBuffer.push_back(line[i++].m_char);
          }
        }
        
        if (!lineBuffer.empty()) {
          const ImVec2 newOffset(textScreenPos.x + bufferOffset.x,
                                 textScreenPos.y + bufferOffset.y);
          drawList->AddText(newOffset, 0xffffffff, lineBuffer.c_str());
          lineBuffer.clear();
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
                     editorState.cursorPosition.line,
                     editorState.cursorPosition.column);
      
      auto textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr);
      
      auto lineColPos =
        ImVec2(bottomMax.x - textSize.x - 5.f,
               bottomMin.y + ((bottomLineHeight - textSize.y) / 2));
      
      drawList->AddText(lineColPos, 0xFFFFFFFF, buf);
      
      if(hasSelection()) {
        char buf2[256];
        
        stbsp_snprintf(buf, 256, "Sel: %i %i - %i %i", editorState.selectionStart.line, 
                       editorState.selectionStart.column,
                       editorState.selectionEnd.line,
                       editorState.selectionEnd.column);
        
        textSize = ImGui::GetFont()->CalcTextSizeA(ImGui::GetFontSize(), FLT_MAX, -1.0f, buf, nullptr, nullptr);
        lineColPos = ImVec2(lineColPos.x - 5.f - textSize.x, lineColPos.y);
        drawList->AddText(lineColPos, 0xFFFFFFFF, buf);
      }
      
    }
    ImGui::Dummy(ImVec2((longest + 2),
                        (lines.size() * charAdvance.y) + bottomLineHeight));
  }
  
  void EditorUI::setFindResult(const string& str) {
    for (int i = 0; i < lines.size(); i++) {
      string currentLine;
      for (auto &glypth : lines[i]) {
        currentLine += glypth.m_char;
      }
      
      auto pos = currentLine.find(str, 0);
      while (pos != string::npos) {
        SelectionRange range;
        range.start = Coordinate(i, (int)pos);
        range.end = Coordinate(i, (int)(pos + str.size()));
        searchResults.emplace_back(range);
        
        pos = currentLine.find(str, pos + 1);
      }
    }
    
    if (!searchResults.empty()) {
      editorState.cursorPosition = searchResults[0].start;
      ensureCursorVisible();
    }
  }
  
  void EditorUI::findNext(const string& next) {
    if (lines.empty()) {
      return;
    }
    
    if (next != lastSearchString) {
      lastSearchString = next;
      searchResults.clear();
    }
    
    if (searchResults.empty()) {
      setFindResult(next);
    } else {
      currentSearchItem++;
      if (currentSearchItem == (int)searchResults.size()) {
        currentSearchItem = 0;
      }
      
      editorState.cursorPosition = searchResults[currentSearchItem].start;
      ensureCursorVisible();
    }
  }
  
  void EditorUI::findPrev(const string& prev) {
    if (lines.empty()) {
      return;
    }
    
    if (prev != lastSearchString) {
      lastSearchString = prev;
      searchResults.clear();
    }
    
    if (searchResults.empty()) {
      setFindResult(prev);
    } else {
      currentSearchItem--;
      if (currentSearchItem < 0) {
        currentSearchItem = (int)searchResults.size() - 1;
      }
      
      editorState.cursorPosition = searchResults[currentSearchItem].start;
      ensureCursorVisible();
    }
  }
  
  void EditorUI::replaceAll(const string& searchText, const string& replaceText) {
    if (lines.empty()) {
      return;
    }
    
    if (searchText != lastSearchString) {
      searchResults.clear();
    }
    
    if (searchResults.empty()) {
      setFindResult(searchText);
    }
    
    if (searchResults.empty()) {
      return;
    }
    
    auto cursorBefore = editorState.cursorPosition;
    
    for(auto &result : searchResults) {
      deleteRange(result.start, result.end);
      setCursorPosition(result.start);
      insertText(replaceText);
    }
    
    editorState.cursorPosition = cursorBefore;
    searchResults.clear();
  }
  
  void EditorUI::setSearchAndReplace(SearchAndReplaceUI *search) {
    searchAndReplace = search;
    search->editorMode = true;
    search->onFindNext = [this](const string& next) { this->findNext(next); };
    search->onFindPrev = [this](const string& prev) { this->findPrev(prev); };
    search->onReplaceAll = [this](const string& search, const string& replace) { this->replaceAll(search, replace); };
  }
}  // namespace UI
