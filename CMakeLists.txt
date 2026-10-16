cmake_minimum_required(VERSION 3.16)
project(rgb_led_daemon VERSION 1.0.0 LANGUAGES C CXX)

option(RGB_LED_BUILD_DAEMON "Build rgb-led-daemon and rgbctl (Linux only, needs libgpiod 2.0 or newer)" ON)
option(RGB_LED_BUILD_TESTS "Build the unit tests" ON)
option(RGB_LED_SANITIZE "Build the unit tests with AddressSanitizer and UBSan" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Warnings are errors for this project's own code.
function(rgb_led_harden target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wshadow -Werror)
        if(RGB_LED_SANITIZE)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

# PWM, protocol and parsing: no operating-system dependency.
add_library(rgb_core STATIC src/rgb/command.cpp)
target_include_directories(rgb_core PUBLIC src)
rgb_led_harden(rgb_core)

if(RGB_LED_BUILD_DAEMON AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPIOD REQUIRED IMPORTED_TARGET libgpiod>=2.0)

    add_library(rgb_ipc STATIC src/ipc/unix_socket.cpp)
    target_include_directories(rgb_ipc PUBLIC src)
    rgb_led_harden(rgb_ipc)

    add_executable(rgb-led-daemon src/daemon/main.cpp src/gpiod/gpiod_lamp.cpp)
    target_link_libraries(rgb-led-daemon PRIVATE rgb_core rgb_ipc PkgConfig::GPIOD)
    rgb_led_harden(rgb-led-daemon)

    add_executable(rgbctl src/rgbctl/main.cpp)
    target_link_libraries(rgbctl PRIVATE rgb_ipc)
    rgb_led_harden(rgbctl)

    install(TARGETS rgb-led-daemon rgbctl RUNTIME DESTINATION bin)
    install(FILES packaging/rgb-led-daemon.service DESTINATION lib/systemd/system)
    install(FILES packaging/sysusers.d/rgb-led-daemon.conf DESTINATION lib/sysusers.d)
endif()

if(RGB_LED_BUILD_TESTS AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    enable_testing()
    add_subdirectory(test)
endif()
