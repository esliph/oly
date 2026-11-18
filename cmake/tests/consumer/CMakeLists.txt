cmake_minimum_required(VERSION 3.25)
project(OlyConsumer LANGUAGES CXX C)

# A game enables testing for its own tests; the check on the engine's tests depends on ctest looking here.
enable_testing()

include(FetchContent)
# Never fetched: ./spur subproject-test sets FETCHCONTENT_SOURCE_DIR_OLY to the checkout.
FetchContent_Declare(oly
  GIT_REPOSITORY https://github.com/esliph/oly.git
  GIT_TAG v0.0.0)
FetchContent_MakeAvailable(oly)

if(OLY_ENVIRONMENT STREQUAL "web")
  oly_add_game(consumer
    ENVIRONMENT web
    DIMENSION 2d
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/consumer.oly
    DEPENDS ecs app capi)
elseif(OLY_ENVIRONMENT STREQUAL "native")
  oly_add_game(consumer
    ENVIRONMENT native
    DIMENSION 2d
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/consumer.oly
    SOURCES native/main.cpp
    DEPENDS ecs app)
endif()
