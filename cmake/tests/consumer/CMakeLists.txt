cmake_minimum_required(VERSION 3.25)
project(OlyConsumer LANGUAGES CXX C)

include(FetchContent)
# Never fetched: ./spur subproject-test sets FETCHCONTENT_SOURCE_DIR_OLY to the checkout.
FetchContent_Declare(oly
  GIT_REPOSITORY https://github.com/esliph/oly.git
  GIT_TAG v0.0.0)
FetchContent_MakeAvailable(oly)

if(EMSCRIPTEN)
  oly_add_game(consumer
    ENVIRONMENT web
    DIMENSION 2d
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/consumer.oly
    DEPENDS ecs app capi)
else()
  oly_add_game(consumer
    ENVIRONMENT native
    DIMENSION 2d
    SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/consumer.oly
    SOURCES native/main.cpp
    DEPENDS ecs app)
endif()
