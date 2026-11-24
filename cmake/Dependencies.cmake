include(FetchContent)

if(OLY_BUILD_TESTS)
  set(DOCTEST_WITH_TESTS OFF CACHE BOOL "" FORCE)
  set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
    SYSTEM)
  FetchContent_MakeAvailable(doctest)
endif()

if(OLY_BUILD_BENCH)
  set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
  set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
    SYSTEM)
  FetchContent_MakeAvailable(benchmark)
endif()

if(OLY_BUILD_NATIVE_HOST)
  # SDL ignores a misspelled option without a warning: check the configure summary when changing this list.
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)

  foreach(subsystem AUDIO CAMERA JOYSTICK HAPTIC HIDAPI SENSOR POWER)
    set(SDL_${subsystem} OFF CACHE BOOL "" FORCE)
  endforeach()

  FetchContent_Declare(SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.30
    SYSTEM)
  FetchContent_MakeAvailable(SDL3)
endif()
