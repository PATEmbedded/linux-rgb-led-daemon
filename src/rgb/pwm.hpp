// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar
//
// Software PWM for three colour channels. It only computes output states, so
// it has no operating-system or hardware dependency and runs in unit tests.

#pragma once

#include <cstdint>

namespace rgb {

/// Brightness of one channel: 0 is off, 255 is the brightest setting.
using level = std::uint8_t;

/// Target colour of the lamp.
struct color {
    level red = 0;
    level green = 0;
    level blue = 0;

    friend constexpr bool operator==(color a, color b) noexcept {
        return a.red == b.red && a.green == b.green && a.blue == b.blue;
    }
    friend constexpr bool operator!=(color a, color b) noexcept {
        return !(a == b);
    }
};

/// Whether each channel's LED is lit during one PWM step.
struct lamp_state {
    bool red = false;
    bool green = false;
    bool blue = false;

    friend constexpr bool operator==(lamp_state a, lamp_state b) noexcept {
        return a.red == b.red && a.green == b.green && a.blue == b.blue;
    }
    friend constexpr bool operator!=(lamp_state a, lamp_state b) noexcept {
        return !(a == b);
    }
};

/// Software PWM with 256 steps per period.
///
/// A channel at level L is lit during L of the 256 steps of every period, so
/// its brightness is L/256 of full power. The length of a step is set by how
/// often step() is called.
class soft_pwm {
public:
    static constexpr unsigned steps_per_period = 256;

    /// Sets the colour to show from the next step on.
    constexpr void set_target(color target) noexcept {
        target_ = target;
    }

    [[nodiscard]] constexpr color target() const noexcept {
        return target_;
    }

    /// Advances one step and returns the outputs for it.
    constexpr lamp_state step() noexcept {
        step_ = (step_ + 1) % steps_per_period;
        return lamp_state{lit(target_.red), lit(target_.green), lit(target_.blue)};
    }

private:
    [[nodiscard]] constexpr bool lit(level channel) const noexcept {
        return step_ < static_cast<unsigned>(channel);
    }

    color target_{};
    unsigned step_ = 0;
};

}  // namespace rgb
