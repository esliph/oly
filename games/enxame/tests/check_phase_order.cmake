# Runs COMMAND (a ; list), requires exit 0 and checks the JSON in OUT: flushes[].phase is the phase order of a game with
# essentials2d and physics2d, and fireTurrets and moveProjectiles run in UpdateAfter, after the physics.
execute_process(
  COMMAND ${COMMAND}
  RESULT_VARIABLE exitCode
  ERROR_VARIABLE errorText
  OUTPUT_QUIET)

if(NOT exitCode EQUAL 0)
  message(FATAL_ERROR "expected exit code 0, got ${exitCode}; stderr: ${errorText}")
endif()

file(READ ${OUT} json)
set(expectedPhases oly::app::UpdateBefore oly::app::Update oly::physics2d::Physics oly::essentials2d::Propagation oly::app::UpdateAfter oly::app::Render)
list(LENGTH expectedPhases expectedCount)
string(JSON flushCount LENGTH "${json}" flushes)

if(NOT flushCount EQUAL expectedCount)
  message(FATAL_ERROR "expected ${expectedCount} flushes, got ${flushCount}")
endif()

set(actualPhases "")
math(EXPR lastFlush "${flushCount} - 1")

foreach(slot RANGE ${lastFlush})
  string(JSON phase GET "${json}" flushes ${slot} phase)
  list(APPEND actualPhases ${phase})
endforeach()

if(NOT actualPhases STREQUAL expectedPhases)
  message(FATAL_ERROR "phases are ${actualPhases}, expected ${expectedPhases}")
endif()

string(JSON systemCount LENGTH "${json}" systems)
math(EXPR lastSystem "${systemCount} - 1")
set(combatSystemsSeen 0)

foreach(slot RANGE ${lastSystem})
  string(JSON name GET "${json}" systems ${slot} name)
  string(JSON phase GET "${json}" systems ${slot} phase)

  if((name STREQUAL "fireTurrets" OR name STREQUAL "moveProjectiles") AND NOT phase STREQUAL "oly::app::UpdateAfter")
    message(FATAL_ERROR "${name} runs in ${phase}, expected oly::app::UpdateAfter")
  endif()

  if(name STREQUAL "fireTurrets" OR name STREQUAL "moveProjectiles")
    math(EXPR combatSystemsSeen "${combatSystemsSeen} + 1")
  endif()
endforeach()

if(NOT combatSystemsSeen EQUAL 2)
  message(FATAL_ERROR "expected fireTurrets and moveProjectiles in systems, saw ${combatSystemsSeen} of them")
endif()
