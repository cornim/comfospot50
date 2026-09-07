<!-- SPDX-FileCopyrightText: 2026 cornim -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# ComfoSpot 50 Home Assistant integration

This project integrates a ComfoSpot 50 into Home Assistant through a custom
ESPHome component and a custom-designed PCB board. The component connects an
ESP32-C3 to the existing panel, reading its LEDs and electrically simulating
short and long presses of the `+` and `-` buttons.

The component is in `components/comfospot/`. Complete ESPHome examples are in
`examples/`, installation and troubleshooting documentation is in `docs/`, and
wiring and the controller board design are in `hardware/`.

## Installation

See [`docs/installation.md`](docs/installation.md) for software flashing, board
power selection, hardware installation, security, and safety instructions.

## Safety and no warranty

This project is provided "as is", without warranty of any kind, express or
implied, including but not limited to warranties of merchantability, fitness
for a particular purpose, safety, and non-infringement. Use of this project
may involve electrical, fire, ventilation, property-damage, personal-injury,
or equipment-damage risks. You are responsible for verifying the design,
electrical compatibility, isolation, installation, operation, and compliance
with applicable laws and regulations.

Do not connect this circuit to a ventilation appliance or mains-related
equipment unless the installation has been reviewed and performed by a
suitably qualified person. Verify voltage levels, current limits, isolation,
grounding, enclosure requirements, and failure behavior before construction or
use. This project is not a safety control and must not be used where its
failure could create an unsafe condition.

## Unofficial project

This is an independent project and is not affiliated with, endorsed by, or
approved by Zehnder Group. "Zehnder" and "ComfoSpot" are trademarks of their
respective owners and are used only to identify the compatible product.

The original idea for this project and the reverse engineering of the ComfoSpot
50 were done by [lorenzspenger](https://github.com/lorenzspenger) in the
[`comfospot-50`](https://github.com/lorenzspenger/comfospot-50) project. This
repository's implementation was built from scratch because the original project
does not support standby mode.

## Fan levels

The fan exposes four active levels plus standby:

- `0`: standby, fans off
- `1` through `4`: the four ComfoSpot fan levels

In Home Assistant, standby is represented by the fan being off. The diagnostic
`Current Fan Speed` sensor reports the last confirmed numeric level `0` through
`4`. It remains unavailable only while the panel's display state is ambiguous,
such as immediately after boot with all LEDs off; it keeps reporting the last
known active speed while the display is temporarily dark for energy-saving mode.

The component reads the physical panel during boot and does not turn the unit
on automatically. Physical short button presses are synchronized back to Home
Assistant. Remote commands read and validate the panel directly, retrying when
the first press only wakes a sleeping display. Direction changes use the
observed panel marker and a bounded long-press operation. Filter runtime counts
confirmed active fan time and is persisted periodically and after a confirmed
filter reset.

## Hardware

The controller board design and detailed panel wiring documentation are in
[`hardware/README.md`](hardware/README.md) and
[`hardware/wiring.md`](hardware/wiring.md).

## Getting hardware

Detailed instructions for ordering the controller board are in
[`hardware/HOW_TO_ORDER.md`](hardware/HOW_TO_ORDER.md). The minimum
order is five boards, so if you need fewer boards, or if you have additional
boards to sell, feel free to open an issue in this repository to arrange a
purchase or sale.

## ESPHome configuration

The example downloads the component from this repository's `main` branch:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/cornim/comfospot50
      ref: main
    components: [comfospot]
```

See `examples/comfospot.yaml` for a complete example. It requires encrypted
API access, an OTA password, Wi-Fi credentials, and a non-empty fallback access
point password supplied through a local `examples/secrets.yaml`. Never commit
that file or any other deployment secrets.

## Building and testing

Build and run the host-side tests with:

```text
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The live ESPHome API test runner is an optional Python end-to-end test suite in
`tests/e2e_comfospot.py`. It connects to a flashed ComfoSpot device over the
native ESPHome API, discovers its entities, exercises fan controls and state
reporting, and records a test report and device logs. It requires the
development dependency listed in `requirements-dev.txt`. The runner uses
credentials supplied through environment variables and never stores the API key
in test artifacts. See [`docs/e2e-testing.md`](docs/e2e-testing.md) for setup
and usage.

## Repository licensing

- ESPHome component source and tests: `GPL-3.0-or-later`.
- Project-authored schematic, PCB layout, wiring diagram, and hardware design:
  `CERN-OHL-S-2.0`.
- Project-authored documentation and timing notes: `CC-BY-SA-4.0`.
- Embedded component-library data and any other third-party material remain
  subject to their applicable terms; see `THIRD_PARTY_NOTICES.md`.

The project copyright holder is [`cornim`](https://github.com/cornim).
Licensing, security, and contribution questions are handled through GitHub for
this repository.

The license files are in `LICENSES/`. The root `LICENSE` is the GPLv3-or-later
license used for the software portion of the repository. The scoped notices in
the hardware and documentation directories define the other scopes.

No manufacturer datasheet PDFs or product manuals are distributed by this
repository. Consult the relevant manufacturer or distributor for current
documentation.
