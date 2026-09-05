#pragma once

#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comfospot.h"

namespace esphome::comfospot {

class ComfoSpotFan final : public Component, public fan::Fan {
 public:
  explicit ComfoSpotFan(ComfoSpot *parent) : parent_(parent) {}

  void setup() override { this->parent_->set_fan(this); }
  void dump_config() override {
    ESP_LOGCONFIG("comfospot", "ComfoSpot Fan '%s'", this->get_name().c_str());
  }
  fan::FanTraits get_traits() override { return fan::FanTraits(false, true, false, 4); }

 protected:
  void control(const fan::FanCall &call) override {
    int requested_speed = this->speed;
    if (call.get_speed().has_value()) {
      requested_speed = *call.get_speed();
    } else if (call.get_state().has_value() && *call.get_state() && requested_speed == 0) {
      requested_speed = this->parent_->preferred_speed();
    }
    if (call.get_state().has_value() && !*call.get_state()) {
      requested_speed = 0;
    }
    this->parent_->request_speed(requested_speed);
  }

  ComfoSpot *parent_;
};

}  // namespace esphome::comfospot
