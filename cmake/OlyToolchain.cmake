# The toolchain check against toolchain.env. This file only defines functions, so cmake/tests/toolchain.cmake can include
# it in script mode.

set(_olyToolchainKeys EMSDK_VERSION GCC_VERSION GCC_MINIMUM MINGW_GCC_MINIMUM CMAKE_VERSION CMAKE_MINIMUM NINJA_MINIMUM)

# oly_read_toolchain(<file>): sets OLY_TOOLCHAIN_<KEY> in the caller's scope for each KEY=VALUE line of the file.
function(oly_read_toolchain file)
  file(STRINGS ${file} lines)

  foreach(line IN LISTS lines)
    if(line MATCHES "^([A-Z_]+)=(.*)$")
      set(OLY_TOOLCHAIN_${CMAKE_MATCH_1} ${CMAKE_MATCH_2})
    endif()
  endforeach()

  foreach(key IN LISTS _olyToolchainKeys)
    if(NOT DEFINED OLY_TOOLCHAIN_${key})
      message(FATAL_ERROR "${file} has no ${key} (toolchain.env)")
    endif()

    set(OLY_TOOLCHAIN_${key} ${OLY_TOOLCHAIN_${key}} PARENT_SCOPE)
  endforeach()
endfunction()

# oly_check_toolchain(FILE <file> COMPILER_ID <id> COMPILER_VERSION <version> CMAKE_VERSION <version>
#                     [EMSCRIPTEN_VERSION <version>] [TARGET_WINDOWS] [NINJA_VERSION <version>] [ALLOW_UNTESTED])
# Reads every input from its arguments, so a script-mode test drives it. Emscripten must equal EMSDK_VERSION (a warning
# with ALLOW_UNTESTED); GCC, MinGW GCC, CMake and Ninja are minimums and always errors.
function(oly_check_toolchain)
  cmake_parse_arguments(CHECK "TARGET_WINDOWS;ALLOW_UNTESTED"
    "FILE;COMPILER_ID;COMPILER_VERSION;CMAKE_VERSION;EMSCRIPTEN_VERSION;NINJA_VERSION" "" ${ARGN})
  oly_read_toolchain(${CHECK_FILE})

  if(CHECK_CMAKE_VERSION VERSION_LESS OLY_TOOLCHAIN_CMAKE_MINIMUM)
    message(FATAL_ERROR "CMake ${CHECK_CMAKE_VERSION} is older than the minimum ${OLY_TOOLCHAIN_CMAKE_MINIMUM} (toolchain.env)")
  endif()

  if(CHECK_EMSCRIPTEN_VERSION)
    if(NOT CHECK_EMSCRIPTEN_VERSION VERSION_EQUAL OLY_TOOLCHAIN_EMSDK_VERSION)
      set(untested "Emscripten ${CHECK_EMSCRIPTEN_VERSION} is not the tested version ${OLY_TOOLCHAIN_EMSDK_VERSION}")

      if(CHECK_ALLOW_UNTESTED)
        message(WARNING "untested ${untested} (toolchain.env)")
      else()
        message(FATAL_ERROR "${untested} (toolchain.env): install emsdk ${OLY_TOOLCHAIN_EMSDK_VERSION}, or configure with -DOLY_ALLOW_UNTESTED_TOOLCHAIN=ON")
      endif()
    endif()
  elseif(CHECK_COMPILER_ID STREQUAL "GNU")
    if(CHECK_TARGET_WINDOWS)
      set(compilerName "MinGW GCC")
      set(compilerMinimum ${OLY_TOOLCHAIN_MINGW_GCC_MINIMUM})
    else()
      set(compilerName "GCC")
      set(compilerMinimum ${OLY_TOOLCHAIN_GCC_MINIMUM})
    endif()

    if(CHECK_COMPILER_VERSION VERSION_LESS compilerMinimum)
      message(FATAL_ERROR "${compilerName} ${CHECK_COMPILER_VERSION} is older than the minimum ${compilerMinimum} (toolchain.env)")
    endif()
  endif()

  if(CHECK_NINJA_VERSION AND CHECK_NINJA_VERSION VERSION_LESS OLY_TOOLCHAIN_NINJA_MINIMUM)
    message(FATAL_ERROR "Ninja ${CHECK_NINJA_VERSION} is older than the minimum ${OLY_TOOLCHAIN_NINJA_MINIMUM} (toolchain.env)")
  endif()
endfunction()

# oly_check_build_toolchain(): oly_check_toolchain on the toolchain this configure uses, against the toolchain.env of the
# engine.
function(oly_check_build_toolchain)
  set(arguments
    FILE ${PROJECT_SOURCE_DIR}/toolchain.env
    COMPILER_ID ${CMAKE_CXX_COMPILER_ID}
    COMPILER_VERSION ${CMAKE_CXX_COMPILER_VERSION}
    CMAKE_VERSION ${CMAKE_VERSION})

  if(EMSCRIPTEN)
    list(APPEND arguments EMSCRIPTEN_VERSION ${EMSCRIPTEN_VERSION})
  endif()

  if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    list(APPEND arguments TARGET_WINDOWS)
  endif()

  if(OLY_ALLOW_UNTESTED_TOOLCHAIN)
    list(APPEND arguments ALLOW_UNTESTED)
  endif()

  if(CMAKE_GENERATOR MATCHES "Ninja")
    execute_process(COMMAND ${CMAKE_MAKE_PROGRAM} --version OUTPUT_VARIABLE ninjaVersion OUTPUT_STRIP_TRAILING_WHITESPACE)
    string(REGEX MATCH "^[^\r\n]*" ninjaVersion "${ninjaVersion}")
    list(APPEND arguments NINJA_VERSION ${ninjaVersion})
  endif()

  set_property(DIRECTORY ${PROJECT_SOURCE_DIR} APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/toolchain.env)
  oly_check_toolchain(${arguments})
endfunction()
