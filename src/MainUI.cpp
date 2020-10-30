#include "MainUI.h"
#include "Tooling.h"
#include "../vendor/IconFontCppHeaders/IconsFontAwesome5.h"
#include "../vendor/imgui/imgui.h"
#include "EditorUI.h"
#include "WelcomeUI.h"
#include <fstream>
#include <mutex>
#include <thread>

namespace UI {
  
  struct EditorWrapper {
    EditorUI *editor;
    SearchAndReplaceUI *searchAndReplace;
    string name;
  };
  
  static Project::VSSolution *g_sln = nullptr;
  static bool g_renderWelcome = true;
  static bool g_renderSolutionExplorer = true;
  static std::vector<EditorWrapper *> g_editors;
  static std::mutex g_newEditorMutex;
  
  bool doRenderInSolutionExplorer(const std::string &name) {
    return !(name.ends_with("obj") || name.ends_with("proj") ||
             name == "Properties");
  }
  
  void renderSlnExplorerRecursive(const directory_entry &dir,
                                  bool propertiesRendered) {
    
    if (!propertiesRendered) {
      for (auto &entry : directory_iterator(dir)) {
        if (entry.is_directory() &&
            entry.path().filename().string() == "Properties") {
          propertiesRendered = true;
          if (ImGui::TreeNode(entry.path().filename().string().c_str())) {
            renderSlnExplorerRecursive(entry, propertiesRendered);
            ImGui::TreePop();
          }
          break;
        }
      }
    }
    
    for (auto &entry : directory_iterator(dir)) {
      if (entry.is_directory() &&
          doRenderInSolutionExplorer(entry.path().filename().string())) {
        if (ImGui::TreeNode(entry.path().filename().string().c_str())) {
          renderSlnExplorerRecursive(entry, propertiesRendered);
          ImGui::TreePop();
        }
      }
    }
    
    for (auto &entry : directory_iterator(dir)) {
      if (!entry.is_directory() &&
          doRenderInSolutionExplorer(entry.path().filename().string())) {
        ImGui::TreeNodeEx(entry.path().filename().string().c_str(),
                          ImGuiTreeNodeFlags_Leaf |
                          ImGuiTreeNodeFlags_NoTreePushOnOpen);
        if (ImGui::IsItemClicked()) {
          std::thread([entry]() {
                        std::ifstream ifs(entry.path().c_str(),
                                          std::ios::in | std::ios::binary | std::ios::ate);
                        std::ifstream::pos_type fileSize = ifs.tellg();
                        ifs.seekg(0, std::ios::beg);
                        
                        std::vector<char> bytes(fileSize);
                        ifs.read(bytes.data(), fileSize);
                        
                        ifs.close();
                        
                        EditorWrapper *wrapper = new EditorWrapper;
                        EditorUI *newEditor = new EditorUI;
                        SearchAndReplaceUI *searchAndReplace = new SearchAndReplaceUI;
                        wrapper->editor = newEditor;
                        wrapper->name = entry.path().filename().string();
                        wrapper->searchAndReplace = searchAndReplace;
                        
                        newEditor->setText(string(bytes.data(), fileSize));
                        newEditor->setSearchAndReplace(searchAndReplace);
                        
                        newEditor->onSave = [entry](const string& text) {
                          std::ofstream ofs(entry.path().c_str(), std::ios::trunc);
                          ofs << text;
                          ofs.close();
                        };
                        
                        g_newEditorMutex.lock();
                        g_editors.push_back(wrapper);
                        g_newEditorMutex.unlock();
                      }).detach();
        }
      }
    }
  }
  
  void renderSlnExplorerRecursive(Project::VSProject *project) {
    if (project->isFolder) {
      for (auto &p : project->childs) {
        if (ImGui::TreeNode(p->name.c_str())) {
          renderSlnExplorerRecursive(p);
          ImGui::TreePop();
        }
      }
    } else {
      renderSlnExplorerRecursive(project->directory, false);
    }
  }
  
