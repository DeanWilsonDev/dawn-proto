#pragma once

#include "SceneDocument.h"

#include <amanuensis/value.hpp>

#include <string>
#include <vector>

namespace Dawn {

// A plain RGBA colour, free of SDL so it lives in the pure-logic layer. The editor
// converts this to an SDL_Color at the rendering boundary (EditorApplication).
struct Color {
    unsigned char R{255};
    unsigned char G{255};
    unsigned char B{255};
    unsigned char A{255};
};

// Parses "#RRGGBB" (or "#RRGGBBAA") into a Color. On a malformed string, returns
// the supplied fallback. Never throws.
Color ParseHexColor(const std::string& Hex, Color Fallback = {});

// One entity type from the schema: how it is presented and its placement defaults.
struct EntityTypeInfo {
    std::string Id;
    std::string DisplayName;
    Color       EditorColor;
    double      DefaultWidth{0.0};
    double      DefaultHeight{0.0};
};

// The loaded entity schema. The palette is built from Types; the viewport resolves
// an entity's colour by looking its Type up here.
struct EntitySchema {
    std::vector<EntityTypeInfo> Types;
    const EntityTypeInfo*       Find(const std::string& Id) const;
};

// Parsed .dawn project file: the project name and the paths it points at.
struct ProjectData {
    std::string Name;
    std::string SchemaPath;
    std::string AssetsRoot;
    std::string ScenesRoot;
};

// All scene/project/schema JSON read and write for Dawn, via Amanuensis. Pure logic:
// operates on already-parsed Amanuensis::Value trees, returns plain structs, and is
// defensive — missing or wrong-typed fields fall back to defaults rather than throw,
// so a malformed file degrades gracefully instead of crashing. File IO (ParseFile /
// WriteToFile) and any logging stay with the caller, which owns Firefly.
class SceneSerialiser {
public:
    // .dawn project file. Returns false only if Root is not a JSON object.
    static bool DeserialiseProject(const Amanuensis::Value& Root, ProjectData& Out);

    // entity_schema.json. Returns false if Root is not an object or lacks entityTypes.
    static bool DeserialiseSchema(const Amanuensis::Value& Root, EntitySchema& Out);

    // scene JSON -> document. Returns false if Root is not an object. Populates the
    // document with whatever valid fields are present.
    static bool Deserialise(const Amanuensis::Value& Root, SceneDocument& OutDocument);

    // document -> scene JSON, matching the bootstrap scene format exactly so a
    // load/save round-trip is lossless.
    static Amanuensis::Value Serialise(const SceneDocument& Document);
};

} // namespace Dawn
