#include "SceneSerialiser.h"

#include <cctype>
#include <cmath>
#include <optional>
#include <utility>

namespace Dawn {

namespace {

using Amanuensis::Json;
using Amanuensis::JsonValue;

// ---- safe, non-throwing field accessors -----------------------------------
// Amanuensis' As* accessors throw on a type mismatch, so every read goes through
// these: they check presence and type and fall back to a default otherwise.

const JsonValue* FindField(const JsonValue& Object, const char* Key) {
    return Json::IsObject(Object) ? Json::Find(Object, Key) : nullptr;
}

double NumberAsDouble(const JsonValue& Number) {
    return Json::IsInteger(Number) ? static_cast<double>(Json::AsInteger(Number)) : Json::AsDouble(Number);
}

std::optional<long long> NumberAsInteger(const JsonValue& Number) {
    if (Json::IsInteger(Number)) {
        return Json::AsInteger(Number);
    }
    const double Value = Json::AsDouble(Number);
    constexpr double FirstDoubleAboveLongLong = 9223372036854775808.0;
    if (!std::isfinite(Value) || Value < -FirstDoubleAboveLongLong || Value >= FirstDoubleAboveLongLong) {
        return std::nullopt;
    }
    return static_cast<long long>(Value);
}

std::string GetString(const JsonValue& Object, const char* Key, const std::string& Fallback = "") {
    const JsonValue* Found = FindField(Object, Key);
    return (Found && Json::IsString(*Found)) ? Json::AsString(*Found) : Fallback;
}

double GetDouble(const JsonValue& Object, const char* Key, double Fallback = 0.0) {
    const JsonValue* Found = FindField(Object, Key);
    return (Found && Json::IsNumber(*Found)) ? NumberAsDouble(*Found) : Fallback;
}

long long GetInt(const JsonValue& Object, const char* Key, long long Fallback = 0) {
    const JsonValue* Found = FindField(Object, Key);
    if (!Found || !Json::IsNumber(*Found)) {
        return Fallback;
    }
    return NumberAsInteger(*Found).value_or(Fallback);
}

bool GetBool(const JsonValue& Object, const char* Key, bool Fallback = false) {
    const JsonValue* Found = FindField(Object, Key);
    return (Found && Json::IsBoolean(*Found)) ? Json::AsBoolean(*Found) : Fallback;
}

// Returns a child object/array by key, or a reference to a shared empty Null value
// when absent — lets callers chain reads without intermediate null checks.
const JsonValue& Child(const JsonValue& Object, const char* Key) {
    static const JsonValue Empty;
    const JsonValue* Found = FindField(Object, Key);
    return Found ? *Found : Empty;
}

JsonValue MakeJsonValue(std::string Text) { return JsonValue{std::move(Text)}; }
JsonValue MakeJsonValue(long long Number) { return JsonValue{Number}; }
JsonValue MakeJsonValue(double Number) { return JsonValue{Number}; }
JsonValue MakeJsonValue(bool Flag) { return JsonValue{Flag}; }

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

bool SceneSerialiser::DeserialiseProject(const JsonValue& Root, ProjectData& Out) {
    if (!Json::IsObject(Root)) {
        return false;
    }
    const JsonValue& Project = Child(Root, "project");
    const JsonValue& Paths   = Child(Root, "paths");

    Out.Name       = GetString(Project, "name", "Untitled");
    Out.SchemaPath = GetString(Paths, "schemaPath");
    Out.AssetsRoot = GetString(Paths, "assetsRoot");
    Out.ScenesRoot = GetString(Paths, "scenesRoot");
    return true;
}

bool SceneSerialiser::DeserialiseSchema(const JsonValue& Root, EntitySchema& Out) {
    Out.Types.clear();
    if (!Json::IsObject(Root)) {
        return false;
    }
    const JsonValue& Types = Child(Root, "entityTypes");
    if (!Json::IsArray(Types)) {
        return false;
    }
    for (std::size_t Index = 0; Index < Json::Size(Types); ++Index) {
        const JsonValue& Entry = Json::At(Types, Index);
        if (!Json::IsObject(Entry)) {
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

bool SceneSerialiser::Deserialise(const JsonValue& Root, SceneDocument& OutDocument) {
    if (!Json::IsObject(Root)) {
        return false;
    }

    const JsonValue& Scene      = Child(Root, "scene");
    const JsonValue& Dimensions = Child(Scene, "dimensions");
    OutDocument.Name             = GetString(Scene, "name");
    OutDocument.DimensionsWidth  = GetInt(Dimensions, "width");
    OutDocument.DimensionsHeight = GetInt(Dimensions, "height");

    OutDocument.Layers.clear();
    const JsonValue& Layers = Child(Root, "layers");
    if (Json::IsArray(Layers)) {
        for (std::size_t Index = 0; Index < Json::Size(Layers); ++Index) {
            const JsonValue& Entry = Json::At(Layers, Index);
            LayerData Layer;
            Layer.Id      = GetString(Entry, "id");
            Layer.Order   = GetInt(Entry, "order");
            Layer.Visible = GetBool(Entry, "visible", true);
            Layer.Locked  = GetBool(Entry, "locked", false);
            OutDocument.Layers.push_back(std::move(Layer));
        }
    }

    OutDocument.Entities.clear();
    const JsonValue& Entities = Child(Root, "entities");
    if (Json::IsArray(Entities)) {
        for (std::size_t Index = 0; Index < Json::Size(Entities); ++Index) {
            const JsonValue& Entry      = Json::At(Entities, Index);
            const JsonValue& Transform  = Child(Entry, "transform");
            const JsonValue& Properties = Child(Entry, "properties");

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

JsonValue SceneSerialiser::Serialise(const SceneDocument& Document) {
    JsonValue Root = Json::MakeObject();

    JsonValue Scene = Json::MakeObject();
    Json::Insert(Scene, "name", MakeJsonValue(Document.Name));
    JsonValue Dimensions = Json::MakeObject();
    Json::Insert(Dimensions, "width", MakeJsonValue(static_cast<long long>(Document.DimensionsWidth)));
    Json::Insert(Dimensions, "height", MakeJsonValue(static_cast<long long>(Document.DimensionsHeight)));
    Json::Insert(Scene, "dimensions", std::move(Dimensions));
    Json::Insert(Root, "scene", std::move(Scene));

    JsonValue Layers = Json::MakeArray();
    for (const auto& Layer : Document.Layers) {
        JsonValue Entry = Json::MakeObject();
        Json::Insert(Entry, "id", MakeJsonValue(Layer.Id));
        Json::Insert(Entry, "order", MakeJsonValue(static_cast<long long>(Layer.Order)));
        Json::Insert(Entry, "visible", MakeJsonValue(Layer.Visible));
        Json::Insert(Entry, "locked", MakeJsonValue(Layer.Locked));
        Json::PushBack(Layers, std::move(Entry));
    }
    Json::Insert(Root, "layers", std::move(Layers));

    JsonValue Entities = Json::MakeArray();
    for (const auto& Entity : Document.Entities) {
        JsonValue Entry = Json::MakeObject();
        Json::Insert(Entry, "id", MakeJsonValue(Entity.Id));
        Json::Insert(Entry, "name", MakeJsonValue(Entity.Name));
        Json::Insert(Entry, "type", MakeJsonValue(Entity.Type));
        Json::Insert(Entry, "layer", MakeJsonValue(Entity.Layer));

        JsonValue Transform = Json::MakeObject();
        Json::Insert(Transform, "positionX", MakeJsonValue(Entity.PositionX));
        Json::Insert(Transform, "positionY", MakeJsonValue(Entity.PositionY));
        Json::Insert(Transform, "rotation", MakeJsonValue(Entity.Rotation));
        Json::Insert(Transform, "scaleX", MakeJsonValue(Entity.ScaleX));
        Json::Insert(Transform, "scaleY", MakeJsonValue(Entity.ScaleY));
        Json::Insert(Entry, "transform", std::move(Transform));

        JsonValue Properties = Json::MakeObject();
        Json::Insert(Properties, "width", MakeJsonValue(Entity.Width));
        Json::Insert(Properties, "height", MakeJsonValue(Entity.Height));
        Json::Insert(Entry, "properties", std::move(Properties));

        Json::PushBack(Entities, std::move(Entry));
    }
    Json::Insert(Root, "entities", std::move(Entities));

    return Root;
}

} // namespace Dawn
