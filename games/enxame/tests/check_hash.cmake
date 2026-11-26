# Runs COMMAND (a ; list), requires exit 0 and checks that measures.worldHash in OUT is HASH.
execute_process(
  COMMAND ${COMMAND}
  RESULT_VARIABLE exitCode
  ERROR_VARIABLE errorText
  OUTPUT_QUIET)

if(NOT exitCode EQUAL 0)
  message(FATAL_ERROR "expected exit code 0, got ${exitCode}; stderr: ${errorText}")
endif()

file(READ ${OUT} json)
string(JSON hash GET "${json}" measures worldHash)

if(NOT hash STREQUAL HASH)
  message(FATAL_ERROR "world hash of ${SCENARIO} is ${hash}, expected ${HASH}")
endif()
