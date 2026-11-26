add_subdirectory(fixture/plugins/counting)
add_subdirectory(fixture/plugins/doubling)

if(OLY_ENVIRONMENT STREQUAL "web")
  oly_add_engine_test_game(fixture_game
    ENVIRONMENT web
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/fixture/fixture.oly
    SOURCES fixture/systems.cpp
    DEPENDS ecs app capi input
    PLUGINS doubling
    TYPESCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/fixture/fixture.generated.ts)

  if(OLY_BUILD_TESTS)
    add_test(NAME ts.testing.wasm COMMAND ${OLY_NODE} --test ${PROJECT_SOURCE_DIR}/engine/testing/dist/tests/wasm/)
    set_tests_properties(ts.testing.wasm PROPERTIES ENVIRONMENT OLY_FIXTURE_MODULE=$<TARGET_FILE:fixture_game>)
  endif()
else()
  add_subdirectory(fixture/plugins/native)
endif()
