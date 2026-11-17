cmake_minimum_required(VERSION 3.25)
project(Oly VERSION 1.0 LANGUAGES CXX C)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(OlyOptions)
include(OlyWarnings)
include(OlyWasm)
include(Dependencies)
include(OlyModule)
include(OlyPlugin)
include(OlyGame)

enable_testing()

if(OLY_BUILD_TESTS)
  set(pluginOrderCases
    "independent_keep_list_order|order=\\[a,b\\]"
    "dependency_first|order=\\[a,b\\]"
    "dependency_listed_after|order=\\[a,b\\]"
    "transitive|order=\\[a,b,c\\]"
    "shared_dependency_once|order=\\[a,b,c\\]"
    "cycle|plugin cycle: a -> b -> a"
    "unknown|unknown plugin z")
  foreach(pluginOrderCase IN LISTS pluginOrderCases)
    string(REPLACE "|" ";" fields "${pluginOrderCase}")
    list(GET fields 0 caseName)
    list(GET fields 1 expected)
    add_test(NAME oly.cmake.plugin_order.${caseName}
      COMMAND ${CMAKE_COMMAND} -DCASE=${caseName} -P ${PROJECT_SOURCE_DIR}/cmake/tests/plugin_order.cmake)
    set_tests_properties(oly.cmake.plugin_order.${caseName} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}")
  endforeach()
endif()
add_subdirectory(engine)
if(OLY_BUILD_GAMES)
  add_subdirectory(games)
endif()

# Boundary manifest for oly.lint.boundaries: modules, plugins and games with their DEPENDS.
function(oly_manifest_entries property outputVariable)
  get_property(entries GLOBAL PROPERTY ${property})
  set(json "")
  foreach(entry IN LISTS entries)
    string(REPLACE "|" ";" fields "${entry}")
    list(GET fields 0 name)
    list(GET fields 1 directory)
    list(GET fields 2 dependencies)
    if(dependencies STREQUAL "")
      set(dependencyArray "")
    else()
      string(REPLACE "," "\", \"" dependencyArray "\"${dependencies}\"")
    endif()
    list(APPEND json "{\"name\": \"${name}\", \"directory\": \"${directory}\", \"depends\": [${dependencyArray}]}")
  endforeach()
  list(JOIN json ", " joined)
  set(${outputVariable} "${joined}" PARENT_SCOPE)
endfunction()

oly_manifest_entries(OLY_MODULES moduleEntries)
oly_manifest_entries(OLY_PLUGINS pluginEntries)
oly_manifest_entries(OLY_GAMES gameEntries)
file(WRITE ${CMAKE_BINARY_DIR}/oly_boundaries.json "{\"modules\": [${moduleEntries}], \"plugins\": [${pluginEntries}], \"games\": [${gameEntries}]}\n")

if(NOT EMSCRIPTEN OR OLY_BUILD_GAMES)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(NAME oly.lint.boundaries
      COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/check_boundaries.py
              --root ${PROJECT_SOURCE_DIR} --manifest ${CMAKE_BINARY_DIR}/oly_boundaries.json)
    add_test(NAME oly.lint.boundaries_selftest
      COMMAND ${Python3_EXECUTABLE} -I ${PROJECT_SOURCE_DIR}/tools/test_check_boundaries.py)
  endif()
endif()
