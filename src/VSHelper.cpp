#include "VSHelper.h"
#include "Constants.h"
#include "Tooling.h"
#include <fstream>
#include <iostream>
#include <map>

namespace Helper {

struct ProjectWrapper {
  Project::VSProject *project = nullptr;
  bool touched = false;
};

#if DEBUG
void print(std::vector<Project::VSProject *> &projects, int intent) {
  for (auto &p : projects) {
    for (int i = 0; i < intent; i++) {
      std::cout << "\t";
    }

    std::cout << p->name << " is Folder: " << p->isFolder << std::endl;

    print(p->childs, intent + 1);
  }
}
#endif

void addToProject(const string &parentId, const string &childId,
                  std::map<std::string, ProjectWrapper *> &wrappers) {
  auto parent = wrappers[parentId];
  auto child = wrappers[childId];
  child->touched = true;
  parent->project->childs.emplace_back(child->project);
}

Project::VSProject *createProjectFromLine(std::string &line, path &slnPath) {
  auto project = new Project::VSProject;

  auto typeId = line.substr(10, 36);
  auto nameStart = 53;
  auto nameEnd = line.find_first_of("\",", nameStart);
  auto name = line.substr(nameStart, nameEnd - nameStart);
  auto pathStart = nameEnd + 4;
  auto pathEnd = line.find_first_of("\",", pathStart);
  auto partialPath = line.substr(pathStart, pathEnd - pathStart);
  auto path = slnPath.parent_path().append(partialPath);
  auto id = line.substr(pathEnd + 5, 36);

  project->id = id;
  project->name = name;
  project->path = path.string();
  project->directory = std::filesystem::directory_entry(path.parent_path());
  project->typeId = typeId;
  project->isFolder = line.find(Helper::Constants::VS_FOLDER_ID) !=
                       std::string::npos;

  return project;
}

void parseProject(Project::VSProject *project,
                  std::map<string, ProjectWrapper *> &wrappers) {
  std::string currentLine;
  bool inProjectReference = false;
  std::ifstream projectStream(project->path);
  if (projectStream.is_open()) {
    while (std::getline(projectStream, currentLine)) {

      if (inProjectReference) {
        inProjectReference = false;
        auto start = currentLine.find_first_of('{') + 1;
        auto end = currentLine.find_first_of('}', start);
        auto id = currentLine.substr(start, end - start);

        // TODO: Speed
        // TODO: Else -> Project no in current sln
        if (wrappers.contains(id)) {
          project->projectReferences.emplace_back(wrappers[id]->project);
        }
      } else if (currentLine.find("Include") != string::npos) {
        auto start = currentLine.find_first_of('<') + 1;
        auto end = currentLine.find_first_of(' ', start);
        auto tag = currentLine.substr(start, end - start);
        auto contentStart = currentLine.find_first_of('\"') + 1;
        auto contentEnd = currentLine.find_first_of('\"', contentStart);
        auto content =
            currentLine.substr(contentStart, contentEnd - contentStart);

        if (tag == "Reference") {
          project->references.push_back(content);
        } else if (tag == "PackageReference") {
          project->packageReferences.push_back(content);
        } else if (tag == "ProjectReference") {
          inProjectReference = true;
        }
      }
    }
    projectStream.close();
  }
}

Project::VSSolution *VSHelper::parseVSsln(
    const string &slnFilePath,
    const std::function<void(float, const char *)> &progessCallback) {
  PROFILE_START;
  if (progessCallback) {
    progessCallback(.0f, (char *)"Loading Solution");
  }
  auto sln = new Project::VSSolution;

  sln->path = slnFilePath;

  auto slnPath = path(slnFilePath);

  auto name = slnPath.stem().string();
  sln->name = name;

  std::string currentLine;
  std::ifstream slnStream(slnFilePath);
  std::map<std::string, ProjectWrapper *> wrappers;

  bool handleStructure = false;
  if (slnStream.is_open()) {
    while (std::getline(slnStream, currentLine)) {
      if (currentLine.starts_with("Project")) {
        auto wrapper = new ProjectWrapper;
        wrapper->project = createProjectFromLine(currentLine, slnPath);
        wrapper->touched = false;
        wrappers[wrapper->project->id] = wrapper;
      }

      if (currentLine.starts_with("\tGlobalSection(NestedProjects)")) {
        handleStructure = true;
        continue;
      }

      if (handleStructure) {
        if (currentLine.starts_with("\tEndGlobalSection")) {
          handleStructure = false;
          continue;
        }

        auto childId = currentLine.substr(3, 36);
        auto parentId = currentLine.substr(44, 36);
        addToProject(parentId, childId, wrappers);
      }
    }
    slnStream.close();
  }

  size_t counter = 0;
  auto size = wrappers.size();
  for (auto &wrapper : wrappers) {
    if (progessCallback) {
      auto text = "Loading Project " + wrapper.second->project->name;
      progessCallback((float)counter / size, text.c_str());
    }
    if (!wrapper.second->project->isFolder) {
      parseProject(wrapper.second->project, wrappers);
    }

    if (!wrapper.second->touched) {
      sln->projects.emplace_back(wrapper.second->project);
    }
    delete wrapper.second;
    counter++;
  }
  if (progessCallback) {
    progessCallback(1.f, (char *)"Loading Solution");
  }

#if DEBUG
  std::cout << "SOLUTION STRUCTURE" << std::endl;
  print(sln->projects, 0);
#endif

  return sln;
}
} // namespace Helper
