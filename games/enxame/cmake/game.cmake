file(GLOB coreSources CONFIGURE_DEPENDS core/*.cpp)
file(GLOB gameSources CONFIGURE_DEPENDS native/*.cpp desktop/*.cpp)

add_library(enxame_core STATIC ${coreSources})
target_include_directories(enxame_core PUBLIC core)
target_link_libraries(enxame_core PUBLIC oly::ecs)
oly_warnings(enxame_core)

oly_add_game(enxame
  ENVIRONMENT native
  DIMENSION 2d
  SCHEMA ${CMAKE_CURRENT_SOURCE_DIR}/enxame.oly
  SOURCES ${gameSources}
  DEPENDS ecs app input native_host)
target_include_directories(enxame PRIVATE native native/workloads desktop)
target_link_libraries(enxame PRIVATE enxame_core)

add_executable(enxame_unit tests/unit.cpp)
target_link_libraries(enxame_unit PRIVATE enxame_core)
oly_warnings(enxame_unit)

add_test(NAME enxame.unit COMMAND enxame_unit)
add_test(NAME enxame.headless COMMAND enxame --frames 120)
set_tests_properties(enxame.headless PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)

# enxame_expect_exit(<name> <exit code> <stderr text> <arguments...>)
function(enxame_expect_exit name exitCode text)
  add_test(NAME enxame.usage.${name}
    COMMAND ${CMAKE_COMMAND} "-DCOMMAND=$<TARGET_FILE:enxame>;${ARGN}" -DEXIT_CODE=${exitCode} "-DTEXT=${text}"
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/expect_exit.cmake)
endfunction()

enxame_expect_exit(size_not_frozen 64 "is not one of S1's" --headless --scenario S1 --size 7 --commit test --out x.json)
enxame_expect_exit(missing_out 64 "needs --out" --headless --scenario S1 --size 1000 --commit test)
enxame_expect_exit(missing_commit 64 "needs --commit" --headless --scenario S1 --size 1000 --out x.json)
enxame_expect_exit(unknown_scenario 64 "unknown scenario S9" --headless --scenario S9 --size 1 --commit test --out x.json)
enxame_expect_exit(bad_commit 64 "--commit must match" --headless --scenario S1 --size 1000 --commit a/b --out x.json)
enxame_expect_exit(blocked_s4 64 "blocked: needs stable ids" --headless --scenario S4 --size 1 --commit test --out x.json)
enxame_expect_exit(blocked_s7 64 "blocked: needs assets" --headless --scenario S7 --size 1 --commit test --out x.json)
enxame_expect_exit(unwritable_out 1 "cannot write" --headless --scenario S1 --size 1000 --steps 3 --commit test --out missing/folder/x.json)

# enxame_check_run(<name> <scenario> <timing on|off> <arguments...>)
function(enxame_check_run name scenario timing)
  set(out ${CMAKE_CURRENT_BINARY_DIR}/run_${name}.json)
  add_test(NAME enxame.run.${name}
    COMMAND ${CMAKE_COMMAND} "-DCOMMAND=$<TARGET_FILE:enxame>;${ARGN};--out;${out}" -DOUT=${out} -DSCENARIO=${scenario} -DTIMING=${timing}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_run.cmake)
endfunction()

enxame_check_run(S1 S1 on --headless --scenario S1 --size 1000 --steps 5 --commit test)
enxame_check_run(S1_untimed S1 off --headless --scenario S1 --size 1000 --steps 5 --commit test --timing off)
enxame_check_run(S5 S5 on --headless --scenario S5 --size 2000 --steps 5 --commit test)
enxame_check_run(S2 S2 on --headless --scenario S2 --size 50000 --steps 5 --commit test)
enxame_check_run(S3 S3 on --headless --scenario S3 --size 10000 --steps 5 --commit test)
enxame_check_run(S6 S6 on --headless --scenario S6 --size 1000 --steps 5 --commit test)
enxame_check_run(S8 S8 on --headless --scenario S8 --size 2000 --steps 120 --commit test)

add_test(NAME enxame.replay_determinism
  COMMAND ${CMAKE_COMMAND} "-DCOMMAND=$<TARGET_FILE:enxame>;--headless;--scenario;S5;--size;2000;--steps;2500;--commit;test"
          -DOUT=${CMAKE_CURRENT_BINARY_DIR}/replay -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/replay_determinism.cmake)
