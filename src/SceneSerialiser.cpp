#include "SceneSerialiser.h"

#include <cctype>

namespace Dawn {

namespace {

using Amanuensis::Value;

// ---- safe, non-throwing field accessors -----------------------------------
// Amanuensis' As* accessors throw on a type mismatch, so every read goes through
// these: they check presence and type and fall back to a default otherwise.

std::string GetString(const Value& Object, const char* Key, const std::string& Fallback = "") {
    const Value* Found = Object.IsObject() ? Object.Find(Key) : nullptr;
    return (Found && Found->IsString()) ? Found->AsString() : Fallback;
}

double GetDouble(const Value& Object, const char* Key, double Fallback = 0.0) {
    const Value* Found = Object.IsObject() ? Object.Find(Key) : nullptr;
    return (Found && Found->IsNumber()) ? Found->AsDouble() : Fallback;
}

long long GetInt(const Value& Object, const char* Key, long long Fallback = 0) {
    const Value* Found = Object.IsObject() ? Object.Find(Key) : nullptr;
    return (Found && Found->IsNumber()) ? Found->AsInteger() : Fallback;
}

bool GetBool(const Value& Object, const char* Key, bool Fallback = false) {
    const Value* Found = Object.IsObject() ? Object.Find(Key) : nullptr;
    return (Found && Found->IsBoolean()) ? Found->AsBoolean() : Fallback;
}

// Returns a child object/array by key, or a reference to a shared empty Null value
// when absent — lets callers chain reads without intermediate null checks.
const Value& Child(const Value& Object, const char* Key) {
    static const Value Empty;
    const Value* Found = Object.IsObject() ? Object.Find(Key) : nullptr;
    return Found ? *Found : Empty;
}

int HexNibble(char C) {
    if (C >= '0' && C <= '9') return C - '0';
    C = static_cast<char>(std::tolower(static_cast<unsigned char>(C)));
    if (C >= 'a' && C <= 'f') return 10 + (C - 'a');
    return -1;
}

} // namespace

Color ParseHexColor(const std::string& Hex, Color Fallback) {
    if (Hex.empty() || Hex.front() != '#') {
        return Fallback;
    }
    const std::string Digits = Hex.substr(1);
    if (Digits.size() != 6 && Digits.size() != 8) {
        return Fallback;
    }
    auto Byte = [&](std::size_t Index) -> int {
        const int High = HexNibble(Digits[Index]);
        const int Low  = HexNibble(Digits[Index + 1]);
        return (High < 0 || Low < 0) ? -1 : (High << 4) | Low;
    };
    const int R = Byte(0), G = Byte(2), B = Byte(4);
    const int A = (Digits.size() == 8) ? Byte(6) : 255;
    if (R < 0 || G < 0 || B < 0 || A < 0) {
        return Fallback;
    }
    return {static_cast<unsigned char>(R), static_cast<unsigned char>(G),
            static_cast<unsigned char>(B), static_cast<unsigned char>(A)};
}

const EntityTypeInfo* EntitySchema::Find(const std::string& Id) const {
    for (const auto& Type : Types) {
        if (Type.Id == Id) {
            return &Type;
        }
    }
    return nullptr;
}

bool SceneSerialiser::DeserialiseProject(const Value& Root, ProjectData& Out) {
    if (!Root.IsObject()) {
        return false;
    }
    const Value& Project = Child(Root, "project");
    const Value& Paths   = Child(Root, "paths");

    Out.Name       = GetString(Project, "name", "Untitled");
    Out.SchemaPath = GetString(Paths, "schemaPath");
    Out.AssetsRoot = GetString(Paths, "assetsRoot");
    Out.ScenesRoot = GetString(Paths, "scenesRoot");
    return true;
}

bool SceneSerialiser::DeserialiseSchema(const Value& Root, EntitySchema& Out) {
    Out.Types.clear();
    if (!Root.IsObject()) {
        return false;
    }
    const Value& Types = Child(Root, "entityTypes");
    if (!Types.IsArray()) {
        return false;
    }
    for (std::size_t Index = 0; Index < Types.Size(); ++Index) {
        const Value& Entry = Types.At(Index);
        if (!Entry.IsObject()) {
            continue;
        }
        EntityTypeInfo Info;
        Info.Id            = GetString(Entry, "id");
        Info.DisplayName   = GetString(Entry, "displayName", Info.Id);
        Info.EditorColor   = ParseHexColor(GetString(Entry, "editorColor"), {200, 200, 200, 255});
        Info.DefaultWidth  = GetDouble(Entry, "defaultWidth", 32.0);
        Info.DefaultHeight = GetDouble(Entry, "defaultHeight", 32.0);
        if (!Info.Id.empty()) {
            Out.Types.push_back(std::move(Info));
        }
    }
    return true;
}

bool SceneSerialiser::Deserialise(const Value& Root, SceneDocument& OutDocument) {
    if (!Root.IsObject()) {
        return false;
    }

    const Value& Scene      = Child(Root, "scene");
    const Value& Dimensions = Child(Scene, "dimensions");
    OutDocument.Name             = GetString(Scene, "name");
    OutDocument.DimensionsWidth  = GetInt(Dimensions, "width");
    OutDocument.DimensionsHeight = GetInt(Dimensions, "height");

    OutDocument.Layers.clear();
    const Value& Layers = Child(Root, "layers");
    if (Layers.IsArray()) {
        for (std::size_t Index = 0; Index < Layers.Size(); ++Index) {
            const Value& Entry = Layers.At(Index);
            LayerData Layer;
            Layer.Id      = GetString(Entry, "id");
            Layer.Order   = GetInt(Entry, "order");
            Layer.Visible = GetBool(Entry, "visible", true);
            Layer.Locked  = GetBool(Entry, "locked", false);
            OutDocument.Layers.push_back(std::move(Layer));
        }
    }

    OutDocument.Entities.clear();
    const Value& Entities = Child(Root, "entities");
    if (Entities.IsArray()) {
        for (std::size_t Index = 0; Index < Entities.Size(); ++Index) {
            const Value& Entry      = Entities.At(Index);
            const Value& Transform  = Child(Entry, "transform");
            const Value& Properties = Child(Entry, "properties");

            EntityData Entity;
            Entity.Id        = GetString(Entry, "id");
            Entity.Name      = GetString(Entry, "name");
            Entity.Type      = GetString(Entry, "type");
            Entity.Layer     = GetString(Entry, "layer");
            Entity.PositionX = GetDouble(Transform, "positionX");
            Entity.PositionY = GetDouble(Transform, "positionY");
            Entity.Rotation  = GetDouble(Transform, "rotation", 0.0);
            Entity.ScaleX    = GetDouble(Transform, "scaleX", 1.0);
            Entity.ScaleY    = GetDouble(Transform, "scaleY", 1.0);
            Entity.Width     = GetDouble(Properties, "width");
            Entity.Height    = GetDouble(Properties, "height");
            OutDocument.Entities.push_back(std::move(Entity));
        }
    }

    return true;
}

Value SceneSerialiser::Serialise(const SceneDocument& Document) {
    Value Root = Value::MakeObject();

    Value Scene = Value::MakeObject();
    Scene.Insert("name", Document.Name);
    Value Dimensions = Value::MakeObject();
    Dimensions.Insert("width", static_cast<long long>(Document.DimensionsWidth));
    Dimensions.Insert("height", static_cast<long long>(Document.DimensionsHeight));
    Scene.Insert("dimensions", Dimensions);
    Root.Insert("scene", Scene);

    Value Layers = Value::MakeArray();
    for (const auto& Layer : Document.Layers) {
        Value Entry = Value::MakeObject();
        Entry.Insert("id", Layer.Id);
        Entry.Insert("order", static_cast<long long>(Layer.Order));
        Entry.Insert("visible", Layer.Visible);
        Entry.Insert("locked", Layer.Locked);
        Layers.PushBack(Entry);
    }
    Root.Insert("layers", Layers);

    Value Entities = Value::MakeArray();
    for (const auto& Entity : Document.Entities) {
        Value Entry = Value::MakeObject();
        Entry.Insert("id", Entity.Id);
        Entry.Insert("name", Entity.Name);
        Entry.Insert("type", Entity.Type);
        Entry.Insert("layer", Entity.Layer);

        Value Transform = Value::MakeObject();
        Transform.Insert("positionX", Entity.PositionX);
        Transform.Insert("positionY", Entity.PositionY);
        Transform.Insert("rotation", Entity.Rotation);
        Transform.Insert("scaleX", Entity.ScaleX);
        Transform.Insert("scaleY", Entity.ScaleY);
        Entry.Insert("transform", Transform);

        Value Properties = Value::MakeObject();
        Properties.Insert("width", Entity.Width);
        Properties.Insert("height", Entity.Height);
        Entry.Insert("properties", Properties);

        Entities.PushBack(Entry);
    }
    Root.Insert("entities", Entities);

    return Root;
}

} // namespace Dawn
