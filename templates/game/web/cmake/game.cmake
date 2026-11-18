file(GLOB nativeSources CONFIGURE_DEPENDS native/*.cpp)

oly_add_game(@GAME_TARGET@
  ENVIRONMENT web
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/@GAME_NAME@.oly
  SOURCES ${nativeSources}
  DEPENDS ecs app capi
  TYPESCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/web/schema.generated.ts)

add_test(NAME @GAME_TARGET@.smoke COMMAND ${OLY_NODE} --test ${CMAKE_CURRENT_SOURCE_DIR}/web/dist/tests/)
set_tests_properties(@GAME_TARGET@.smoke PROPERTIES ENVIRONMENT OLY_GAME_MODULE=$<TARGET_FILE:@GAME_TARGET@>)
