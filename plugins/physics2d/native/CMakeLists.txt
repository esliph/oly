oly_add_plugin(NAME physics2d DEPENDS ecs app PLUGINS essentials2d)

if(OLY_BUILD_TESTS)
  add_subdirectory(tests)
endif()
