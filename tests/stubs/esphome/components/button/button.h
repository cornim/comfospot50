#pragma once

// SPDX-FileCopyrightText: 2026 cornim
// SPDX-License-Identifier: GPL-3.0-or-later

// Minimal host-test stand-in for esphome/components/button/button.h.
//
// components/comfospot.h only needs this include to exist; ComfoSpot itself
// holds no button::Button members (only comfospot_button.h, which is not
// exercised by the host test).

namespace esphome::button {

class Button {
 public:
  virtual ~Button() = default;

 protected:
  virtual void press_action() = 0;
};

}  // namespace esphome::button
