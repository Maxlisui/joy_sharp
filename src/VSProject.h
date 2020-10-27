#pragma once

#include "Constants.h"

namespace Project {

struct VSProject {
  string name;
  string path;
  string id;
  string typeId;
  bool isFolder;
  std::vector<Project::VSProject *> childs = {};
  std::vector<std::string> references = {};
  std::vector<std::string> compiles = {};
  std::vector<std::string> nones = {};
  std::vector<std::string> contents = {};
  std::vector<Project::VSProject *> projectReferences = {};
  std::vector<std::string> pages = {};
  std::vector<std::string> packageReferences = {};
  directory_entry directory = {};
};

struct VSSolution {
  string name = {};
  string path = {};
  std::vector<Project::VSProject *> projects = {};
};

}; // namespace Project
