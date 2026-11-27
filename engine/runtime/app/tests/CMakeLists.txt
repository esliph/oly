oly_add_test(MODULE app NAME loop SOURCES loop.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME task_queue SOURCES task_queue.cpp)
oly_add_test(MODULE app NAME states SOURCES states.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME state_scope SOURCES state_scope.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME schedule SOURCES schedule.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME systems SOURCES systems.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME pause SOURCES pause.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME recovery SOURCES recovery.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME events SOURCES events.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME events_loop SOURCES events_loop.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME actions SOURCES actions.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME plugins SOURCES plugins.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME timing SOURCES timing.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME system_parameters SOURCES system_parameters.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE app NAME system_order SOURCES system_order.cpp SUPPORT engine/runtime/ecs/tests/support)

# A negative case holds one system the adapter must refuse. The build exits non-zero by design, so the test passes only
# when the expected compiler error is printed. Every case builds the same binary tree, and two builds of one tree at once
# write the same build-tool state and can run the same step (a re-configure, a shared dependency) together, so the cases
# share one RESOURCE_LOCK and never overlap under ctest -j.
function(oly_add_negative_compile case expected)
  add_library(app_negative_${case} OBJECT EXCLUDE_FROM_ALL negative/${case}.cpp)
  target_link_libraries(app_negative_${case} PRIVATE oly::app)
  add_test(NAME app.negative_compile.${case} COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target app_negative_${case})
  set_tests_properties(app.negative_compile.${case} PROPERTIES PASS_REGULAR_EXPRESSION "${expected}" RESOURCE_LOCK oly_binary_tree_build)
endfunction()

oly_add_negative_compile(unknown_parameter "is not a system parameter")
oly_add_negative_compile(write_through_const_query "read-only|const-qualified")
oly_add_negative_compile(write_through_const_resource "read-only|const-qualified")

oly_add_contract_death(MODULE app SOURCE contract_death.cpp CASES add_system_during_frame while_paused_frame_system while_paused_before_frame_phase system_cycle phase_cycle unordered_phases duplicate_system_name add_system_after_first_frame missing_resource write_write_conflict every_conflict_is_listed read_write_conflict resource_conflict missing_constraint_target contradicting_cross_phase_constraint undeclared_event frame_time_backwards emit_undeclared_event on_undeclared_event declare_event_twice on_during_frame declare_event_during_frame event_round_limit events_wrong_type declare_action_during_frame schedule_negative_delay schedule_not_finite_delay every_zero_interval schedule_null_function task_payload_wrong_type cancel_slot_out_of_range declare_state_during_frame declare_state_twice too_many_states request_state_out_of_range on_enter_during_frame system_states_during_frame system_state_mask_undeclared system_states_unknown_system handler_states_unknown_handler persist_dead_entity install_plugin_twice install_plugin_after_first_frame plugin_not_installed set_timing_enabled_during_frame)
