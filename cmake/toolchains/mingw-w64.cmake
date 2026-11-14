set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_CROSSCOMPILING_EMULATOR wine)

# Static, so the .exe runs on a machine with no MinGW runtime next to it.
set(CMAKE_EXE_LINKER_FLAGS_INIT -static)

# msvcrt's printf does not know %zu; this selects MinGW's C99 printf, which OLY_ASSERT messages rely on.
set(CMAKE_C_FLAGS_INIT -D__USE_MINGW_ANSI_STDIO=1)
set(CMAKE_CXX_FLAGS_INIT -D__USE_MINGW_ANSI_STDIO=1)
