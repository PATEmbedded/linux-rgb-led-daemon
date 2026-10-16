// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar

#include "gpiod/gpiod_lamp.hpp"

#include <gpiod.h>

#include <cerrno>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>

namespace rgb {
namespace {

// Calls the matching libgpiod free function when the owner goes out of scope.
template <typename T, void (*Free)(T*)>
struct c_deleter {
    void operator()(T* object) const noexcept {
        if (object != nullptr) {
            Free(object);
        }
    }
};

using settings_ptr = std::unique_ptr<gpiod_line_settings, c_deleter<gpiod_line_settings, gpiod_line_settings_free>>;
using line_config_ptr = std::unique_ptr<gpiod_line_config, c_deleter<gpiod_line_config, gpiod_line_config_free>>;
using request_config_ptr =
    std::unique_ptr<gpiod_request_config, c_deleter<gpiod_request_config, gpiod_request_config_free>>;

[[noreturn]] void throw_errno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

}  // namespace

gpiod_lamp::gpiod_lamp(const std::string& chip_path, line_offsets lines, bool active_low)
    : lines_(lines), active_low_(active_low) {
    chip_ = ::gpiod_chip_open(chip_path.c_str());
    if (chip_ == nullptr) {
        throw_errno("open " + chip_path);
    }
    try {
        settings_ptr settings(::gpiod_line_settings_new());
        line_config_ptr line_config(::gpiod_line_config_new());
        request_config_ptr request_config(::gpiod_request_config_new());
        if (!settings || !line_config || !request_config) {
            throw std::runtime_error("libgpiod: out of memory");
        }

        if (::gpiod_line_settings_set_direction(settings.get(), GPIOD_LINE_DIRECTION_OUTPUT) != 0) {
            throw_errno("set output direction");
        }
        const unsigned offsets[] = {lines.red, lines.green, lines.blue};
        if (::gpiod_line_config_add_line_settings(line_config.get(), offsets, 3, settings.get()) != 0) {
            throw_errno("configure the LED lines");
        }
        ::gpiod_request_config_set_consumer(request_config.get(), "rgb-led-daemon");

        request_ = ::gpiod_chip_request_lines(chip_, request_config.get(), line_config.get());
        if (request_ == nullptr) {
            throw_errno("request lines on " + chip_path);
        }
        write(lamp_state{});  // start with the LED off
    } catch (...) {
        release();
        throw;
    }
}

gpiod_lamp::~gpiod_lamp() {
    if (request_ != nullptr) {
        try {
            write(lamp_state{});  // leave the LED off when the program exits
        } catch (...) {
            // Nothing useful can be done while the program is ending.
        }
    }
    release();
}

void gpiod_lamp::release() noexcept {
    if (request_ != nullptr) {
        ::gpiod_line_request_release(request_);
        request_ = nullptr;
    }
    if (chip_ != nullptr) {
        ::gpiod_chip_close(chip_);
        chip_ = nullptr;
    }
}

void gpiod_lamp::write(lamp_state state) {
    const bool lit[] = {state.red, state.green, state.blue};
    const unsigned offsets[] = {lines_.red, lines_.green, lines_.blue};
    for (std::size_t i = 0; i < 3; ++i) {
        // A lit LED needs a high line, or a low line when it is wired active-low.
        const bool high = lit[i] != active_low_;
        const gpiod_line_value value = high ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE;
        if (::gpiod_line_request_set_value(request_, offsets[i], value) != 0) {
            throw_errno("set line " + std::to_string(offsets[i]));
        }
    }
}

}  // namespace rgb
