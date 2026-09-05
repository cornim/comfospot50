#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include <optional>

// Minimal host-test stand-in for esphome/components/fan/fan.h.
//
// comfospot_fan.h's ComfoSpotFan::control() is a concrete (non-template)
// member function body, so it is fully type-checked as soon as
// comfospot.cpp is compiled even though the test harness never
// instantiates ComfoSpotFan itself (tests attach a plain fan::Fan
// instead). FanTraits/FanCall therefore need working, if minimal,
// implementations rather than just forward declarations.

namespace esphome::fan {

class FanTraits {
 public:
  FanTraits() = default;
  FanTraits(bool oscillation, bool speed, bool direction, int speed_count) {
    (void) oscillation;
    (void) speed;
    (void) direction;
    (void) speed_count;
  }
};

class FanCall {
 public:
  std::optional<bool> get_state() const { return {}; }
  std::optional<int> get_speed() const { return {}; }
};

class Fan {
 public:
  virtual ~Fan() = default;
  virtual void publish_state() { this->publish_count++; }
  virtual FanTraits get_traits() { return FanTraits(); }

  bool state{false};
  int speed{0};
  int publish_count{0};

 protected:
  virtual void control(const FanCall &call) { (void) call; }
};

}  // namespace esphome::fan
