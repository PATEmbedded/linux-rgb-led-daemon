// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar
//
// rgb-led-daemon drives one RGB LED from three GPIO lines with software PWM.
// Clients send one-line commands over a Unix socket (see rgbctl). A single
// epoll loop handles the PWM tick, client connections and SIGINT/SIGTERM.

#include <getopt.h>
#include <signal.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <optional>
#include <string>
#include <system_error>
#include <utility>

#include "gpiod/gpiod_lamp.hpp"
#include "ipc/unix_socket.hpp"
#include "rgb/command.hpp"
#include "rgb/controller.hpp"

namespace {

constexpr const char* kVersion = "1.0.0";
constexpr unsigned kDefaultStepUs = 50;
constexpr unsigned kMinStepUs = 10;
constexpr unsigned kMaxStepUs = 1000;
constexpr unsigned kMaxLineOffset = 1023;
constexpr const char* kDefaultChip = "/dev/gpiochip0";
constexpr const char* kDefaultSocket = "/run/rgb-led/rgb-led.sock";

struct options {
    std::string chip = kDefaultChip;
    std::optional<unsigned> red;
    std::optional<unsigned> green;
    std::optional<unsigned> blue;
    bool active_low = false;
    std::string socket = kDefaultSocket;
    unsigned step_us = kDefaultStepUs;
};

enum class parse_status { run, exit_success, exit_usage };

// A connected client and the bytes it has sent that do not form a line yet.
struct client {
    ipc::unique_fd fd;
    std::string pending;
};

[[noreturn]] void throw_errno(const std::string& what) {
    throw std::system_error(errno, std::generic_category(), what);
}

void print_usage(std::FILE* out) {
    std::fprintf(out,
                 "Usage: rgb-led-daemon --red LINE --green LINE --blue LINE [options]\n"
                 "\n"
                 "Drives one RGB LED from three GPIO lines with software PWM.\n"
                 "\n"
                 "  --chip PATH     GPIO character device (default %s)\n"
                 "  --red LINE      line offset of the red channel\n"
                 "  --green LINE    line offset of the green channel\n"
                 "  --blue LINE     line offset of the blue channel\n"
                 "  --active-low    the LED lights when its line is low\n"
                 "  --socket PATH   command socket (default %s)\n"
                 "  --step-us N     PWM step in microseconds, %u to %u (default %u)\n"
                 "  --help          show this help\n"
                 "  --version       show the version\n",
                 kDefaultChip, kDefaultSocket, kMinStepUs, kMaxStepUs, kDefaultStepUs);
}

parse_status usage_error(const char* message) {
    std::fprintf(stderr, "rgb-led-daemon: %s\nTry 'rgb-led-daemon --help' for more information.\n", message);
    return parse_status::exit_usage;
}

// Parses a whole decimal number within [min, max].
bool parse_unsigned(const char* text, unsigned min, unsigned max, unsigned& value) {
    char* end = nullptr;
    errno = 0;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < min || parsed > max) {
        return false;
    }
    value = static_cast<unsigned>(parsed);
    return true;
}

