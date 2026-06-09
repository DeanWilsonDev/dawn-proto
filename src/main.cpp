#include "DawnLog.h"

#include "DawnTheme.h"
#include "EditorApplication.h"

#include <amanuensis.hpp>

#include <string>

namespace {

// Pulls the human-readable project name out of a parsed .dawn file. Defensive:
// a malformed project file should degrade to a sensible default, never crash.
std::string ReadProjectName(const Amanuensis::Value& Root) {
    if (!Root.IsObject() || !Root.Contains("project")) {
        LOG_WARNING("Project file has no 'project' object; using default name");
        return "Untitled";
    }
    const Amanuensis::Value& Project = Root.Get("project");
    if (!Project.IsObject() || !Project.Contains("name") || !Project.Get("name").IsString()) {
        LOG_WARNING("Project file has no 'project.name' string; using default name");
        return "Untitled";
    }
    return Project.Get("name").AsString();
}

} // namespace

int main(int argc, char** argv) {
    // Logging is the very first thing Dawn does — every subsequent line can log.
    Firefly::LogRegistry::RegisterLogger("Dawn", "dawn.log.csv", true);
    LOG_INFO("Dawn PoC starting");

    if (argc < 2) {
        LOG_ERROR("No project file given. Usage: dawn <path/to/project.dawn>");
        return 1;
    }

    const std::string ProjectPath = argv[1];
    LOG_INFO("Opening project file: {}", ProjectPath);

    const Amanuensis::ParseResult Parsed = Amanuensis::Reader::ParseFile(ProjectPath);
    if (!Parsed.succeeded) {
        LOG_ERROR("Failed to parse project file {} at {}:{} — {}", ProjectPath,
                  Parsed.error.line, Parsed.error.column, Parsed.error.message);
        return 1;
    }

    const std::string ProjectName = ReadProjectName(Parsed.value);
    LOG_INFO("Project loaded: '{}'", ProjectName);

    Dawn::Theme Theme;
    Dawn::EditorApplication App(Theme, ProjectName);
    return App.Run();
}
