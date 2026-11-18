file(GLOB gameSources CONFIGURE_DEPENDS native/*.cpp desktop/*.cpp)

oly_add_game(@GAME_TARGET@
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/@GAME_NAME@.oly
  SOURCES ${gameSources}
  DEPENDS ecs app input native_host)
target_include_directories(@GAME_TARGET@ PRIVATE native)

add_test(NAME @GAME_TARGET@.headless COMMAND @GAME_TARGET@ --frames 120)
set_tests_properties(@GAME_TARGET@.headless PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)
