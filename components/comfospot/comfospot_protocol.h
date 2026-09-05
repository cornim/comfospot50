#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cstdint>

namespace esphome::comfospot::protocol {

constexpr int speed_for_led_mask(uint8_t mask) {
  switch (mask) {
    case 0x01:
      return 1;
    case 0x03:
      return 2;
    case 0x07:
      return 3;
    case 0x0F:
      return 4;
    default:
      return -1;
  }
}

constexpr bool deadline_reached(uint32_t now, uint32_t deadline) {
  return static_cast<int32_t>(now - deadline) >= 0;
}

constexpr bool elapsed_at_least(uint32_t now, uint32_t started, uint32_t duration) {
  return now - started >= duration;
}

}  // namespace esphome::comfospot::protocol