parse_status parse_args(int argc, char** argv, options& opt) {
    static const option long_options[] = {
        {"chip", required_argument, nullptr, 'c'},
        {"red", required_argument, nullptr, 'r'},
        {"green", required_argument, nullptr, 'g'},
        {"blue", required_argument, nullptr, 'b'},
        {"active-low", no_argument, nullptr, 'l'},
        {"socket", required_argument, nullptr, 's'},
        {"step-us", required_argument, nullptr, 't'},
        {"help", no_argument, nullptr, 'h'},
        {"version", no_argument, nullptr, 'v'},
        {nullptr, 0, nullptr, 0},
    };

    int option_char = 0;
    while ((option_char = ::getopt_long(argc, argv, "", long_options, nullptr)) != -1) {
        unsigned value = 0;
        switch (option_char) {
            case 'c':
                opt.chip = optarg;
                break;
            case 'r':
                if (!parse_unsigned(optarg, 0, kMaxLineOffset, value)) {
                    return usage_error("--red needs a line offset from 0 to 1023");
                }
                opt.red = value;
                break;
            case 'g':
                if (!parse_unsigned(optarg, 0, kMaxLineOffset, value)) {
                    return usage_error("--green needs a line offset from 0 to 1023");
                }
                opt.green = value;
                break;
            case 'b':
                if (!parse_unsigned(optarg, 0, kMaxLineOffset, value)) {
                    return usage_error("--blue needs a line offset from 0 to 1023");
                }
                opt.blue = value;
                break;
            case 'l':
                opt.active_low = true;
                break;
            case 's':
                opt.socket = optarg;
                break;
            case 't':
                if (!parse_unsigned(optarg, kMinStepUs, kMaxStepUs, value)) {
                    return usage_error("--step-us needs a number from 10 to 1000");
                }
                opt.step_us = value;
                break;
            case 'h':
                print_usage(stdout);
                return parse_status::exit_success;
            case 'v':
                std::printf("rgb-led-daemon %s\n", kVersion);
                return parse_status::exit_success;
            default:
                return usage_error("unknown option");
        }
    }
    if (optind < argc) {
        return usage_error("unexpected argument");
    }
    if (!opt.red || !opt.green || !opt.blue) {
        return usage_error("--red, --green and --blue are required");
    }
    if (*opt.red == *opt.green || *opt.green == *opt.blue || *opt.red == *opt.blue) {
        return usage_error("the three channels need three different lines");
    }
    return parse_status::run;
}

// Answers one command line. Every reply ends with a line feed.
std::string reply_to(rgb::controller& lamp, const std::string& line) {
    const rgb::parse_result parsed = rgb::parse_command(line);
    if (!parsed.value) {
        return "ERR " + parsed.error + "\n";
    }
    switch (parsed.value->kind) {
        case rgb::command_kind::set:
            lamp.set_color(parsed.value->value);
            return "OK\n";
        case rgb::command_kind::off:
            lamp.set_color(rgb::color{});
            return "OK\n";
        case rgb::command_kind::status: {
            const rgb::color now = lamp.target();
            return "OK " + std::to_string(now.red) + " " + std::to_string(now.green) + " " +
                   std::to_string(now.blue) + "\n";
        }
        case rgb::command_kind::ping:
            return "OK\n";
    }
    return "ERR internal error\n";
}

void watch(int epoll_fd, int fd) {
    epoll_event event{};
    event.events = EPOLLIN;
    event.data.fd = fd;
    if (::epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) != 0) {
        throw_errno("epoll_ctl");
    }
}

// Accepts every connection that is waiting. Client sockets are non-blocking.
void accept_clients(int epoll_fd, int listener, std::map<int, client>& clients) {
    for (;;) {
        ipc::unique_fd fd(::accept4(listener, nullptr, nullptr, SOCK_CLOEXEC | SOCK_NONBLOCK));
        if (!fd) {
            if (errno == EINTR) {
                continue;
            }
            return;  // EAGAIN: no more connections are waiting
        }
        const int key = fd.get();
        watch(epoll_fd, key);
        clients.emplace(key, client{std::move(fd), std::string()});
    }
}

// Reads what a client sent and answers each complete line. A client that is
// closed, or that sends a line that is too long, is disconnected.
void handle_client(std::map<int, client>& clients, int fd, rgb::controller& lamp) {
    const auto it = clients.find(fd);
    if (it == clients.end()) {
        return;  // already closed earlier in this batch of events
    }
    client& current = it->second;

    std::array<char, 128> buffer{};
    const ssize_t received = ::recv(fd, buffer.data(), buffer.size(), 0);
    if (received < 0 && (errno == EAGAIN || errno == EINTR)) {
        return;
    }
    if (received <= 0) {
        clients.erase(it);
        return;
    }
    current.pending.append(buffer.data(), static_cast<std::size_t>(received));

    for (std::size_t end = current.pending.find('\n'); end != std::string::npos;
         end = current.pending.find('\n')) {
        std::string line = current.pending.substr(0, end);
        current.pending.erase(0, end + 1);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!ipc::send_all(fd, reply_to(lamp, line))) {
            clients.erase(it);
            return;
        }
    }
    if (current.pending.size() > rgb::max_line_length) {
        ipc::send_all(fd, "ERR line too long\n");
        clients.erase(it);
    }
}

