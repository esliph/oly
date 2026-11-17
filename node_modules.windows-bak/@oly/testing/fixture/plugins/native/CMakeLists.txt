oly_add_engine_test_game(plugin_fixture_game
  ENVIRONMENT native
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/plugin_fixture.oly
  SOURCES plugins_test.cpp ${PROJECT_SOURCE_DIR}/engine/runtime/core/tests/support/doctest_main.cpp
  DEPENDS ecs app
  PLUGINS doubling)
target_link_libraries(plugin_fixture_game PRIVATE doctest::doctest)

add_test(NAME fixture.plugins COMMAND plugin_fixture_game)
