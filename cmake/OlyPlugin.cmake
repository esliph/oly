# Plugin convention: target oly_<name> (alias oly::<name>), namespace oly::<name>, class oly::<name>::Plugin declared in
# include/oly/<name>/plugin.hpp, generated header oly/<name>/schema.generated.hpp. This file only defines functions, so
# cmake/tests/plugin_order.cmake can include it in script mode.

# oly_schema_tool(<toolVariable> <dependencyVariable>): the oly-schema a custom command runs, and what it depends on.
function(oly_schema_tool toolVariable dependencyVariable)
  if(TARGET oly-schema)
    # A target COMMAND gets CMAKE_CROSSCOMPILING_EMULATOR, so the Windows build runs oly-schema.exe under Wine.
    set(${toolVariable} oly-schema PARENT_SCOPE)
    set(${dependencyVariable} oly-schema PARENT_SCOPE)
    return()
  endif()

  if(NOT EXISTS ${OLY_SCHEMA_TOOL})
    if(OLY_ENGINE_IS_TOP_LEVEL)
      message(FATAL_ERROR "oly-schema not found at ${OLY_SCHEMA_TOOL}: run './spur schema-tool' (or './spur ts') before configuring with OLY_BUILD_GAMES=ON")
    endif()

    message(FATAL_ERROR "oly-schema not found at ${OLY_SCHEMA_TOOL}: pass -DOLY_SCHEMA_TOOL=<path to oly-schema>")
  endif()

  set(${toolVariable} ${OLY_SCHEMA_TOOL} PARENT_SCOPE)
  set(${dependencyVariable} ${OLY_SCHEMA_TOOL} PARENT_SCOPE)
endfunction()

# oly_add_plugin(NAME <p> [SCHEMA <file.oly>] DEPENDS <modules...> [PLUGINS <plugins...>])
# A static library of src/*.cpp. With SCHEMA, oly-schema generates the plugin's own header from its .oly, and the .oly
# is recorded so that oly_add_game merges it into each game that turns the plugin on.
function(oly_add_plugin)
  cmake_parse_arguments(PLUGIN "" "NAME;SCHEMA" "DEPENDS;PLUGINS" ${ARGN})
  set(name ${PLUGIN_NAME})
  set(target oly_${name})

  file(GLOB pluginSources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

  if(NOT pluginSources)
    message(FATAL_ERROR "plugin ${name}: no src/*.cpp; a plugin defines oly::${name}::Plugin")
  endif()

  add_library(${target} STATIC ${pluginSources})
  add_library(oly::${name} ALIAS ${target})
  target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(${target} PUBLIC cxx_std_20)
  oly_warnings(${target})

  foreach(dependency IN LISTS PLUGIN_DEPENDS PLUGIN_PLUGINS)
    target_link_libraries(${target} PUBLIC oly::${dependency})
  endforeach()

  set(schemaFile "")

  if(PLUGIN_SCHEMA)
    cmake_path(ABSOLUTE_PATH PLUGIN_SCHEMA BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} OUTPUT_VARIABLE schemaFile)
    oly_schema_tool(schemaTool schemaToolDependency)
    set(generatedRoot ${CMAKE_CURRENT_BINARY_DIR}/${target}_generated)
    set(generatedHeader ${generatedRoot}/oly/${name}/schema.generated.hpp)
    add_custom_command(
      OUTPUT ${generatedHeader}
      COMMAND ${schemaTool} ${schemaFile} --emit cpp-plugin=${generatedRoot}/oly/${name}
      DEPENDS ${schemaFile} ${schemaToolDependency}
      COMMENT "Generating the C++ schema of plugin ${name}"
      VERBATIM)
    target_sources(${target} PRIVATE ${generatedHeader})
    target_include_directories(${target} PUBLIC ${generatedRoot})
  endif()

  set_property(GLOBAL PROPERTY OLY_PLUGIN_SCHEMA_${name} "${schemaFile}")
  set_property(GLOBAL PROPERTY OLY_PLUGIN_DEPENDENCIES_${name} "${PLUGIN_PLUGINS}")
  oly_register_target(OLY_PLUGINS ${name} ${CMAKE_CURRENT_SOURCE_DIR} ${PLUGIN_DEPENDS} ${PLUGIN_PLUGINS})
endfunction()

# Depth-first: the dependencies of `plugin` go into _olyPluginOrder before it. `chain` is the path from the listed
# plugin, which names the cycle when the walk meets a plugin already on it.
function(_oly_plugin_visit plugin chain)
  if(plugin IN_LIST _olyPluginOrder)
    return()
  endif()

  if(plugin IN_LIST chain)
    list(FIND chain ${plugin} cycleStart)
    list(SUBLIST chain ${cycleStart} -1 cycle)
    list(APPEND cycle ${plugin})
    list(JOIN cycle " -> " cycleText)
    message(FATAL_ERROR "plugin cycle: ${cycleText}")
  endif()

  get_property(known GLOBAL PROPERTY OLY_PLUGIN_DEPENDENCIES_${plugin} SET)

  if(NOT known)
    message(FATAL_ERROR "unknown plugin ${plugin}")
  endif()

  get_property(dependencies GLOBAL PROPERTY OLY_PLUGIN_DEPENDENCIES_${plugin})
  set(dependencyChain ${chain} ${plugin})

  foreach(dependency IN LISTS dependencies)
    _oly_plugin_visit(${dependency} "${dependencyChain}")
  endforeach()

  list(APPEND _olyPluginOrder ${plugin})
  set(_olyPluginOrder "${_olyPluginOrder}" PARENT_SCOPE)
endfunction()

# oly_plugin_order(<outputVariable> <plugins...>): the install order of the plugins and their transitive dependencies,
# each once, dependencies first; among independent plugins, the order of the list.
function(oly_plugin_order outputVariable)
  set(_olyPluginOrder "")

  foreach(plugin IN LISTS ARGN)
    _oly_plugin_visit(${plugin} "")
  endforeach()

  set(${outputVariable} "${_olyPluginOrder}" PARENT_SCOPE)
endfunction()

# oly_dimension_plugin(<outputVariable> <dimension>): the essentials plugin of a game's DIMENSION.
function(oly_dimension_plugin outputVariable dimension)
  if(dimension STREQUAL "2d")
    set(${outputVariable} essentials2d PARENT_SCOPE)
  elseif(dimension STREQUAL "3d")
    set(${outputVariable} essentials3d PARENT_SCOPE)
  else()
    message(FATAL_ERROR "${dimension}: DIMENSION is 2d or 3d")
  endif()
endfunction()
