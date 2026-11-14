oly_add_module(NAME native_host DEPENDS app input ecs core)
target_link_libraries(oly_native_host PUBLIC SDL3::SDL3-static)

if(OLY_BUILD_TESTS)
  add_subdirectory(tests)
endif()
