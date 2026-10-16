// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar
//
// Connects the software PWM to an output. The output is reached through the
// lamp_sink interface, so this file has no hardware dependency.

#pragma once

#include "rgb/pwm.hpp"

namespace rgb {

/// Receives the output states of the lamp, for example three GPIO lines.
class lamp_sink {
public:
    virtual ~lamp_sink() = default;

    /// Drives the outputs to `state`. May throw when the hardware fails.
    virtual void write(lamp_state state) = 0;
};

/// Runs the PWM and writes the sink whenever an output changes.
class controller {
public:
    explicit controller(lamp_sink& sink) noexcept : sink_(sink) {}

    /// Sets the colour. It takes effect within one PWM period.
    void set_color(color target) noexcept {
        pwm_.set_target(target);
    }

    [[nodiscard]] color target() const noexcept {
        return pwm_.target();
    }

    /// Advances the PWM by one step. Call it once per timer tick.
    void tick() {
        const lamp_state next = pwm_.step();
        if (next != shown_) {
            sink_.write(next);
            shown_ = next;
        }
    }

    /// Makes the colour black and switches every output off.
    void stop() {
        pwm_.set_target(color{});
        sink_.write(lamp_state{});
        shown_ = lamp_state{};
    }

private:
    lamp_sink& sink_;
    soft_pwm pwm_;
    lamp_state shown_;
};

}  // namespace rgb
