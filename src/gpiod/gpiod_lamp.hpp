// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar
//
// lamp_sink backed by the Linux GPIO character device through libgpiod 2.x.

#pragma once

#include <string>

#include "rgb/controller.hpp"

// Opaque libgpiod types, declared in <gpiod.h>.
struct gpiod_chip;
struct gpiod_line_request;

namespace rgb {

/// GPIO line offsets of the three channels on one chip.
struct line_offsets {
    unsigned red = 0;
    unsigned green = 0;
    unsigned blue = 0;
};

/// Drives the three channels as outputs. The lines are requested on
/// construction and released on destruction; the LED is switched off first.
class gpiod_lamp final : public lamp_sink {
public:
    /// Opens the chip and requests the three lines, all low. Set `active_low`
    /// when the LED lights while its line is low. Throws std::system_error.
    gpiod_lamp(const std::string& chip_path, line_offsets lines, bool active_low);
    ~gpiod_lamp() override;

    gpiod_lamp(const gpiod_lamp&) = delete;
    gpiod_lamp& operator=(const gpiod_lamp&) = delete;

    /// Throws std::system_error when a line cannot be set.
    void write(lamp_state state) override;

private:
    void release() noexcept;

    gpiod_chip* chip_ = nullptr;
    gpiod_line_request* request_ = nullptr;
    line_offsets lines_;
    bool active_low_;
};

}  // namespace rgb
