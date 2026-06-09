#pragma once

#include <string>
#include <vector>

namespace Dawn {

// A single placed entity. Pure data — no rendering, colour, or file knowledge.
// The editor colour is NOT stored here: it is a property of the entity's type and
// is resolved from the schema at render time, keeping this struct free of SDL types
// so it compiles into the SDL-free dawn_tests binary.
struct EntityData {
    std::string Id;
    std::string Name;
    std::string Type;
    std::string Layer;

    // transform
    double PositionX{0.0};
    double PositionY{0.0};
    double Rotation{0.0};
    double ScaleX{1.0};
    double ScaleY{1.0};

    // properties
    double Width{0.0};
    double Height{0.0};
};

// A scene layer. Layers exist in the document and JSON but are not UI-editable in
// this PoC (deferred per the brief); they round-trip faithfully through save/load.
struct LayerData {
    std::string Id;
    long long   Order{0};
    bool        Visible{true};
    bool        Locked{false};
};

// The in-memory scene model. Owns the entity and layer lists and a dirty flag.
// Knows nothing about the UI, files, or commands — those operate on it from outside.
class SceneDocument {
public:
    std::string Name;
    long long   DimensionsWidth{0};
    long long   DimensionsHeight{0};

    std::vector<LayerData>  Layers;
    std::vector<EntityData> Entities;

    // Returns a pointer to the entity with the given id, or nullptr if absent.
    EntityData*       FindEntity(const std::string& Id);
    const EntityData* FindEntity(const std::string& Id) const;

    bool IsDirty() const { return Dirty; }
    void MarkDirty()     { Dirty = true; }
    void ClearDirty()    { Dirty = false; }

private:
    bool Dirty{false};
};

} // namespace Dawn
