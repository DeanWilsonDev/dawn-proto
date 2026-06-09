#include "SceneCommand.h"

#include "UuidGenerator.h"

#include <algorithm>
#include <utility>

namespace Dawn {

// ---------------------------------------------------------------------------
// CompoundCommand
// ---------------------------------------------------------------------------

CompoundCommand::CompoundCommand(std::string Label) : Label(std::move(Label)) {}

void CompoundCommand::Add(std::unique_ptr<SceneCommand> Command) {
    Commands.push_back(std::move(Command));
}

void CompoundCommand::Execute(SceneDocument& Document) {
    for (auto& Command : Commands) {
        Command->Execute(Document);
    }
}

void CompoundCommand::Undo(SceneDocument& Document) {
    for (auto Iterator = Commands.rbegin(); Iterator != Commands.rend(); ++Iterator) {
        (*Iterator)->Undo(Document);
    }
}

// ---------------------------------------------------------------------------
// CommandStack
// ---------------------------------------------------------------------------

void CommandStack::Execute(std::unique_ptr<SceneCommand> Command, SceneDocument& Document) {
    Command->Execute(Document);
    UndoStack.push_back(std::move(Command));
    RedoStack.clear(); // a fresh edit discards any redoable future
}

std::string CommandStack::Undo(SceneDocument& Document) {
    if (UndoStack.empty()) {
        return {};
    }
    std::unique_ptr<SceneCommand> Command = std::move(UndoStack.back());
    UndoStack.pop_back();
    Command->Undo(Document);
    const std::string Label = Command->Describe();
    RedoStack.push_back(std::move(Command));
    return Label;
}

std::string CommandStack::Redo(SceneDocument& Document) {
    if (RedoStack.empty()) {
        return {};
    }
    std::unique_ptr<SceneCommand> Command = std::move(RedoStack.back());
    RedoStack.pop_back();
    Command->Execute(Document);
    const std::string Label = Command->Describe();
    UndoStack.push_back(std::move(Command));
    return Label;
}

void CommandStack::Clear() {
    UndoStack.clear();
    RedoStack.clear();
}

// ---------------------------------------------------------------------------
// PlaceEntityCommand
// ---------------------------------------------------------------------------

PlaceEntityCommand::PlaceEntityCommand(EntityData Entity) : Entity(std::move(Entity)) {
    if (this->Entity.Id.empty()) {
        this->Entity.Id = GenerateUuidV4();
    }
}

PlaceEntityCommand::PlaceEntityCommand(std::string Type, double PositionX, double PositionY) {
    Entity.Id        = GenerateUuidV4();
    Entity.Name      = Type;
    Entity.Type      = std::move(Type);
    Entity.PositionX = PositionX;
    Entity.PositionY = PositionY;
}

void PlaceEntityCommand::Execute(SceneDocument& Document) {
    Document.Entities.push_back(Entity);
    Document.MarkDirty();
}

void PlaceEntityCommand::Undo(SceneDocument& Document) {
    auto& Entities = Document.Entities;
    Entities.erase(std::remove_if(Entities.begin(), Entities.end(),
                                  [this](const EntityData& E) { return E.Id == Entity.Id; }),
                   Entities.end());
    Document.MarkDirty();
}

std::string PlaceEntityCommand::Describe() const {
    return "Place " + Entity.Type + " (" + Entity.Name + ")";
}

} // namespace Dawn
