<!-- SPDX-FileCopyrightText: 2026 cornim -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# ComfoSpot Panel Timings

These timings were measured on the live ComfoSpot panel through the ESPHome
verbose scan log. The panel was sampled every 50 ms, so individual transition
timestamps have an uncertainty of approximately one scan interval. Times in
each measurement are relative to the start of that monitor run.

## Measurement 1: Normal Button Press

One physical `+` press was monitored while the panel was active.

| Event | Time |
| --- | ---: |
| Button pressed | 5.832 s |
| Button released | 6.152 s |
| Button duration | 0.319 s |
| Fan LEDs on | 6.195 s |
| Fan LEDs off | 15.237 s |
| Fan LED duration | 9.043 s |

LED mask sequence:

```text
0x00 -> 0x01 -> 0x00
```

The LED indication started approximately 43 ms after the button release.

## Measurement 2: Wake From Standby

One short physical `+` press woke the panel from standby. The standby wake
acknowledgement lights all four fan LEDs.

| Event | Time |
| --- | ---: |
| Button pressed | 5.072 s |
| Button released | 5.278 s |
| Button duration | 0.206 s |
| All fan LEDs on (`0x0F`) | 5.278 s |
| All fan LEDs off (`0x00`) | 6.467 s |
| All-LED duration | 1.189 s |

LED mask sequence:

```text
0x00 -> 0x0F -> 0x00
```

The all-LED frame started at the observed button-release transition, within
the 50 ms measurement resolution.

## Measurement 3: Standby Wake and Intake

The panel was first woken with a short `+` press and then changed to Intake
with a long `+` press.

### Button timings

| Press | Start | End | Duration |
| --- | ---: | ---: | ---: |
| Wake press (`+`) | 6.955 s | 7.120 s | 0.166 s |
| Intake press (`+`) | 7.689 s | 15.777 s | 8.088 s |

### Fan LED transitions

| Time | Transition |
| ---: | --- |
| 7.178 s | `0x00 -> 0x03` |
| 15.198 s | `0x03 -> 0x08` |
| 15.664 s | `0x08 -> 0x00` |
| 16.184 s | `0x00 -> 0x08` |
| 16.689 s | `0x08 -> 0x00` |
| 17.266 s | `0x00 -> 0x08` |
| 17.726 s | `0x08 -> 0x00` |
| 18.182 s | `0x00 -> 0x03` |
| 21.220 s | `0x03 -> 0x08` |
| 21.629 s | `0x08 -> 0x00` |

The primary Intake marker started 7.509 s after the long press began, or
0.579 s before it was released. The first marker sequence returned to the
normal Speed-2 display (`0x03`) 2.405 s after the long press was released.

Relative to the end of the long press:

| Relative time | Transition |
| ---: | --- |
| -0.579 s | `0x03 -> 0x08` |
| -0.113 s | `0x08 -> 0x00` |
| +0.407 s | `0x00 -> 0x08` |
| +0.912 s | `0x08 -> 0x00` |
| +1.489 s | `0x00 -> 0x08` |
| +1.949 s | `0x08 -> 0x00` |
| +2.405 s | `0x00 -> 0x03` |
| +5.443 s | `0x03 -> 0x08` |
| +5.852 s | `0x08 -> 0x00` |

The normal Speed-2 display is `0x03`; the Intake marker is `0x08` alternating
with `0x00`.

## Measurement 4: Intake to Exchange

The panel was woken with a short `+` press and then changed from Intake back to
Exchange with a long `+` press.

### Button timings

| Press | Start | End | Duration |
| --- | ---: | ---: | ---: |
| Wake press (`+`) | 3.798 s | 4.004 s | 0.207 s |
| Exchange press (`+`) | 4.616 s | 12.675 s | 8.059 s |

### Fan LED transitions

| Time | Transition |
| ---: | --- |
| 4.004 s | `0x00 -> 0x03` |
| 4.313 s | `0x03 -> 0x08` |
| 4.768 s | `0x08 -> 0x00` |
| 5.330 s | `0x00 -> 0x08` |
| 5.832 s | `0x08 -> 0x00` |
| 6.353 s | `0x00 -> 0x08` |
| 6.862 s | `0x08 -> 0x00` |
| 7.332 s | `0x00 -> 0x03` |
| 19.302 s | `0x03 -> 0x00` |

The Intake marker sequence completed before the long press ended. The normal
Speed-2 display returned 5.343 s before release and remained visible for
6.627 s afterward. The later `0x03 -> 0x00` transition occurred 6.627 s after
release, when the display entered Energy-Saving mode.

LED mask sequence:

```text
0x00 -> 0x03 -> 0x08 -> 0x00 -> 0x08 -> 0x00
     -> 0x08 -> 0x00 -> 0x03 -> 0x00
```

No `0x0F` all-LED standby acknowledgement was observed in this measurement.

## Measurement 5: Speed 2 to Speed 1 and Energy-Saving

The panel was woken with a short `-` press and then reduced from Speed 2 to
Speed 1 with a short `-` press. The measurement ended when the display entered
Energy-Saving mode.

### Button timings

| Press | Start | End | Duration |
| --- | ---: | ---: | ---: |
| Wake press (`-`) | 3.082 s | 3.238 s | 0.156 s |
| Speed reduction (`-`) | 3.963 s | 4.064 s | 0.101 s |

### Fan LED transitions

| Time | Transition |
| ---: | --- |
| 3.238 s | `0x00 -> 0x03` |
| 4.064 s | `0x03 -> 0x01` |
| 13.296 s | `0x01 -> 0x00` |

The Speed-1 display (`0x01`) remained visible for 9.232 s after the second
button was released. The display then entered Energy-Saving mode at 13.296 s.

LED mask sequence:

```text
0x00 -> 0x03 -> 0x01 -> 0x00
```

## Summary

| Scenario | Button behavior | Relevant panel timing |
| --- | --- | --- |
| Normal press | One short press, 0.319 s | `0x01` appeared 43 ms after release and lasted 9.043 s |
| Standby wake | One short press, 0.206 s | `0x0F` appeared immediately and lasted 1.189 s |
| Wake to Intake | Wake 0.166 s, long press 8.088 s | Intake marker began 7.509 s after long-press start; normal display returned 2.405 s after release |
| Intake to Exchange | Wake 0.207 s, long press 8.059 s | Intake marker completed before release; normal display remained 6.627 s after release |
| Speed 2 to 1 | Wake 0.156 s, short press 0.101 s | Speed-1 display lasted 9.232 s before Energy-Saving |

## Interpretation

- A normal short press is approximately 0.1 to 0.3 s in these measurements.
- The observed long direction press is approximately 8.1 s.
- The Intake marker begins approximately 7.5 s after the long press starts.
- The direction marker alternates a single LED with the all-off mask. Intake
  uses `0x08`; Exhaust uses `0x01` on the observed hardware.
- Standby wake uses a distinct all-four-LED frame (`0x0F`) lasting about 1.2 s.
- After a speed change, the active speed display remains lit for about 9.2 s
  before entering Energy-Saving mode.
