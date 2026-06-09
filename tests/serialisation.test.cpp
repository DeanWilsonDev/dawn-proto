#include <cimmerian/test.hpp>

#include "SceneDocument.h"
#include "SceneSerialiser.h"

#include <amanuensis/io/reader.hpp>
#include <amanuensis/io/writer.hpp>

#include <string>

using Dawn::SceneDocument;
using Dawn::SceneSerialiser;

namespace {

const char* const SampleScene = R"({
  "scene": { "name": "test_scene", "dimensions": { "width": 3840, "height": 1080 } },
  "layers": [
    { "id": "background", "order": 0, "visible": true, "locked": false },
    { "id": "midground",  "order": 1, "visible": true, "locked": false }
  ],
  "entities": [
    {
      "id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890",
      "name": "floor_left",
      "type": "platform",
      "layer": "midground",
      "transform": { "positionX": 100.0, "positionY": 800.0,
                     "rotation": 0.0, "scaleX": 1.0, "scaleY": 1.0 },
      "properties": { "width": 400.0, "height": 32.0 }
    },
    {
      "id": "b2c3d4e5-f6a7-8901-bcde-f12345678901",
      "name": "player_spawn",
      "type": "character",
      "layer": "midground",
      "transform": { "positionX": 200.0, "positionY": 700.0,
                     "rotation": 0.0, "scaleX": 1.0, "scaleY": 1.0 },
      "properties": { "width": 48.0, "height": 96.0 }
    }
  ]
})";

} // namespace

DESCRIBE("SceneSerialiser", {
    IT("round-trips a scene without data loss", {
        auto ParseResult = Amanuensis::Reader::ParseString(SampleScene);
        REQUIRE_TRUE(ParseResult.succeeded);

        SceneDocument Doc;
        REQUIRE_TRUE(SceneSerialiser::Deserialise(ParseResult.value, Doc));

        Amanuensis::Value Written = SceneSerialiser::Serialise(Doc);
        std::string RoundTripped = Amanuensis::Writer::WriteToString(Written);

        auto ReParseResult = Amanuensis::Reader::ParseString(RoundTripped);
        REQUIRE_TRUE(ReParseResult.succeeded);
        const Amanuensis::Value& Re = ReParseResult.value;

        ASSERT_EQUAL(Re.Get("scene").Get("name").AsString(), std::string("test_scene"));
        ASSERT_EQUAL(Re.Get("scene").Get("dimensions").Get("width").AsInteger(),
                     static_cast<long long>(3840));
        ASSERT_EQUAL(Re.Get("layers").Size(), std::size_t{2});
        ASSERT_EQUAL(Re.Get("entities").Size(), std::size_t{2});

        const Amanuensis::Value& Entity0 = Re.Get("entities").At(0);
        ASSERT_EQUAL(Entity0.Get("name").AsString(), std::string("floor_left"));
        ASSERT_EQUAL(Entity0.Get("type").AsString(), std::string("platform"));
        ASSERT_EQUAL(Entity0.Get("transform").Get("positionX").AsDouble(), 100.0);
        ASSERT_EQUAL(Entity0.Get("properties").Get("width").AsDouble(), 400.0);
    });

    IT("deserialises entity transform and properties correctly", {
        auto ParseResult = Amanuensis::Reader::ParseString(SampleScene);
        REQUIRE_TRUE(ParseResult.succeeded);

        SceneDocument Doc;
        REQUIRE_TRUE(SceneSerialiser::Deserialise(ParseResult.value, Doc));

        REQUIRE_EQUAL(Doc.Entities.size(), std::size_t{2});
        ASSERT_EQUAL(Doc.Name, std::string("test_scene"));
        ASSERT_EQUAL(Doc.DimensionsWidth, static_cast<long long>(3840));
        ASSERT_EQUAL(Doc.Layers.size(), std::size_t{2});

        const Dawn::EntityData& First = Doc.Entities[0];
        ASSERT_EQUAL(First.Id, std::string("a1b2c3d4-e5f6-7890-abcd-ef1234567890"));
        ASSERT_EQUAL(First.Type, std::string("platform"));
        ASSERT_EQUAL(First.Layer, std::string("midground"));
        ASSERT_EQUAL(First.PositionX, 100.0);
        ASSERT_EQUAL(First.PositionY, 800.0);
        ASSERT_EQUAL(First.Width, 400.0);
        ASSERT_EQUAL(First.Height, 32.0);

        const Dawn::EntityData& Second = Doc.Entities[1];
        ASSERT_EQUAL(Second.Type, std::string("character"));
        ASSERT_EQUAL(Second.Height, 96.0);
    });

    IT("degrades gracefully on a non-object root", {
        auto ParseResult = Amanuensis::Reader::ParseString("[]");
        REQUIRE_TRUE(ParseResult.succeeded);

        SceneDocument Doc;
        ASSERT_FALSE(SceneSerialiser::Deserialise(ParseResult.value, Doc));
        ASSERT_EQUAL(Doc.Entities.size(), std::size_t{0});
    });

    IT("tolerates missing optional fields without throwing", {
        // Minimal scene: no layers, no transform/properties on the entity.
        auto ParseResult = Amanuensis::Reader::ParseString(
            R"({ "scene": { "name": "tiny" }, "entities": [ { "id": "x", "type": "platform" } ] })");
        REQUIRE_TRUE(ParseResult.succeeded);

        SceneDocument Doc;
        REQUIRE_TRUE(SceneSerialiser::Deserialise(ParseResult.value, Doc));
        REQUIRE_EQUAL(Doc.Entities.size(), std::size_t{1});
        ASSERT_EQUAL(Doc.Entities[0].Type, std::string("platform"));
        ASSERT_EQUAL(Doc.Entities[0].PositionX, 0.0);
        ASSERT_EQUAL(Doc.Entities[0].ScaleX, 1.0); // default, not zero
    });
});

DESCRIBE("ParseHexColor", {
    IT("parses a 6-digit hex colour", {
        Dawn::Color C = Dawn::ParseHexColor("#4A90D9");
        ASSERT_EQUAL(static_cast<int>(C.R), 74);
        ASSERT_EQUAL(static_cast<int>(C.G), 144);
        ASSERT_EQUAL(static_cast<int>(C.B), 217);
        ASSERT_EQUAL(static_cast<int>(C.A), 255);
    });

    IT("falls back on a malformed string", {
        // NB: brace-init with commas (Color{1,2,3,4}) would break the IT macro — the
        // preprocessor only shields commas inside parentheses — so set fields directly.
        Dawn::Color Fallback;
        Fallback.R = 1;
        Fallback.B = 3;
        Dawn::Color C = Dawn::ParseHexColor("not-a-colour", Fallback);
        ASSERT_EQUAL(static_cast<int>(C.R), 1);
        ASSERT_EQUAL(static_cast<int>(C.B), 3);
    });
});
