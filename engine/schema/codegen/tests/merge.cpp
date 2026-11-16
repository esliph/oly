#include <doctest/doctest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "oly/schema/schema.hpp"

namespace {
  const std::filesystem::path corpus = OLY_SCHEMA_CORPUS;

  const oly::schema::PluginSource counting{"counting", "counting.oly", "component Counter { value: u32 }\nevent Counted { value: u32 }\n"};

  const std::string gameSource = "component Position { x: f32, y: f32 }\nevent Hit { amount: f32 }\naction jump: button\nstate menu\n";

  std::string readText(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();

    return text.str();
  }

  std::vector<std::filesystem::path> entriesIn(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> entries;
    for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(directory)) {
      entries.push_back(entry.path());
    }

    std::sort(entries.begin(), entries.end());
    return entries;
  }

  // A corpus plugin file is "<order>-<plugin>.oly"; the order only sorts the files.
  oly::schema::PluginSource pluginSourceOf(const std::filesystem::path& file) {
    const std::string stem = file.stem().string();

    return {stem.substr(stem.find('-') + 1), file.filename().string(), readText(file)};
  }
}

TEST_CASE("plugin declarations come first: ComponentId order, event order") {
  const oly::schema::Schema schema = oly::schema::mergeSchemas({&counting, 1}, gameSource, "game.oly");

  REQUIRE(schema.types.size() == 2);
  CHECK(schema.types[0].name == "Counter");
  CHECK(schema.types[0].origin == "counting");
  CHECK(schema.types[1].name == "Position");
  CHECK(schema.types[1].origin.empty());
  REQUIRE(schema.events.size() == 2);
  CHECK(schema.events[0].name == "Counted");
  CHECK(schema.events[0].origin == "counting");
  CHECK(schema.events[1].name == "Hit");
  CHECK(schema.plugins == std::vector<std::string>{"counting"});
  CHECK(schema.fileName == "game.oly");
}

TEST_CASE("without plugins the merged schema equals parseSchema, hash included") {
  const oly::schema::Schema parsed = oly::schema::parseSchema(gameSource, "game.oly");

  const oly::schema::Schema merged = oly::schema::mergeSchemas({}, gameSource, "game.oly");

  CHECK(merged.hash == parsed.hash);
  CHECK(merged.types.size() == parsed.types.size());
  CHECK(merged.events.size() == parsed.events.size());
  CHECK(merged.actions.size() == parsed.actions.size());
  CHECK(merged.states.size() == parsed.states.size());
  CHECK(merged.plugins.empty());
}

TEST_CASE("the hash changes when a plugin is added") {
  const oly::schema::Schema withoutPlugin = oly::schema::mergeSchemas({}, gameSource, "game.oly");

  const oly::schema::Schema withPlugin = oly::schema::mergeSchemas({&counting, 1}, gameSource, "game.oly");

  CHECK(withPlugin.hash != withoutPlugin.hash);
}

TEST_CASE("a name repeated across files names both locations") {
  CHECK_THROWS_WITH_AS(oly::schema::mergeSchemas({&counting, 1}, "component Counter { value: u32 }\n", "game.oly"), "game.oly:1:11: repeated name \"Counter\"; first declared at counting.oly:1:11", oly::schema::SchemaError);
}

TEST_CASE("an action in a plugin file is an error") {
  const oly::schema::PluginSource input{"input", "input.oly", "tag Pressed\naction jump: button\n"};

  CHECK_THROWS_WITH_AS(oly::schema::mergeSchemas({&input, 1}, "tag Player\n", "game.oly"), "input.oly:2:8: a plugin declares only components, tags and events; action \"jump\" is not allowed", oly::schema::SchemaError);
}

TEST_CASE("a state in a plugin file is an error") {
  const oly::schema::PluginSource modes{"modes", "modes.oly", "state menu\n"};

  CHECK_THROWS_WITH_AS(oly::schema::mergeSchemas({&modes, 1}, "tag Player\n", "game.oly"), "modes.oly:1:7: a plugin declares only components, tags and events; state \"menu\" is not allowed", oly::schema::SchemaError);
}

TEST_CASE("every merge corpus case matches the expectation of its game.oly") {
  const std::vector<std::filesystem::path> cases = entriesIn(corpus / "merge");
  REQUIRE(!cases.empty());

  for (const std::filesystem::path& folder : cases) {
    std::vector<oly::schema::PluginSource> plugins;
    for (const std::filesystem::path& file : entriesIn(folder)) {
      if (file.filename() != "game.oly") {
        plugins.push_back(pluginSourceOf(file));
      }
    }
    const std::string game = readText(folder / "game.oly");
    const std::string expectPrefix = "# expect: ";
    REQUIRE(game.rfind(expectPrefix, 0) == 0);
    const std::string expected = game.substr(expectPrefix.size(), game.find('\n') - expectPrefix.size());

    INFO(folder.filename().string());
    if (expected == "ok") {
      CHECK_NOTHROW(oly::schema::mergeSchemas(plugins, game, "game.oly"));
    } else {
      CHECK_THROWS_WITH_AS(oly::schema::mergeSchemas(plugins, game, "game.oly"), expected.c_str(), oly::schema::SchemaError);
    }
  }
}
