# Runs COMMAND (a ; list) twice with --out set to OUT_a.json and OUT_b.json, and fails unless both runs exit 0 and write
# the same measures.worldHash.
foreach(run a b)
  execute_process(
    COMMAND ${COMMAND} --out ${OUT}_${run}.json
    RESULT_VARIABLE exitCode
    ERROR_VARIABLE errorText
    OUTPUT_QUIET)

  if(NOT exitCode EQUAL 0)
    message(FATAL_ERROR "run ${run}: expected exit code 0, got ${exitCode}; stderr: ${errorText}")
  endif()

  file(READ ${OUT}_${run}.json json)
  string(JSON hash_${run} GET "${json}" measures worldHash)
endforeach()

if(NOT hash_a STREQUAL hash_b)
  message(FATAL_ERROR "the world hash differs between two runs of the same input: ${hash_a} and ${hash_b}")
endif()
