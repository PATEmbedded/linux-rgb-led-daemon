// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Pourya Afshintabar
//
// rgbctl sends one command to rgb-led-daemon and prints the reply.

#include <getopt.h>

#include <cstddef>
#include <cstdio>
#include <string>

#include "ipc/unix_socket.hpp"

namespace {

constexpr const char* kDefaultSocket = "/run/rgb-led/rgb-led.sock";
constexpr std::size_t kMaxReply = 256;

void print_usage(std::FILE* out) {
    std::fprintf(out,
                 "Usage: rgbctl [--socket PATH] COMMAND [ARGUMENTS]\n"
                 "\n"
                 "Commands:\n"
                 "  set R G B   show a colour; each level is 0 to 255\n"
                 "  off         switch the LED off\n"
                 "  status      print the colour that is set\n"
                 "  ping        check that the daemon answers\n"
                 "\n"
                 "Exit status: 0 on success, 1 when the daemon rejects the command,\n"
                 "2 for a usage error, 3 when the daemon cannot be reached.\n"
                 "\n"
                 "  --socket PATH   command socket (default %s)\n",
                 kDefaultSocket);
}

}  // namespace

int main(int argc, char** argv) {
    std::string socket_path = kDefaultSocket;

    static const option long_options[] = {
        {"socket", required_argument, nullptr, 's'},
        {"help", no_argument, nullptr, 'h'},
        {nullptr, 0, nullptr, 0},
    };

    // The leading "+" stops option parsing at the first command word, so a
    // negative level is never mistaken for an option.
    int option_char = 0;
    while ((option_char = ::getopt_long(argc, argv, "+", long_options, nullptr)) != -1) {
        switch (option_char) {
            case 's':
                socket_path = optarg;
                break;
            case 'h':
                print_usage(stdout);
                return 0;
            default:
                print_usage(stderr);
                return 2;
        }
    }

    std::string line;
    for (int i = optind; i < argc; ++i) {
        if (!line.empty()) {
            line += ' ';
        }
        line += argv[i];
    }
    if (line.empty()) {
        print_usage(stderr);
        return 2;
    }

    std::string error;
    ipc::unique_fd connection = ipc::connect_unix(socket_path, error);
    if (!connection) {
        std::fprintf(stderr, "rgbctl: cannot reach the daemon at %s: %s\n", socket_path.c_str(), error.c_str());
        return 3;
    }

    std::string reply;
    if (!ipc::send_all(connection.get(), line + "\n") || !ipc::read_line(connection.get(), reply, kMaxReply)) {
        std::fprintf(stderr, "rgbctl: the daemon closed the connection without a reply\n");
        return 3;
    }
    std::printf("%s\n", reply.c_str());
    return (reply == "OK" || reply.rfind("OK ", 0) == 0) ? 0 : 1;
}
