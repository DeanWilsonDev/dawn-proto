#include <cimmerian/test.hpp>

#include "SceneCommand.h"
#include "SceneDocument.h"

#include <memory>
#include <string>

using Dawn::CommandStack;
using Dawn::CompoundCommand;
using Dawn::PlaceEntityCommand;
using Dawn::SceneDocument;

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