  void renderSlnExplorer() {
    PROFILE_START;
    ImGui::SetNextWindowSize(ImVec2(0.f, 0.f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Solution Explorer", &g_renderSolutionExplorer);
    
    if (ImGui::TreeNode(g_sln->name.c_str())) {
      for (auto &project : g_sln->projects) {
        if (ImGui::TreeNode(project->name.c_str())) {
          renderSlnExplorerRecursive(project);
          ImGui::TreePop();
        }
      }
      ImGui::TreePop();
    }
    
    ImGui::End();
  }
  
  void renderMain() {
    PROFILE_START;
    
    ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetWorkPos());
    ImGui::SetNextWindowSize(viewport->GetWorkSize());
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiWindowFlags window_flags =
      ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
      ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
    ImGui::Begin("MAIN", nullptr, window_flags);
    
    ImGui::PopStyleVar(3);
    
    ImGuiID mainDockspace = ImGui::GetID("MAIN_DOCKSPACE");
    ImGui::DockSpace(mainDockspace, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    
    if (ImGui::BeginMenuBar()) {
      if (ImGui::BeginMenu("Folder")) {
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("View")) {
        ImGui::MenuItem("Solution Explorer", nullptr, &g_renderSolutionExplorer);
        ImGui::EndMenu();
      }
      ImGui::EndMenuBar();
    }
    
    ImGui::End();
    
    if (g_renderSolutionExplorer) {
      renderSlnExplorer();
    }
    
    
    
    if (g_newEditorMutex.try_lock()) {
      for (auto wrapper : g_editors) {
        ImGui::SetNextWindowDockID(mainDockspace, ImGuiCond_FirstUseEver);
        
        ImGui::PushStyleColor(ImGuiCol_ChildBg, 0xFF1E1E1E);
        
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_HorizontalScrollbar |
          ImGuiWindowFlags_AlwaysHorizontalScrollbar;
        
        if (wrapper->editor->textChanged) {
          windowFlags |= ImGuiWindowFlags_UnsavedDocument;
        }
        
        ImGui::Begin(wrapper->name.c_str(), nullptr,
                     windowFlags);
        ImGui::PushAllowKeyboardFocus(true);
        
        wrapper->editor->render();
        
        ImGui::PopStyleColor();
        ImGui::PopAllowKeyboardFocus();
        
        ImGui::End();
      }
      g_newEditorMutex.unlock();
    }
  }
  
  void MainUI::setup(Project::VSSolution *sln) {
    if (sln) {
      g_sln = sln;
      g_renderWelcome = false;
      
      EditorWrapper *wrapper = new EditorWrapper;
      EditorUI *editor = new EditorUI;
      wrapper->editor = editor;
      wrapper->name = "TestMax";
      wrapper->searchAndReplace = new SearchAndReplaceUI;
      wrapper->editor->setSearchAndReplace(wrapper->searchAndReplace);
      wrapper->editor->setText(
                               "  public void test(string test)\n  {\n    this.exist += "
                               "17;\n    _local.test "
                               "<= 17;\n  }\n\n  //Hello thios is a comment\n  while (!isWord || "
                               "skip) "
                               "{\n    if (at.m_line >= m_lines.size()) {\n      auto l = std::max(0, "
                               "(int)m_lines.size() - 1);\n      return {l, "
                               "getLineMaxColumn(l)};\n    }\n  }");
      g_newEditorMutex.lock();
      g_editors.push_back(wrapper);
      g_newEditorMutex.unlock();
    }
  }
  
  void MainUI::render() {
    PROFILE_START;
    if (g_renderWelcome) {
      g_sln = UI::WelcomeUI::tryGetSln();
      if (g_sln) {
        g_renderWelcome = false;
        return;
      }
      UI::WelcomeUI::render();
    } else {
      renderMain();
    }
  }
  
  void MainUI::shutdown() {
    g_newEditorMutex.lock();
    for (auto wrapper : g_editors) {
      delete wrapper->editor;
      delete wrapper->searchAndReplace;
      delete wrapper;
    }
    g_newEditorMutex.unlock();
  }
  
}
