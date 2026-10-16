include(FetchContent)

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

# Mark GoogleTest headers as system headers so -Werror applies to our code only.
if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25)
    set(googletest_system SYSTEM)
endif()

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.tar.gz
    URL_HASH SHA256=65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c
    ${googletest_system}
)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(rgb_tests
    command_test.cpp
    controller_test.cpp
    pwm_test.cpp
)
target_link_libraries(rgb_tests PRIVATE rgb_core GTest::gtest_main)
rgb_led_harden(rgb_tests)

include(GoogleTest)
gtest_discover_tests(rgb_tests)
