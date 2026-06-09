#include "DawnLog.h"

#include "DawnTheme.h"
#include "EditorApplication.h"
#include "SceneDocument.h"
#include "SceneSerialiser.h"

#include <amanuensis.hpp>

#include <filesystem>
#include <optional>
#include <string>

namespace {

constexpr const char* DefaultSceneFileName = "test_scene.json";

// Parses a JSON file via Amanuensis, logging any IO/parse failure on the way out.
// Returns nullopt on failure so the caller can decide how to degrade.
std::optional<Amanuensis::Value> LoadJsonFile(const std::filesystem::path& Path) {
    const Amanuensis::ParseResult Result = Amanuensis::Reader::ParseFile(Path);
    if (!Result.succeeded) {
        LOG_ERROR("Failed to parse {} at {}:{} — {}", Path.string(), Result.error.line,
                  Result.error.column, Result.error.message);
        return std::nullopt;
    }
    return Result.value;
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

    const std::filesystem::path ProjectPath = argv[1];
    LOG_INFO("Opening project file: {}", ProjectPath.string());

    const std::optional<Amanuensis::Value> ProjectJson = LoadJsonFile(ProjectPath);
    if (!ProjectJson) {
        return 1; // already logged
    }

    Dawn::ProjectData Project;
    if (!Dawn::SceneSerialiser::DeserialiseProject(*ProjectJson, Project)) {
        LOG_ERROR("Project file {} is not a valid project object", ProjectPath.string());
        return 1;
    }
    LOG_INFO("Project loaded: '{}'", Project.Name);

    // The entity schema. Without it the palette is empty, but the editor still runs.
    Dawn::EntitySchema Schema;
    if (Project.SchemaPath.empty()) {
        LOG_WARNING("Project specifies no schemaPath; entity palette will be empty");
    } else if (const std::optional<Amanuensis::Value> SchemaJson = LoadJsonFile(Project.SchemaPath)) {
        if (Dawn::SceneSerialiser::DeserialiseSchema(*SchemaJson, Schema)) {
            LOG_INFO("Schema loaded: {} entity type(s)", Schema.Types.size());
        } else {
            LOG_WARNING("Schema file {} has no entityTypes array", Project.SchemaPath);
        }
    }

    // The scene. The PoC opens the conventional scene file under the project's
    // scenes root; a missing/invalid scene starts an empty document rather than failing.
    Dawn::SceneDocument Scene;
    const std::filesystem::path ScenePath =
        std::filesystem::path(Project.ScenesRoot) / DefaultSceneFileName;
    if (const std::optional<Amanuensis::Value> SceneJson = LoadJsonFile(ScenePath)) {
        if (Dawn::SceneSerialiser::Deserialise(*SceneJson, Scene)) {
            LOG_INFO("Scene loaded: '{}' with {} entit{} from {}", Scene.Name,
                     Scene.Entities.size(), Scene.Entities.size() == 1 ? "y" : "ies",
                     ScenePath.string());
        } else {
            LOG_WARNING("Scene file {} is not a valid scene object; starting empty",
                        ScenePath.string());
        }
    } else {
        LOG_WARNING("No scene loaded; starting with an empty document");
    }

    Dawn::Theme Theme;
    Dawn::EditorApplication App(Theme, std::move(Project), std::move(Schema), std::move(Scene));
    return App.Run();
}
