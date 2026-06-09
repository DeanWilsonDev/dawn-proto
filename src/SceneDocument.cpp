#include "SceneDocument.h"

namespace Dawn {

EntityData* SceneDocument::FindEntity(const std::string& Id) {
    for (auto& Entity : Entities) {
        if (Entity.Id == Id) {
            return &Entity;
        }
    }
    return nullptr;
}

const EntityData* SceneDocument::FindEntity(const std::string& Id) const {
    for (const auto& Entity : Entities) {
        if (Entity.Id == Id) {
            return &Entity;
        }
    }
    return nullptr;
}

} // namespace Dawn
