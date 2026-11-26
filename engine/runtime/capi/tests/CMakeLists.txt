oly_add_test(MODULE capi NAME c_api SOURCES c_api.cpp test_schema_registry.cpp
  SUPPORT engine/runtime/ecs/tests/support)

oly_add_test(MODULE capi NAME engine_api SOURCES engine_api.cpp test_schema_registry.cpp
  SUPPORT engine/runtime/ecs/tests/support)

oly_add_contract_death(MODULE capi SOURCE contract_death.cpp CASES query_end_out_of_order system_spawn_outside_system engine_phase_out_of_range engine_emit_out_of_range bind_button_wrong_kind action_out_of_range schedule_script_task_negative_interval direct_spawn_outside_context context_persist_outside_context state_hook_invalid_when request_state_out_of_range_c set_script_invoker_null script_without_invoker order_systems_cycle)

# Plain C, without doctest: it includes the C API as a client in another language would.
add_executable(capi_foreign_client foreign_client.c test_schema_registry.cpp)
target_link_libraries(capi_foreign_client PRIVATE oly::capi)
target_include_directories(capi_foreign_client PRIVATE ${PROJECT_SOURCE_DIR}/engine/runtime/ecs/tests/support)
set_target_properties(capi_foreign_client PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON C_EXTENSIONS OFF
  LINKER_LANGUAGE CXX RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
oly_warnings(capi_foreign_client)
add_test(NAME capi.foreign_client COMMAND capi_foreign_client)
