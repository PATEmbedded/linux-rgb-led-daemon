// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar

#include "ipc/unix_socket.hpp"

#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <system_error>

namespace ipc {
namespace {

// Fills in the socket address. Returns false when the path does not fit.
bool make_address(const std::string& path, sockaddr_un& address) {
    if (path.size() >= sizeof(address.sun_path)) {
        return false;
    }
    address = sockaddr_un{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, path.c_str(), path.size() + 1);
    return true;
}

[[noreturn]] void throw_errno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

}  // namespace

void unique_fd::reset(int fd) noexcept {
    if (fd_ >= 0) {
        ::close(fd_);
    }
    fd_ = fd;
}

unique_fd listen_unix(const std::string& path, mode_t mode) {
    sockaddr_un address{};
    if (!make_address(path, address)) {
        throw std::invalid_argument("socket path is too long: " + path);
    }

    // Replace a socket left by an earlier run. Never remove any other kind of file.
    struct stat existing {};
    if (::lstat(path.c_str(), &existing) == 0) {
        if (!S_ISSOCK(existing.st_mode)) {
            throw std::runtime_error(path + " exists and is not a socket");
        }
        ::unlink(path.c_str());
    }

    unique_fd fd(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK, 0));
    if (!fd) {
        throw_errno("socket");
    }

    // The socket file is created with only the requested permissions, so no
    // other user can connect between bind() and a later chmod().
    const mode_t previous = ::umask(static_cast<mode_t>(0777 & ~mode));
    const int bound = ::bind(fd.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address));
    const int bind_error = errno;
    ::umask(previous);
    if (bound != 0) {
        errno = bind_error;
        throw_errno("bind " + path);
    }
    if (::listen(fd.get(), 8) != 0) {
        throw_errno("listen " + path);
    }
    return fd;
}

unique_fd connect_unix(const std::string& path, std::string& error) {
    sockaddr_un address{};
    if (!make_address(path, address)) {
        error = "socket path is too long";
        return unique_fd();
    }
    unique_fd fd(::socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (!fd) {
        error = std::strerror(errno);
        return unique_fd();
    }
    if (::connect(fd.get(), reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0) {
        error = std::strerror(errno);
        return unique_fd();
    }
    return fd;
}

bool read_line(int fd, std::string& line, std::size_t limit) {
    line.clear();
    char c = '\0';
    while (line.size() < limit) {
        const ssize_t received = ::recv(fd, &c, 1, 0);
        if (received < 0 && errno == EINTR) {
            continue;
        }
        if (received <= 0) {
            return false;
        }
        if (c == '\n') {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            return true;
        }
        line.push_back(c);
    }
    return false;
}

bool send_all(int fd, std::string_view data) noexcept {
    while (!data.empty()) {
        const ssize_t sent = ::send(fd, data.data(), data.size(), MSG_NOSIGNAL);
        if (sent < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        data.remove_prefix(static_cast<std::size_t>(sent));
    }
    return true;
}

}  // namespace ipc
