oly_add_plugin(NAME essentials3d SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/../essentials3d.oly DEPENDS ecs app core)

if(OLY_BUILD_TESTS)
  add_subdirectory(tests)
endif()

