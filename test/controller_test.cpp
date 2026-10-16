// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar

#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#include "rgb/controller.hpp"

namespace {

// Records every write. `current` holds the state the outputs are in now.
class recording_sink final : public rgb::lamp_sink {
public:
    void write(rgb::lamp_state state) override {
        writes.push_back(state);
        current = state;
    }

    std::vector<rgb::lamp_state> writes;
    rgb::lamp_state current;
};

TEST(Controller, LitTicksEqualTheLevelOverOnePeriod) {
    recording_sink sink;
    rgb::controller lamp(sink);
    lamp.set_color(rgb::color{0, 0, 100});

    unsigned lit = 0;
    for (unsigned tick = 0; tick < rgb::soft_pwm::steps_per_period; ++tick) {
        lamp.tick();
        if (sink.current.blue) {
            ++lit;
        }
    }
    EXPECT_EQ(lit, 100u);
}

TEST(Controller, WritesOnlyWhenAnOutputChanges) {
    recording_sink sink;
    rgb::controller lamp(sink);
    lamp.set_color(rgb::color{128, 0, 0});
    for (unsigned tick = 0; tick < 512; ++tick) {
        lamp.tick();
    }
    EXPECT_LT(sink.writes.size(), 512u);
    for (std::size_t i = 1; i < sink.writes.size(); ++i) {
        EXPECT_NE(sink.writes[i], sink.writes[i - 1]) << "write " << i << " repeats the previous state";
    }
}

TEST(Controller, BlackColourNeverWritesTheOutputs) {
    recording_sink sink;
    rgb::controller lamp(sink);
    for (unsigned tick = 0; tick < rgb::soft_pwm::steps_per_period; ++tick) {
        lamp.tick();
    }
    EXPECT_TRUE(sink.writes.empty());
}

TEST(Controller, StopSwitchesEverythingOffAndResetsTheColour) {
    recording_sink sink;
    rgb::controller lamp(sink);
    lamp.set_color(rgb::color{255, 255, 255});
    for (unsigned tick = 0; tick < 10; ++tick) {
        lamp.tick();
    }
    lamp.stop();
    EXPECT_FALSE(sink.current.red || sink.current.green || sink.current.blue);
    EXPECT_EQ(lamp.target(), (rgb::color{0, 0, 0}));
}

}  // namespace
