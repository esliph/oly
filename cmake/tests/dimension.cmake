# cmake -DCASE=<case> -P dimension.cmake: runs oly_dimension_plugin on one DIMENSION and prints "plugin=<name>".
cmake_minimum_required(VERSION 3.25)
include(${CMAKE_CURRENT_LIST_DIR}/../OlyPlugin.cmake)

if(CASE STREQUAL "two_d")
  oly_dimension_plugin(plugin 2d)
elseif(CASE STREQUAL "three_d")
  oly_dimension_plugin(plugin 3d)
elseif(CASE STREQUAL "invalid")
  oly_dimension_plugin(plugin 4d)
else()
  message(FATAL_ERROR "unknown case ${CASE}")
endif()

message("plugin=${plugin}")
