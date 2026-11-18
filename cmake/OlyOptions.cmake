set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OLY_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(OLY_BUILD_BENCH "Build the benchmarks" OFF)
option(OLY_SANITIZE "ASan + UBSan" OFF)
option(OLY_WERROR "Warnings become errors" ${PROJECT_IS_TOP_LEVEL})
option(OLY_VEC_REPORT "Vectorization report in the benchmarks" OFF)
option(OLY_BUILD_GAMES "Build the games (requires ./spur ts first)" OFF)
option(OLY_BUILD_NATIVE_HOST "Build the native host (fetches SDL3)" OFF)
option(OLY_ALLOW_UNTESTED_TOOLCHAIN "An Emscripten that is not the tested one only warns" OFF)
set(OLY_ENABLE_ASSERTS "" CACHE STRING "0/1; empty = 0 with NDEBUG, otherwise 1")

# oly_environment(<outputVariable>): the environment this build runs in, web under Emscripten and native otherwise. A game
# calls it from its own directory scope, which has none of the engine's variables.
function(oly_environment outputVariable)
  if(EMSCRIPTEN)
    set(${outputVariable} web PARENT_SCOPE)
  else()
    set(${outputVariable} native PARENT_SCOPE)
  endif()
endfunction()

oly_environment(OLY_ENVIRONMENT)
# A cache entry reaches the directory of a game that fetched the engine; a normal variable stays in the engine's scope.
set(OLY_ENGINE_IS_TOP_LEVEL ${PROJECT_IS_TOP_LEVEL} CACHE INTERNAL "Whether the engine is the top-level project; a game reads it from its own scope")
set(OLY_ENVIRONMENT "${OLY_ENVIRONMENT}" CACHE INTERNAL "The environment this build targets")

if(OLY_BUILD_NATIVE_HOST AND EMSCRIPTEN)
  message(FATAL_ERROR "OLY_BUILD_NATIVE_HOST is for native builds, not for Emscripten")
endif()

if(OLY_BUILD_GAMES AND OLY_ENVIRONMENT STREQUAL "native" AND NOT OLY_BUILD_NATIVE_HOST)
  message(FATAL_ERROR "OLY_BUILD_GAMES on a native build requires OLY_BUILD_NATIVE_HOST")
endif()

if(EMSCRIPTEN)
  # ALLOW_MEMORY_GROWTH: the benchmarks create 1M entities. EXIT_RUNTIME=1: without it ctest does not see the exit
  # code of the program running in node.
  add_link_options(-sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=1)
endif()

if(OLY_ENABLE_ASSERTS STREQUAL "")
  if(CMAKE_BUILD_TYPE MATCHES "^(Release|RelWithDebInfo|MinSizeRel)$")
    set(OLY_ASSERTS_ENABLED 0)
  else()
    set(OLY_ASSERTS_ENABLED 1)
  endif()
else()
  set(OLY_ASSERTS_ENABLED ${OLY_ENABLE_ASSERTS})
endif()

if(OLY_SANITIZE AND PROJECT_IS_TOP_LEVEL)
  # Global: the dependencies are instrumented too, avoiding mixed runtimes. Ignored in a subproject: the same flags would
  # instrument the whole build of the game that fetched the engine.
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
