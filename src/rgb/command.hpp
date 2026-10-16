// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar
//
// The text protocol between rgbctl and rgb-led-daemon: one command per line.

#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include "rgb/pwm.hpp"

namespace rgb {

enum class command_kind { set, off, status, ping };

struct command {
    command_kind kind = command_kind::ping;
    color value{};  ///< The colour for `set`; unused otherwise.
};

/// Longest accepted line, without the line feed.
inline constexpr std::size_t max_line_length = 64;

/// Outcome of parse_command(): a command, or an error message for the client.
struct parse_result {
    std::optional<command> value;
    std::string error;  ///< Set when `value` is empty.
};

/// Parses one line such as "SET 255 128 0", "off", "STATUS" or "PING".
///
/// Keywords are case-insensitive and fields are separated by spaces or tabs.
/// A level is a decimal integer from 0 to 255 with no sign.
parse_result parse_command(std::string_view line);

}  // namespace rgb
