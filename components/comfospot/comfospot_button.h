#pragma once

#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

#include "comfospot.h"

namespace esphome::comfospot {

class ComfoSpotButton final : public Component, public button::Button {
 public:
  explicit ComfoSpotButton(ComfoSpot *parent) : parent_(parent) {}

  void dump_config() override {
    ESP_LOGCONFIG("comfospot", "ComfoSpot Button '%s'", this->get_name().c_str());
  }

 protected:
  void press_action() override { this->parent_->reset_filter(); }

  ComfoSpot *parent_;
};

}  // namespace esphome::comfospot
