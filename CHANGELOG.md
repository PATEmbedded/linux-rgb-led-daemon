# Changelog

## 1.0.0 (2026-10-16)

First release. Written from scratch as a standalone project.

- Software PWM with 256 steps per period, with a lamp interface that is independent of the hardware
- libgpiod 2.x backend, with optional active-low wiring
- `rgb-led-daemon`: epoll loop, Unix command socket, clean shutdown on SIGINT and SIGTERM
- `rgbctl`: command-line client with documented exit codes
- systemd unit with a dedicated system user and hardening options
- Unit tests with sanitizers, and CI builds with libgpiod
