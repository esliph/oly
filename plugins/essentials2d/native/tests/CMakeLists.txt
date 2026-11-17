oly_add_test(MODULE essentials2d NAME transforms SOURCES transforms.cpp SUPPORT engine/runtime/ecs/tests/support)
oly_add_test(MODULE essentials2d NAME hierarchy SOURCES hierarchy.cpp SUPPORT engine/runtime/ecs/tests/support)

oly_add_contract_death(MODULE essentials2d SOURCE contract_death.cpp
  CASES parent_dead parent_self parent_cycle parent_destroyed_in_same_buffer parent_destroyed_before_in_same_buffer)
