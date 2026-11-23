oly_add_test(MODULE native_host NAME window SOURCES window.cpp)
set_tests_properties(native_host.window PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)

oly_add_test(MODULE native_host NAME input SOURCES input.cpp)
set_tests_properties(native_host.input PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)

oly_add_test(MODULE native_host NAME game SOURCES game.cpp)
set_tests_properties(native_host.game PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)
