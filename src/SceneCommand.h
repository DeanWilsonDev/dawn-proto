#pragma once

#include "SceneDocument.h"

#include <memory>
#include <string>
#include <vector>

namespace Dawn {

// Base class for every reversible edit to a SceneDocument. Pure logic — operates
// only on the document, knows nothing about the UI. Execute applies the edit; Undo
// reverses it. The pair must be exact inverses so redo (a second Execute) is safe.
class SceneCommand {
public:
    virtual ~SceneCommand() = default;

    virtual void Execute(SceneDocument& Document) = 0;
    virtual void Undo(SceneDocument& Document)    = 0;

    // Human-readable label, used by callers for logging. Not behaviour-bearing.
    virtual std::string Describe() const = 0;
};

// Groups several commands into one atomic undo/redo step. Executes children in
// order, undoes them in reverse — the standard composite-command pattern.
class CompoundCommand : public SceneCommand {
public:
    explicit CompoundCommand(std::string Label = "Compound");

    void Add(std::unique_ptr<SceneCommand> Command);
    bool IsEmpty() const { return Commands.empty(); }

    void        Execute(SceneDocument& Document) override;
    void        Undo(SceneDocument& Document) override;
    std::string Describe() const override { return Label; }

private:
    std::string                                Label;
    std::vector<std::unique_ptr<SceneCommand>> Commands;
};

// The undo/redo history. Executing a command runs it, records it on the undo stack,
// and clears the redo stack (a new edit invalidates the redone future).
class CommandStack {
public:
    // Runs Command against Document and records it for undo.
    void Execute(std::unique_ptr<SceneCommand> Command, SceneDocument& Document);

    // Records a command whose effect has *already* been applied to the document (e.g.
    // a live drag or property edit), without calling Execute. Still clears redo. Undo
    // reverses it; a later Redo re-applies it via Execute.
    void Record(std::unique_ptr<SceneCommand> Command);

    bool CanUndo() const { return !UndoStack.empty(); }
    bool CanRedo() const { return !RedoStack.empty(); }

    // Reverses / re-applies the most recent command. No-ops if the stack is empty.
    // Returns the command's label (empty string if nothing happened) for logging.
    std::string Undo(SceneDocument& Document);
    std::string Redo(SceneDocument& Document);

    void Clear();

private:
    std::vector<std::unique_ptr<SceneCommand>> UndoStack;
    std::vector<std::unique_ptr<SceneCommand>> RedoStack;
};

// Adds a fully-formed entity to the scene. The caller builds the EntityData (with a
// UUID, schema-derived size, and name); the command just inserts and removes it, so
// redo re-inserts an identical entity (stable id).
class PlaceEntityCommand : public SceneCommand {
public:
    explicit PlaceEntityCommand(EntityData Entity);

    // Convenience constructor for tests / quick placement: builds a minimal entity
    // with a generated UUID, Name defaulting to Type, and zero size.
    PlaceEntityCommand(std::string Type, double PositionX, double PositionY);

    void        Execute(SceneDocument& Document) override;
    void        Undo(SceneDocument& Document) override;
    std::string Describe() const override;

    const std::string& EntityId() const { return Entity.Id; }

private:
    EntityData Entity;
};

// Moves an entity from one position to another. Used to make a viewport drag or a
// properties-panel position edit a single undoable step; the live edit is applied
// first, then this is recorded with the net before/after via CommandStack::Record.
class MoveEntityCommand : public SceneCommand {
public:
    MoveEntityCommand(std::string Id, double FromX, double FromY, double ToX, double ToY);

    void        Execute(SceneDocument& Document) override;
    void        Undo(SceneDocument& Document) override;
    std::string Describe() const override;

private:
    std::string Id;
    double      FromX, FromY, ToX, ToY;
};

// Renames an entity. Recorded (not executed) after a live properties-panel edit.
class RenameEntityCommand : public SceneCommand {
public:
    RenameEntityCommand(std::string Id, std::string From, std::string To);

    void        Execute(SceneDocument& Document) override;
    void        Undo(SceneDocument& Document) override;
    std::string Describe() const override;

private:
    std::string Id, From, To;
};

} // namespace Dawn
