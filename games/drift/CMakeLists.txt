if(OLY_ENVIRONMENT STREQUAL "native")
  file(GLOB gameSources CONFIGURE_DEPENDS native/*.cpp desktop/*.cpp)

  oly_add_game(drift_game
    ENVIRONMENT native
    DIMENSION 2d
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/drift.oly
    SOURCES ${gameSources}
    DEPENDS ecs app input native_host)
  target_include_directories(drift_game PRIVATE native)

  if(OLY_BUILD_TESTS)
    add_test(NAME drift.desktop COMMAND drift_game --frames 600)
    set_tests_properties(drift.desktop PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)
  endif()
endif()
