<!-- SPDX-FileCopyrightText: 2026 cornim -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Live End-to-End Testing

`tests/e2e_comfospot.py` is a live test runner for a flashed ComfoSpot node.
It connects through the native ESPHome API, discovers entity keys at runtime,
subscribes to INFO-level device logs and state updates, drives fan speeds and
directions, records evidence, and attempts to leave the fan in standby when the
suite ends.

The runner does not store or print the API key. Supply it through the
`ESPHOME_API_KEY` environment variable or `--api-key`. The host can be supplied
through `COMFOSPOT_HOST` or `--host`.

## Prerequisites

The runner is compatible with the deployment's INFO logger level:

```yaml
logger:
  level: INFO
  hardware_uart: USB_SERIAL_JTAG
```

Install the API client if it is not already available:

```text
python3 -m pip install -r requirements-dev.txt
```

## Safe API-Driven Run

Run the complete non-physical suite with:

```text
ESPHOME_API_KEY='...' COMFOSPOT_HOST=comfospot.local \
  python3 tests/e2e_comfospot.py
```

The default run includes API connectivity/entity/state checks, standby-to-speed
1 and standby-to-speed 4 cycles, the six requested direction transitions with
the panel active at a non-zero speed, runtime accounting, and API reconnect.
Each standby speed cycle waits 30 seconds after the fan-off state is observed
before waking the panel. Direction changes are not tested from standby,
power-saving, or a separate running condition: the panel only accepts direction
changes while ventilation is active, and the controller owns the wake/display
preparation. The runner writes `report.json` and `logs.txt` under
`/tmp/comfospot-e2e` by default. The key is not written to either file.

The narrowed suite can be run in bounded batches using shell-style test ID
patterns. This is useful because direction and speed operations intentionally
wait for real panel confirmation:

```text
ESPHOME_API_KEY='...' COMFOSPOT_HOST=comfospot.local \
  python3 -u tests/e2e_comfospot.py --tests 'API-*,FAN-*'

ESPHOME_API_KEY='...' COMFOSPOT_HOST=comfospot.local \
  python3 -u tests/e2e_comfospot.py --tests 'DIR-PANEL-ON,RUN-*'
```

List the complete inventory without connecting:

```text
python3 tests/e2e_comfospot.py --host unused --api-key unused --list-tests
```

Every direction transition explicitly starts from a visibly active speed-2
display. The firmware itself also enforces this rule: if the panel is dark, it
sends a short wake tap and waits for the energy-saving wake or standby
acknowledgement to finish before applying the long direction hold.

The process exits nonzero if any executed test fails. It always attempts a
final fan-off command before disconnecting.