// Switches the LED off and removes the socket file when the daemon stops,
// including when it stops because of an error.
class shutdown_guard {
public:
    shutdown_guard(rgb::controller& lamp, std::string socket_path)
        : lamp_(lamp), socket_path_(std::move(socket_path)) {}

    ~shutdown_guard() {
        try {
            lamp_.stop();
        } catch (const std::exception& error) {
            std::fprintf(stderr, "rgb-led-daemon: could not switch the LED off: %s\n", error.what());
        }
        ::unlink(socket_path_.c_str());
    }

    shutdown_guard(const shutdown_guard&) = delete;
    shutdown_guard& operator=(const shutdown_guard&) = delete;

private:
    rgb::controller& lamp_;
    std::string socket_path_;
};

int run(const options& opt) {
    rgb::gpiod_lamp lamp(opt.chip, rgb::line_offsets{*opt.red, *opt.green, *opt.blue}, opt.active_low);
    rgb::controller controller(lamp);

    // Signals are blocked and read through signalfd, so they never interrupt a
    // write to a GPIO line.
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    if (::sigprocmask(SIG_BLOCK, &mask, nullptr) != 0) {
        throw_errno("sigprocmask");
    }
    ipc::unique_fd signal_fd(::signalfd(-1, &mask, SFD_CLOEXEC | SFD_NONBLOCK));
    if (!signal_fd) {
        throw_errno("signalfd");
    }

    // One tick per PWM step. A tick that arrives late is not repeated: the
    // light simply moves on to the next step.
    ipc::unique_fd tick_fd(::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK));
    if (!tick_fd) {
        throw_errno("timerfd_create");
    }
    itimerspec interval{};
    interval.it_value.tv_sec = static_cast<time_t>(opt.step_us / 1000000u);
    interval.it_value.tv_nsec = static_cast<long>((opt.step_us % 1000000u) * 1000u);
    interval.it_interval = interval.it_value;
    if (::timerfd_settime(tick_fd.get(), 0, &interval, nullptr) != 0) {
        throw_errno("timerfd_settime");
    }

    ipc::unique_fd epoll_fd(::epoll_create1(EPOLL_CLOEXEC));
    if (!epoll_fd) {
        throw_errno("epoll_create1");
    }
    watch(epoll_fd.get(), signal_fd.get());
    watch(epoll_fd.get(), tick_fd.get());

    ipc::unique_fd listener = ipc::listen_unix(opt.socket, 0660);
    watch(epoll_fd.get(), listener.get());
    shutdown_guard guard(controller, opt.socket);

    std::fprintf(stderr, "rgb-led-daemon %s: PWM step %u us, socket %s\n", kVersion, opt.step_us,
                 opt.socket.c_str());

    std::map<int, client> clients;
    std::array<epoll_event, 16> ready{};
    bool running = true;
    while (running) {
        const int count = ::epoll_wait(epoll_fd.get(), ready.data(), static_cast<int>(ready.size()), -1);
        if (count < 0) {
            if (errno == EINTR) {
                continue;
            }
            throw_errno("epoll_wait");
        }
        for (int i = 0; i < count; ++i) {
            const int fd = ready[static_cast<std::size_t>(i)].data.fd;
            if (fd == signal_fd.get()) {
                running = false;
            } else if (fd == tick_fd.get()) {
                std::uint64_t expirations = 0;
                if (::read(tick_fd.get(), &expirations, sizeof(expirations)) < 0 && errno != EAGAIN) {
                    throw_errno("read timer");
                }
                controller.tick();
            } else if (fd == listener.get()) {
                accept_clients(epoll_fd.get(), listener.get(), clients);
            } else {
                handle_client(clients, fd, controller);
            }
        }
    }
    std::fprintf(stderr, "rgb-led-daemon: stopping\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    options opt;
    switch (parse_args(argc, argv, opt)) {
        case parse_status::exit_success:
            return 0;
        case parse_status::exit_usage:
            return 2;
        case parse_status::run:
            break;
    }
    try {
        return run(opt);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "rgb-led-daemon: %s\n", error.what());
        return 1;
    }
}
