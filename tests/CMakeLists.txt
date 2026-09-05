cmake_minimum_required(VERSION 3.16)
project(comfospot_tests LANGUAGES CXX)

# SPDX-FileCopyrightText: 2026 cornim
# SPDX-License-Identifier: GPL-3.0-or-later

enable_testing()

add_executable(comfospot_protocol_tests test_comfospot_protocol.cpp)
target_compile_features(comfospot_protocol_tests PRIVATE cxx_std_20)
target_include_directories(comfospot_protocol_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/comfospot
)

add_test(NAME comfospot_protocol_tests COMMAND comfospot_protocol_tests)

# Host-side behavioral tests for the ComfoSpot state machine, built against
# a fake ESPHome component surface (tests/stubs/) instead of the real
# ESP-IDF/ESPHome dependencies.
add_executable(comfospot_state_machine_tests
  test_comfospot_state_machine.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/comfospot/comfospot.cpp
  stubs/global_preferences.cpp
)
target_compile_features(comfospot_state_machine_tests PRIVATE cxx_std_20)
target_include_directories(comfospot_state_machine_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${CMAKE_CURRENT_SOURCE_DIR}/../components/comfospot
)

add_test(NAME comfospot_state_machine_tests COMMAND comfospot_state_machine_tests)
