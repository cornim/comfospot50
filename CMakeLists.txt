cmake_minimum_required(VERSION 3.16)
project(comfospot50 VERSION 0.1.0 LANGUAGES CXX)

# SPDX-FileCopyrightText: 2026 cornim
# SPDX-License-Identifier: GPL-3.0-or-later

enable_testing()

# Root entry point so the host-side C++ tests can be configured and run
# from the repository root:
#
#   cmake -S . -B build
#   cmake --build build
#   ctest --test-dir build --output-on-failure
#
# The tests themselves (protocol helpers and, against a fake ESPHome
# component surface, the full ComfoSpot state machine) live under tests/.
add_subdirectory(tests)
