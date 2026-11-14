find_program(OLY_NODE node)
set(OLY_SCHEMA_TOOL ${PROJECT_SOURCE_DIR}/build/schema-tool/bin/oly-schema CACHE FILEPATH "oly-schema executable that WASM builds of oly_add_game run")

# oly_add_game(<target> ENVIRONMENT web|native SCHEMA <file.oly> SOURCES <cpp...> DEPENDS <m...>)
# Generates the schema structs and declaration with oly-schema and compiles the game with the modules. On web it also
# generates the registry and links the WASM module (.mjs) with the web host's script invoker; on native the sources
# bring their own main and link the native host through DEPENDS.
function(oly_add_game target)
  cmake_parse_arguments(GAME "" "ENVIRONMENT;SCHEMA" "SOURCES;DEPENDS" ${ARGN})

  if(NOT GAME_ENVIRONMENT)
    message(FATAL_ERROR "${target}: oly_add_game needs ENVIRONMENT web|native")
  endif()
  if(NOT GAME_ENVIRONMENT STREQUAL OLY_ENVIRONMENT)
    message(FATAL_ERROR "${target} is a ${GAME_ENVIRONMENT} game, but this build is ${OLY_ENVIRONMENT}")
  endif()

  if(TARGET oly-schema)
    # A target COMMAND gets CMAKE_CROSSCOMPILING_EMULATOR, so the Windows build runs oly-schema.exe under Wine.
    set(schemaTool oly-schema)
    set(schemaToolDependency oly-schema)
  else()
    if(NOT EXISTS ${OLY_SCHEMA_TOOL})
      message(FATAL_ERROR "oly-schema not found at ${OLY_SCHEMA_TOOL}: run './spur schema-tool' (or './spur ts') before configuring with OLY_BUILD_GAMES=ON")
    endif()
    set(schemaTool ${OLY_SCHEMA_TOOL})
    set(schemaToolDependency ${OLY_SCHEMA_TOOL})
  endif()

  set(generatedDirectory ${CMAKE_CURRENT_BINARY_DIR}/${target}_generated)
  set(generatedHeader ${generatedDirectory}/schema.generated.hpp)
  set(generatedDeclaration ${generatedDirectory}/schema_declaration.generated.cpp)
  set(generatedRegistry ${generatedDirectory}/schema_registry.generated.cpp)
  add_custom_command(
    OUTPUT ${generatedHeader} ${generatedDeclaration} ${generatedRegistry}
    COMMAND ${schemaTool} ${GAME_SCHEMA} --emit cpp=${generatedDirectory}
    DEPENDS ${GAME_SCHEMA} ${schemaToolDependency}
    COMMENT "Generating the C++ schema for ${target}"
    VERBATIM)

  add_executable(${target} ${GAME_SOURCES} ${generatedDeclaration} ${generatedHeader})
  target_include_directories(${target} PRIVATE ${generatedDirectory})
  foreach(dependency IN LISTS GAME_DEPENDS)
    target_link_libraries(${target} PRIVATE oly::${dependency})
  endforeach()
  oly_warnings(${target})
  set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/games)

  if(GAME_ENVIRONMENT STREQUAL "web")
    target_sources(${target} PRIVATE ${generatedRegistry})
    set_target_properties(${target} PROPERTIES SUFFIX ".mjs")
    target_link_libraries(${target} PRIVATE oly::web_host)
    # EXIT_RUNTIME=0 overrides the global =1: TS calls the module after it loads, so the runtime must not exit.
    target_link_options(${target} PRIVATE
      -sMODULARIZE=1 -sEXPORT_ES6=1 -sENVIRONMENT=node,web
      -sEXPORTED_FUNCTIONS=_malloc,_free -sEXPORTED_RUNTIME_METHODS=HEAPU8 -sEXIT_RUNTIME=0)
  endif()

  oly_register_target(OLY_GAMES ${target} ${CMAKE_CURRENT_SOURCE_DIR} ${GAME_DEPENDS})
endfunction()
