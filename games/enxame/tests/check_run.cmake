# Runs COMMAND (a ; list), requires exit 0 and checks the JSON in OUT: scenario is SCENARIO, step.p50 exists and, when
# TIMING is on, systems has at least one entry.
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
