cmake_minimum_required(VERSION 3.25)
project(@GAME_TARGET@ LANGUAGES CXX C)

# oly.env is the one place the version is written, so a bare cmake run and an IDE see what oly sees.
set(olyEnvironmentFile ${CMAKE_CURRENT_SOURCE_DIR}/oly.env)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${olyEnvironmentFile})
file(STRINGS ${olyEnvironmentFile} olyVersionLines REGEX "^OLY_VERSION=")

if(NOT olyVersionLines)
  message(FATAL_ERROR "oly.env has no OLY_VERSION")
endif()
list(GET olyVersionLines -1 olyVersionLine)
string(REGEX REPLACE "^OLY_VERSION=" "" OLY_VERSION "${olyVersionLine}")
string(STRIP "${OLY_VERSION}" OLY_VERSION)

enable_testing()

include(FetchContent)
FetchContent_Declare(oly
  GIT_REPOSITORY https://github.com/esliph/oly.git
  GIT_TAG v${OLY_VERSION}
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(oly)

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/game.cmake)
