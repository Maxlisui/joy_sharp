#include "WelcomeUI.h"
#include "UIHelper.h"
#include "VSHelper.h"
#include "../vendor/ImGuiFileDialog/ImGuiFileDialog/ImGuiFileDialog.h"
#include "../vendor/imgui/imgui.h"
#include <mutex>
#include <windows.h>

namespace UI {

static Helper::ProgressHelper *g_slnLoadHelper = nullptr;
static HANDLE g_slnLoadHandle = INVALID_HANDLE_VALUE;
static DWORD g_slnLoadThreadId = -1;
static std::mutex g_slnLoadLock;
static std::mutex g_slnLock;
static Project::VSSolution *g_sln = nullptr;

DWORD
WINAPI loadSlnThread(LPVOID params) {
  auto slnPath = *(static_cast<std::string *>(params));

  g_slnLoadHelper = new Helper::ProgressHelper;

  auto localSln = Helper::VSHelper::parseVSsln(
      slnPath, [](float progess, const char *text) -> void {
        g_slnLoadLock.lock();
        g_slnLoadHelper->value = progess;
        g_slnLoadHelper->name = text;
        g_slnLoadLock.unlock();
      });

  delete g_slnLoadHelper;
  g_slnLoadHelper = nullptr;

  g_slnLock.lock();
  g_sln = localSln;
  g_slnLock.unlock();

  return 0;
}

Project::VSSolution *UI::WelcomeUI::tryGetSln() {
  Project::VSSolution *result = nullptr;
  if (g_slnLock.try_lock() && g_sln) {
    result = g_sln;
  }

  g_slnLock.unlock();

  return result;
}

void UI::WelcomeUI::render() {
  ImGuiIO &io = ImGui::GetIO();
  auto size = io.DisplaySize;

  static float oldProgres = 0.0f;
  static const char *oldText = (char *)"Loading";

  ImVec2 center(size.x / 2.f, size.y / 2.f);

  ImGui::SetNextWindowPos(center, 0, ImVec2(.5f, .5f));
  ImGui::SetNextWindowSize(ImVec2(0.0f, 0.0f));

  ImGui::Begin("Welcome Back!", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);

  if (g_slnLoadHandle == INVALID_HANDLE_VALUE) {
    ImGui::Text("Open an existing .sln file!");

    if (ImGui::Button("Choose .sln")) {
      igfd::ImGuiFileDialog::Instance()->OpenDialog("SlnFile", "Open Solution",
                                                    ".sln", ".");
    }

    if (igfd::ImGuiFileDialog::Instance()->FileDialog("SlnFile")) {
      if (igfd::ImGuiFileDialog::Instance()->IsOk) {
        static std::string filePathName =
            igfd::ImGuiFileDialog::Instance()->GetFilePathName();

        g_slnLoadHandle = CreateThread(nullptr, 0, loadSlnThread, &filePathName,
                                       0, &g_slnLoadThreadId);
      }
      igfd::ImGuiFileDialog::Instance()->CloseDialog("SlnFile");
    }
  } else {
    if (g_slnLoadHelper && g_slnLoadLock.try_lock()) {
      oldProgres = g_slnLoadHelper->value;
      oldText = g_slnLoadHelper->name.c_str();

      g_slnLoadLock.unlock();
    }

    ImGui::ProgressBar(oldProgres, ImVec2(-1.f, 0.0f), oldText);
  }

  ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
              1000.0f / io.Framerate, io.Framerate);

  ImGui::End();
}

} // namespace UI
