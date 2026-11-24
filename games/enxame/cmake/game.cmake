file(GLOB coreSources CONFIGURE_DEPENDS core/*.cpp)
file(GLOB gameSources CONFIGURE_DEPENDS native/*.cpp desktop/*.cpp)

add_library(enxame_core STATIC ${coreSources})
target_include_directories(enxame_core PUBLIC core)
target_link_libraries(enxame_core PUBLIC oly::ecs)
oly_warnings(enxame_core)

oly_add_game(enxame
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/enxame.oly
  SOURCES ${gameSources}
  DEPENDS ecs app input native_host)
target_include_directories(enxame PRIVATE native)
target_link_libraries(enxame PRIVATE enxame_core)

add_executable(enxame_unit tests/unit.cpp)
target_link_libraries(enxame_unit PRIVATE enxame_core)
oly_warnings(enxame_unit)

add_test(NAME enxame.unit COMMAND enxame_unit)
add_test(NAME enxame.headless COMMAND enxame --frames 120)
set_tests_properties(enxame.headless PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)
