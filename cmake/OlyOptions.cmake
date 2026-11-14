set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OLY_BUILD_TESTS "Build the tests" ON)
option(OLY_BUILD_BENCH "Build the benchmarks" OFF)
option(OLY_SANITIZE "ASan + UBSan" OFF)
option(OLY_WERROR "Warnings become errors" ON)
option(OLY_VEC_REPORT "Vectorization report in the benchmarks" OFF)
option(OLY_BUILD_GAMES "Build the games (requires ./spur ts first)" OFF)
option(OLY_BUILD_NATIVE_HOST "Build the native host (fetches SDL3)" OFF)
set(OLY_ENABLE_ASSERTS "" CACHE STRING "0/1; empty = 0 with NDEBUG, otherwise 1")

if(EMSCRIPTEN)
  set(OLY_ENVIRONMENT web)
else()
  set(OLY_ENVIRONMENT native)
endif()

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

if(OLY_SANITIZE)
  # Global: the dependencies are instrumented too, avoiding mixed runtimes.
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
