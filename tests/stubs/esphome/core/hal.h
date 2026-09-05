#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdint>

// Minimal host-test stand-in for esphome/core/hal.h.
//
// The real header provides millis()/delay() backed by the device's hardware
// timer. For host tests we back millis() with a test-controlled counter so
// tests can deterministically simulate elapsed time, including timer wrap.

namespace esphome {

// Test-controlled clock. Tests set this directly (see tests/test_helpers.h).
inline uint32_t &test_fake_millis() {
  static uint32_t now = 0;
  return now;
}

inline uint32_t millis() { return test_fake_millis(); }
inline void delay(uint32_t) {}

}  // namespace esphome
