oly_add_test(MODULE native_host NAME window SOURCES window.cpp)
set_tests_properties(native_host.window PROPERTIES ENVIRONMENT SDL_VIDEO_DRIVER=dummy)
