# Runs COMMAND (a ; list) and fails unless the exit code is EXIT_CODE and stderr contains TEXT.
execute_process(
  COMMAND ${COMMAND}
  RESULT_VARIABLE exitCode
  ERROR_VARIABLE errorText
  OUTPUT_QUIET)

if(NOT exitCode EQUAL EXIT_CODE)
  message(FATAL_ERROR "expected exit code ${EXIT_CODE}, got ${exitCode}; stderr: ${errorText}")
endif()

string(FIND "${errorText}" "${TEXT}" position)

if(position EQUAL -1)
  message(FATAL_ERROR "stderr does not contain \"${TEXT}\"; stderr: ${errorText}")
endif()
