#pragma once

#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comfospot.h"

namespace esphome::comfospot {

class ComfoSpotSensor final : public PollingComponent, public sensor::Sensor {
 public:
  ComfoSpotSensor(ComfoSpot *parent, bool runtime) : parent_(parent), runtime_(runtime) {}

  void setup() override {
    if (this->runtime_) {
      this->parent_->set_runtime_sensor(this);
    } else {
      this->parent_->set_speed_sensor(this);
    }
  }
  void update() override {
    if (this->runtime_) {
      this->publish_state(this->parent_->filter_runtime_days());
    } else if (this->parent_->speed_known()) {
      this->publish_state(this->parent_->current_speed());
    }
  }
  void dump_config() override {
    ESP_LOGCONFIG("comfospot", "ComfoSpot Sensor '%s'", this->get_name().c_str());
  }

 protected:
  ComfoSpot *parent_;
  bool runtime_;
};

}  // namespace esphome::comfospot
