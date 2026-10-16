// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar
//
// Helpers for the local command socket: an owning file descriptor and
// line-based reading and writing on Unix stream sockets.

#pragma once

#include <sys/types.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace ipc {

/// Owns a file descriptor and closes it when it goes out of scope.
class unique_fd {
public:
    unique_fd() noexcept = default;
    explicit unique_fd(int fd) noexcept : fd_(fd) {}
    ~unique_fd() { reset(); }

    unique_fd(const unique_fd&) = delete;
    unique_fd& operator=(const unique_fd&) = delete;

    unique_fd(unique_fd&& other) noexcept : fd_(other.release()) {}
    unique_fd& operator=(unique_fd&& other) noexcept {
        if (this != &other) {
            reset(other.release());
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept {
        return fd_;
    }

    explicit operator bool() const noexcept {
        return fd_ >= 0;
    }

    /// Gives up ownership without closing the descriptor.
    int release() noexcept {
        const int fd = fd_;
        fd_ = -1;
        return fd;
    }

    /// Closes the current descriptor and takes ownership of `fd`.
    void reset(int fd = -1) noexcept;

private:
    int fd_ = -1;
};

/// Creates a listening Unix stream socket at `path` with the given permission
/// bits. A socket file left by an earlier run is replaced; any other file at
/// that path is an error. Throws std::system_error or std::runtime_error.
unique_fd listen_unix(const std::string& path, mode_t mode);

/// Connects to a listening Unix socket. On failure returns an invalid
/// descriptor and describes the reason in `error`.
unique_fd connect_unix(const std::string& path, std::string& error);

/// Reads one line from a blocking socket, without the line feed and a trailing
/// carriage return. Returns false when the peer closes the connection or the
/// line is not complete within `limit` bytes.
bool read_line(int fd, std::string& line, std::size_t limit);

/// Writes all of `data`. Returns false on error.
bool send_all(int fd, std::string_view data) noexcept;

}  // namespace ipc
