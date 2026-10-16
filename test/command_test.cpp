// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "rgb/command.hpp"

namespace {

rgb::command parse_ok(std::string_view line) {
    const rgb::parse_result result = rgb::parse_command(line);
    EXPECT_TRUE(result.value.has_value()) << "rejected: " << std::string(line) << " (" << result.error << ")";
    return result.value.value_or(rgb::command{});
}

bool parse_rejected(std::string_view line) {
    const rgb::parse_result result = rgb::parse_command(line);
    EXPECT_FALSE(result.value.has_value()) << "accepted: " << std::string(line);
    EXPECT_FALSE(result.error.empty()) << "no error message for: " << std::string(line);
    return !result.value.has_value();
}

TEST(ParseCommand, ReadsSetWithThreeLevels) {
    const rgb::command c = parse_ok("SET 255 128 0");
    EXPECT_EQ(c.kind, rgb::command_kind::set);
    EXPECT_EQ(c.value, (rgb::color{255, 128, 0}));
}

TEST(ParseCommand, KeywordsIgnoreCaseAndTabsSeparateFields) {
    const rgb::command c = parse_ok("set\t1 \t2\t3");
    EXPECT_EQ(c.kind, rgb::command_kind::set);
    EXPECT_EQ(c.value, (rgb::color{1, 2, 3}));

    EXPECT_EQ(parse_ok("  off  ").kind, rgb::command_kind::off);
    EXPECT_EQ(parse_ok("Status").kind, rgb::command_kind::status);
    EXPECT_EQ(parse_ok("ping").kind, rgb::command_kind::ping);
}

TEST(ParseCommand, AcceptsTheLimitsOfEachLevel) {
    EXPECT_EQ(parse_ok("SET 0 0 0").value, (rgb::color{0, 0, 0}));
    EXPECT_EQ(parse_ok("SET 255 255 255").value, (rgb::color{255, 255, 255}));
    EXPECT_EQ(parse_ok("SET 007 0 0").value, (rgb::color{7, 0, 0}));
}

TEST(ParseCommand, RejectsLevelsOutsideTheRangeOrWithBadSyntax) {
    EXPECT_TRUE(parse_rejected("SET 256 0 0"));
    EXPECT_TRUE(parse_rejected("SET 1000 0 0"));
    EXPECT_TRUE(parse_rejected("SET -1 0 0"));
    EXPECT_TRUE(parse_rejected("SET +1 0 0"));
    EXPECT_TRUE(parse_rejected("SET 1.5 0 0"));
    EXPECT_TRUE(parse_rejected("SET a 0 0"));
    EXPECT_TRUE(parse_rejected("SET 1234 0 0"));
}

TEST(ParseCommand, RejectsWrongNumberOfFields) {
    EXPECT_TRUE(parse_rejected("SET"));
    EXPECT_TRUE(parse_rejected("SET 1 2"));
    EXPECT_TRUE(parse_rejected("SET 1 2 3 4"));
    EXPECT_TRUE(parse_rejected("OFF now"));
    EXPECT_TRUE(parse_rejected("PING 1"));
    EXPECT_TRUE(parse_rejected("STATUS 1"));
}

TEST(ParseCommand, RejectsUnknownAndEmptyCommands) {
    EXPECT_TRUE(parse_rejected("BLINK"));
    EXPECT_TRUE(parse_rejected(""));
    EXPECT_TRUE(parse_rejected("   "));
}

TEST(ParseCommand, LineLengthLimitIs64Characters) {
    const std::string longest = "PING" + std::string(60, ' ');
    const std::string too_long = "PING" + std::string(61, ' ');
    EXPECT_EQ(longest.size(), rgb::max_line_length);
    EXPECT_TRUE(parse_ok(longest).kind == rgb::command_kind::ping);
    EXPECT_TRUE(parse_rejected(too_long));
}

}  // namespace
