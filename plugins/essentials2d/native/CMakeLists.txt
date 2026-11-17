oly_add_plugin(NAME essentials2d SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/../essentials2d.oly DEPENDS ecs app core)

if(OLY_BUILD_TESTS)
  add_subdirectory(tests)
endif()
