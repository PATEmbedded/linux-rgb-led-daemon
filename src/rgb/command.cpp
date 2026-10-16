// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar

#include "rgb/command.hpp"

#include <array>
#include <utility>

namespace rgb {
namespace {

constexpr std::size_t max_fields = 4;  // the keyword and up to three levels

// Splits on spaces and tabs. Returns the number of fields, or max_fields + 1
// when the line holds more than that.
std::size_t split(std::string_view line, std::array<std::string_view, max_fields>& fields) {
    std::size_t count = 0;
    std::size_t pos = 0;
    while (pos < line.size()) {
        while (pos < line.size() && (line[pos] == ' ' || line[pos] == '\t')) {
            ++pos;
        }
        if (pos == line.size()) {
            break;
        }
        if (count == max_fields) {
            return max_fields + 1;
        }
        const std::size_t start = pos;
        while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t') {
            ++pos;
        }
        fields[count++] = line.substr(start, pos - start);
    }
    return count;
}

// Compares case-insensitively. `word` must be upper-case ASCII.
bool is_keyword(std::string_view field, std::string_view word) {
    if (field.size() != word.size()) {
        return false;
    }
    for (std::size_t i = 0; i < field.size(); ++i) {
        char c = field[i];
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
        if (c != word[i]) {
            return false;
        }
    }
    return true;
}

// Accepts "0" to "255": one to three decimal digits, no sign.
bool parse_level(std::string_view field, level& out) {
    if (field.empty() || field.size() > 3) {
        return false;
    }
    unsigned value = 0;
    for (const char c : field) {
        if (c < '0' || c > '9') {
            return false;
        }
        value = value * 10u + static_cast<unsigned>(c - '0');
    }
    if (value > 255u) {
        return false;
    }
    out = static_cast<level>(value);
    return true;
}

parse_result accepted(command cmd) {
    return parse_result{cmd, std::string()};
}

parse_result rejected(const char* message) {
    return parse_result{std::nullopt, std::string(message)};
}

}  // namespace

parse_result parse_command(std::string_view line) {
    if (line.size() > max_line_length) {
        return rejected("line too long");
    }
    std::array<std::string_view, max_fields> fields{};
    const std::size_t count = split(line, fields);
    if (count == 0) {
        return rejected("empty command");
    }
    if (count > max_fields) {
        return rejected("too many fields");
    }

    if (is_keyword(fields[0], "PING")) {
        return count == 1 ? accepted(command{command_kind::ping, color{}}) : rejected("PING takes no fields");
    }
    if (is_keyword(fields[0], "OFF")) {
        return count == 1 ? accepted(command{command_kind::off, color{}}) : rejected("OFF takes no fields");
    }
    if (is_keyword(fields[0], "STATUS")) {
        return count == 1 ? accepted(command{command_kind::status, color{}}) : rejected("STATUS takes no fields");
    }
    if (is_keyword(fields[0], "SET")) {
        if (count != max_fields) {
            return rejected("SET needs three levels, for example: SET 255 128 0");
        }
        color value;
        if (!parse_level(fields[1], value.red) || !parse_level(fields[2], value.green) ||
            !parse_level(fields[3], value.blue)) {
            return rejected("levels must be whole numbers from 0 to 255");
        }
        return accepted(command{command_kind::set, value});
    }
    return rejected("unknown command");
}

}  // namespace rgb
