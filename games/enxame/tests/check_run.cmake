# Runs COMMAND (a ; list), requires exit 0 and checks the JSON in OUT: scenario is SCENARIO, step.p50 exists and, when
# TIMING is on, systems has at least one entry; and the measures of S2, S3, S6 and S8 exist (S8: two waves).
execute_process(
  COMMAND ${COMMAND}
  RESULT_VARIABLE exitCode
  ERROR_VARIABLE errorText
  OUTPUT_QUIET)

if(NOT exitCode EQUAL 0)
  message(FATAL_ERROR "expected exit code 0, got ${exitCode}; stderr: ${errorText}")
endif()

file(READ ${OUT} json)
string(JSON scenario GET "${json}" scenario)

if(NOT scenario STREQUAL SCENARIO)
  message(FATAL_ERROR "scenario is ${scenario}, expected ${SCENARIO}")
endif()

string(JSON median GET "${json}" step p50)

if(TIMING STREQUAL on)
  string(JSON systemCount LENGTH "${json}" systems)

  if(systemCount LESS 1)
    message(FATAL_ERROR "timing is on and systems is empty")
  endif()
endif()

if(SCENARIO STREQUAL S2)
  string(JSON allocationTotal GET "${json}" measures allocationsPerStep total)
endif()

if(SCENARIO STREQUAL S3)
  string(JSON transitionMedian GET "${json}" measures transitionsPerStep p50)
endif()

if(SCENARIO STREQUAL S6)
  string(JSON visitedEntities GET "${json}" measures visitedEntities)
endif()

if(SCENARIO STREQUAL S8)
  string(JSON waveCount LENGTH "${json}" measures waves)

  if(NOT waveCount EQUAL 2)
    message(FATAL_ERROR "S8 with 120 steps should record 2 waves, got ${waveCount}")
  endif()
endif()
