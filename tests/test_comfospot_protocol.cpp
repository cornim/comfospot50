// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comfospot_protocol.h"

#include <cassert>
#include <cstdint>

using esphome::comfospot::protocol::deadline_reached;
using esphome::comfospot::protocol::elapsed_at_least;
using esphome::comfospot::protocol::speed_for_led_mask;

int main() {
  assert(speed_for_led_mask(0x01) == 1);
  assert(speed_for_led_mask(0x03) == 2);
  assert(speed_for_led_mask(0x07) == 3);
  assert(speed_for_led_mask(0x0F) == 4);
  assert(speed_for_led_mask(0x00) == -1);
  assert(speed_for_led_mask(0x02) == -1);
  assert(speed_for_led_mask(0x08) == -1);
  assert(speed_for_led_mask(0x09) == -1);

  assert(elapsed_at_least(150, 100, 50));
  assert(!elapsed_at_least(149, 100, 50));
  assert(elapsed_at_least(20, UINT32_MAX - 29, 50));
  assert(!elapsed_at_least(20, UINT32_MAX - 29, 51));

  assert(!deadline_reached(19, 20));
  assert(deadline_reached(20, 20));
  assert(deadline_reached(30, 20));
  assert(!deadline_reached(UINT32_MAX - 5, 20));

  return 0;
}
