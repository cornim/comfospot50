<!-- SPDX-FileCopyrightText: 2026 cornim -->
<!-- SPDX-License-Identifier: CERN-OHL-S-2.0 -->

# Wiring

The included examples target an ESP32-C3. The panel GPIO mapping is hardcoded
in the component and matches `comfospot50_full.kicad_sch`:

| ESP32 pin | Panel signal | Function |
| --- | --- | --- |
| IO0 | `led_1` | Fan LED 1 |
| IO1 | `led_3` | Fan LED 3 |
| IO3 | `led_2` | Fan LED 2 |
| IO4 | `led_4` | Fan LED 4 |
| IO5 | `btn_plus` | Panel `+` button |
| IO6 | `btn_minus` | Panel `-` button |
| IO7 | `led_err` | Error LED |
| IO10 | `led_filter` | Filter warning LED |
| TX0 / IO21 | `led_auto` | Automatic-mode LED |

All panel LED inputs use inverted logic with pull-ups. The button connections
use inverted open-drain-style logic. `led_auto` is reported by the diagnostic
`Automatic Mode LED` binary sensor.

The component is restricted to the ESP32-C3 mapping above. Configure the
logger as `USB_SERIAL_JTAG` or `USB_CDC`; UART0/TX0 must not drive IO21 because
IO21 is used as the automatic-mode LED input.

## No warranty and safety

This hardware is provided "as is", without warranty of any kind, express or
implied, including warranties of merchantability, fitness for a particular
purpose, safety, and non-infringement. Verify voltage levels, current limits,
isolation, grounding, enclosure requirements, creepage, clearance, and failure
behavior before construction or use.

Do not connect this circuit to a ventilation appliance or mains-related
equipment unless the installation has been reviewed and performed by a
suitably qualified person. This project is independent and is not affiliated
with, endorsed by, or approved by Zehnder Group.
