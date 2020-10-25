#pragma once

#include "Constants.h"

namespace Project {

class VSProject {
public:
  const string &name() const { return m_name; }
  void setName(const string &name) { m_name = name; }
  const string &path() const { return m_path; }
  void setPath(const string &path) { m_path = path; }
  const string &id() const { return m_id; }
  void setId(const string &id) { m_id = id; }
  const string &typeId() const { return m_typeId; }
  void setTypeId(const string &typeId) { m_typeId = typeId; }
  void setIsFolder(bool folder) { m_isFoler = folder; }
  const bool isFolder() { return m_isFoler; }
  const directory_entry &directory() { return m_directory; }
  void setDirectory(const directory_entry &entry) { m_directory = entry; }

  std::vector<Project::VSProject *> &childs() { return m_childs; }
  std::vector<std::string> &references() { return m_references; }
  std::vector<std::string> &compiles() { return m_compiles; }
  std::vector<std::string> &nones() { return m_nones; }
  std::vector<std::string> &contents() { return m_contents; }
  std::vector<Project::VSProject *> &projectReferences() {
    return m_projectReferences;
  }
  std::vector<string> &pages() { return m_pages; }
  std::vector<string> &packageReferences() { return m_packageReferences; }

private:
  string m_name;
  string m_path;
  string m_id;
  string m_typeId;
  bool m_isFoler;
  std::vector<Project::VSProject *> m_childs = {};
  std::vector<std::string> m_references = {};
  std::vector<std::string> m_compiles = {};
  std::vector<std::string> m_nones = {};
  std::vector<std::string> m_contents = {};
  std::vector<Project::VSProject *> m_projectReferences = {};
  std::vector<std::string> m_pages = {};
  std::vector<std::string> m_packageReferences = {};
  directory_entry m_directory = {};
};

class VSSolution {
public:
  const string &name() const { return m_name; }
  void setName(const string &name) { m_name = name; }
  const string &path() const { return m_path; }
  void setPath(const string &path) { m_path = path; }
  std::vector<Project::VSProject *> &projects() { return m_projects; }

private:
  string m_name = {};
  string m_path = {};
  std::vector<Project::VSProject *> m_projects = {};
};

}; // namespace Project
