#include <cimmerian/test.hpp>

#include "SceneCommand.h"
#include "SceneDocument.h"

#include <memory>
#include <string>

using Dawn::CommandStack;
using Dawn::CompoundCommand;
using Dawn::MoveEntityCommand;
using Dawn::PlaceEntityCommand;
using Dawn::RenameEntityCommand;
using Dawn::SceneDocument;

namespace {

// Builds a document with one entity at a known position, returns its id.
std::string SeedOneEntity(SceneDocument& Doc, double X, double Y) {
    PlaceEntityCommand Place("platform", X, Y);
    Place.Execute(Doc);
    return Doc.Entities.front().Id;
}

} // namespace

DESCRIBE("PlaceEntityCommand", {
    IT("adds an entity to the document", {
        SceneDocument Doc;
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{0});

        PlaceEntityCommand Cmd("platform", 100.0, 200.0);
        Cmd.Execute(Doc);

        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{1});
        ASSERT_EQUAL(Doc.Entities[0].Type, std::string("platform"));
        ASSERT_EQUAL(Doc.Entities[0].PositionX, 100.0);
        ASSERT_EQUAL(Doc.Entities[0].PositionY, 200.0);
        ASSERT_FALSE(Doc.Entities[0].Id.empty()); // a UUID was generated
        ASSERT_TRUE(Doc.IsDirty());
    });

    IT("undoes cleanly", {
        SceneDocument Doc;
        PlaceEntityCommand Cmd("platform", 100.0, 200.0);
        Cmd.Execute(Doc);
        REQUIRE_EQUAL(Doc.Entities.size(), std::size_t{1});

        Cmd.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{0});
    });

    IT("redo (re-execute) restores the same entity id", {
        SceneDocument Doc;
        PlaceEntityCommand Cmd("trigger", 0.0, 0.0);
        Cmd.Execute(Doc);
        const std::string Id = Doc.Entities[0].Id;

        Cmd.Undo(Doc);
        REQUIRE_EQUAL(Doc.Entities.size(), std::size_t{0});

        Cmd.Execute(Doc); // redo
        REQUIRE_EQUAL(Doc.Entities.size(), std::size_t{1});
        ASSERT_EQUAL(Doc.Entities[0].Id, Id);
    });
});

DESCRIBE("CommandStack", {
    IT("undo then redo round-trips through the stack", {
        SceneDocument Doc;
        CommandStack Stack;

        Stack.Execute(std::make_unique<PlaceEntityCommand>("platform", 1.0, 2.0), Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{1});
        ASSERT_TRUE(Stack.CanUndo());
        ASSERT_FALSE(Stack.CanRedo());

        Stack.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{0});
        ASSERT_FALSE(Stack.CanUndo());
        ASSERT_TRUE(Stack.CanRedo());

        Stack.Redo(Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{1});
        ASSERT_TRUE(Stack.CanUndo());
    });

    IT("a new edit clears the redo stack", {
        SceneDocument Doc;
        CommandStack Stack;
        Stack.Execute(std::make_unique<PlaceEntityCommand>("platform", 0.0, 0.0), Doc);
        Stack.Undo(Doc);
        REQUIRE_TRUE(Stack.CanRedo());

        Stack.Execute(std::make_unique<PlaceEntityCommand>("trigger", 0.0, 0.0), Doc);
        ASSERT_FALSE(Stack.CanRedo());
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{1});
    });
});

DESCRIBE("MoveEntityCommand", {
    IT("moves an entity and undoes back to the original position", {
        SceneDocument Doc;
        const std::string Id = SeedOneEntity(Doc, 100.0, 200.0);

        MoveEntityCommand Move(Id, 100.0, 200.0, 350.0, 400.0);
        Move.Execute(Doc);
        ASSERT_EQUAL(Doc.Entities[0].PositionX, 350.0);
        ASSERT_EQUAL(Doc.Entities[0].PositionY, 400.0);

        Move.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities[0].PositionX, 100.0);
        ASSERT_EQUAL(Doc.Entities[0].PositionY, 200.0);
    });

    IT("is a no-op for an unknown entity id", {
        SceneDocument Doc;
        SeedOneEntity(Doc, 0.0, 0.0);
        MoveEntityCommand Move("does-not-exist", 0.0, 0.0, 9.0, 9.0);
        Move.Execute(Doc); // must not throw or crash
        ASSERT_EQUAL(Doc.Entities[0].PositionX, 0.0);
    });

    IT("records a live move and replays it through the stack", {
        SceneDocument Doc;
        const std::string Id = SeedOneEntity(Doc, 10.0, 10.0);

        // Simulate a live drag: mutate directly, then record the net change.
        Doc.Entities[0].PositionX = 60.0;
        Doc.Entities[0].PositionY = 80.0;
        CommandStack Stack;
        Stack.Record(std::make_unique<MoveEntityCommand>(Id, 10.0, 10.0, 60.0, 80.0));

        Stack.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities[0].PositionX, 10.0);
        ASSERT_EQUAL(Doc.Entities[0].PositionY, 10.0);
        Stack.Redo(Doc);
        ASSERT_EQUAL(Doc.Entities[0].PositionX, 60.0);
        ASSERT_EQUAL(Doc.Entities[0].PositionY, 80.0);
    });
});

DESCRIBE("RenameEntityCommand", {
    IT("renames an entity and undoes the rename", {
        SceneDocument Doc;
        const std::string Id = SeedOneEntity(Doc, 0.0, 0.0);
        const std::string Original = Doc.Entities[0].Name;

        RenameEntityCommand Rename(Id, Original, "hero_start");
        Rename.Execute(Doc);
        ASSERT_EQUAL(Doc.Entities[0].Name, std::string("hero_start"));

        Rename.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities[0].Name, Original);
    });
});

DESCRIBE("undo across mixed command types", {
    IT("undoes place, move, and rename step by step in reverse order", {
        SceneDocument Doc;
        CommandStack Stack;

        // 1. place
        Stack.Execute(std::make_unique<PlaceEntityCommand>("platform", 100.0, 200.0), Doc);
        const std::string Id = Doc.Entities[0].Id;

        // 2. move (live + record, as the editor does)
        Doc.Entities[0].PositionX = 300.0;
        Stack.Record(std::make_unique<MoveEntityCommand>(Id, 100.0, 200.0, 300.0, 200.0));

        // 3. rename (live + record)
        Doc.Entities[0].Name = "renamed";
        Stack.Record(std::make_unique<RenameEntityCommand>(Id, "platform", "renamed"));

        // undo rename
        Stack.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities[0].Name, std::string("platform"));
        ASSERT_EQUAL(Doc.Entities[0].PositionX, 300.0);
        // undo move
        Stack.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities[0].PositionX, 100.0);
        REQUIRE_EQUAL(Doc.Entities.size(), std::size_t{1});
        // undo place
        Stack.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{0});
        ASSERT_FALSE(Stack.CanUndo());
    });
});

DESCRIBE("CompoundCommand", {
    IT("executes and undoes its children atomically", {
        SceneDocument Doc;
        auto Compound = std::make_unique<CompoundCommand>("Place two");
        Compound->Add(std::make_unique<PlaceEntityCommand>("platform", 0.0, 0.0));
        Compound->Add(std::make_unique<PlaceEntityCommand>("trigger", 0.0, 0.0));

        CommandStack Stack;
        Stack.Execute(std::move(Compound), Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{2});

        Stack.Undo(Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{0});

        Stack.Redo(Doc);
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{2});
    });
});
