// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar

#include <gtest/gtest.h>

#include <vector>

#include "rgb/pwm.hpp"

namespace {

// Advances the PWM through one full period and counts the steps in which
// `select` picks a lit output. Any 256 consecutive steps cover each step
// value once, so this counts one whole period from any starting point.
template <typename Select>
unsigned lit_steps(rgb::soft_pwm& pwm, Select select) {
    unsigned lit = 0;
    for (unsigned step = 0; step < rgb::soft_pwm::steps_per_period; ++step) {
        if (select(pwm.step())) {
            ++lit;
        }
    }
    return lit;
}

TEST(SoftPwm, DutyCycleIsTheLevelOutOf256) {
    for (const unsigned level : {0u, 1u, 64u, 128u, 254u, 255u}) {
        rgb::soft_pwm pwm;
        pwm.set_target(rgb::color{static_cast<rgb::level>(level), 0, 0});
        EXPECT_EQ(lit_steps(pwm, [](rgb::lamp_state s) { return s.red; }), level) << "level " << level;
    }
}

TEST(SoftPwm, ChannelsAreIndependent) {
    rgb::soft_pwm pwm;
    pwm.set_target(rgb::color{10, 200, 0});
    EXPECT_EQ(lit_steps(pwm, [](rgb::lamp_state s) { return s.red; }), 10u);
    EXPECT_EQ(lit_steps(pwm, [](rgb::lamp_state s) { return s.green; }), 200u);
    EXPECT_EQ(lit_steps(pwm, [](rgb::lamp_state s) { return s.blue; }), 0u);
}

TEST(SoftPwm, PatternRepeatsEveryPeriod) {
    rgb::soft_pwm pwm;
    pwm.set_target(rgb::color{77, 0, 0});
    std::vector<bool> first;
    std::vector<bool> second;
    for (unsigned step = 0; step < rgb::soft_pwm::steps_per_period; ++step) {
        first.push_back(pwm.step().red);
    }
    for (unsigned step = 0; step < rgb::soft_pwm::steps_per_period; ++step) {
        second.push_back(pwm.step().red);
    }
    EXPECT_EQ(first, second);
}

}  // namespace
