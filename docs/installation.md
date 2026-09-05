<!-- SPDX-FileCopyrightText: 2026 cornim -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Installation

## Board power selection

The controller board has a power-source switch with `USB` and `24V`
positions. Select the power source before connecting it:

- For USB flashing, disconnect the 24 V supply and set the switch to `USB`
  before connecting the USB cable. The ESP32 is not powered from USB unless
  the switch is in this position.
- For normal operation, disconnect the USB cable, set the switch to `24V`,
  and then connect the 24 V supply.
- Never connect USB and 24 V at the same time.

## Software

The ESPHome firmware must be compiled and flashed onto the ESP32 before the
controller can operate. Install ESPHome or use the ESPHome add-on in Home
Assistant. Copy `examples/comfospot.yaml` for a local installation and
configure the device name. The panel GPIO mapping is fixed by the component.
Create `examples/secrets.yaml` with your own values:

```yaml
wifi_ssid: My SSID
wifi_password: My Wi-Fi password
api_encryption_key: My API encryption key
ota_password: My OTA password
fallback_ap_password: My fallback hotspot password (8+ characters)
```

With the 24 V supply disconnected and the board switch in the `USB` position,
connect USB and run:

```text
esphome run examples/comfospot.yaml
```

ESPHome may enter download mode automatically. If it cannot connect to the
board or the upload fails, leave USB connected and powered, then use the
board's buttons to enter download mode: hold `BOOT`, press and release `RESET`
once, and then release `BOOT`. Retry the command after entering download mode.
For more detail about ESP32-C3 USB flashing and manual download mode, see
Espressif's [ESP32-C3 serial connection instructions](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/get-started/establish-serial-connection.html).

After the firmware has been flashed successfully, disconnect USB, move the
switch to `24V`, and connect the 24 V supply. Subsequent firmware updates can
use ESPHome OTA with USB disconnected and the switch in the `24V` position.

The example requires all five values shown above when built directly with the
`esphome` CLI.

The YAML and C++ source are build-time inputs. The ESP32 receives one compiled
firmware image; documentation and source files are not uploaded to it.

## Hardware

### Prepare the custom board

Before starting the hardware installation, solder the two Würth connectors to
the custom board:

- Solder the female connector into the `CABLE1` position.
- Solder the male connector into the `PANEL1` position.
- Install and solder both connectors on the side of the board marked with the
  corresponding reference text. Solder all connector pins and verify that the
  connectors are fully seated and aligned.

The male connector in `PANEL1` plugs directly into the back panel of the
ComfoSpot 50. The original panel cable is then plugged into the female
`CABLE1` connector on the custom board.

### Install the board

The ComfoSpot 50 main control board is on the side of the unit under a thin
plastic cover. Pull the cover off gently to expose the main control board.

1. Disconnect the custom board from USB.
2. Set the custom board's power switch to `24V`.
3. Ensure that the 24 V supply is disconnected while changing the appliance
   wiring.
4. Locate the power cable on the side of the main control board furthest from
   the panel and remove it from the original power-supply input.
5. Split the existing 24 V supply using suitable connectors. WAGO 221-413
   clamps are one example; equivalent connectors rated for the wiring and
   circuit may also be used. Use one connector for each power conductor,
   preserving the polarity. From the connectors, run two cables back to the
   original power-supply input of the ComfoSpot main control board and two
   cables to the custom board's `24V IN` connector.
6. Remove the original panel cable from the back panel of the ComfoSpot 50.
7. Plug the custom board's `PANEL1` connector directly into the back panel.
8. Plug the removed panel cable into the custom board's `CABLE1` connector.

Check the wiring and polarity before restoring the 24 V supply. USB and 24 V
must never be connected at the same time.

## Security

Use encrypted ESPHome API access and a non-empty OTA password for installations
connected to a real network. Do not commit `secrets.yaml` or distribute shared
credentials.

## Safety

The software and hardware are provided "as is", without warranty of any kind.
Verify the electrical interface, voltage levels, isolation, grounding, and
failure behavior before connecting an ESP32 to an installed ventilation
appliance. Any appliance or mains-related work must be reviewed and performed
by a suitably qualified person.
