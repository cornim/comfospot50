<!-- SPDX-FileCopyrightText: 2026 cornim -->
<!-- SPDX-License-Identifier: CC-BY-SA-4.0 -->

# Third-Party Notices

The project licenses apply only to project-authored material within the scope
described in `README.md`, `hardware/README.md`, and the source-file notices.
Third-party material remains subject to its own terms.

## JLCPCB, LCSC, and EasyEDA component data

The KiCad schematic and PCB contain embedded symbols and footprints originating
from or based on component data available through the JLCPCB/LCSC/EasyEDA
ecosystem. The following LCSC identifiers are retained as component-selection
and manufacturing references:

```text
C25804       UNI-ROYAL 0603WAF1002T5E
C106214      YAGEO CC0603KRX5R6BB105
C14663       YAGEO CC0603KRX7R9BB104
C13585       Samsung CL31A106KBHNNNE
C5672        Samsung CL31A226KPHNNNE
C2944070     Espressif ESP32-C3-WROOM-02-H4
C5369647     YLPTEC K7803-1000R3
C976701      KEFA KF239-5.08-2P
C82942       MICRONE ME6211C33M5G-N
C963206      G-Switch MK-12C02-G020
C114765      YAGEO RC0402FR-0722RL
C269716      Tyohm RMC06035.1K1%N
C18384       Littelfuse RXEF030
C2482        MDD SS110
C318884      XKB TS-1187A-B-A-B
C393939      SHOU HAN TYPE-C16PIN
C7519        ST USBLC6-2SC6
```

The identifiers and component names do not constitute a license to reproduce
the source library data. No JLCPCB/LCSC/EasyEDA datasheet PDFs are distributed
here. Obtain current component documentation directly from the applicable
manufacturer or distributor.

Relevant source and terms pages include:

- [EasyEDA Terms and Conditions](https://easyeda.com/page/legal)
- [LCSC Terms and Conditions](https://www.lcsc.com/help/terms-and-conditions)
- [JLCPCB Terms and Conditions](https://jlcpcb.com/help/article/Terms-%26-Conditions)
- [easyeda2kicad converter](https://github.com/uPesy/easyeda2kicad.py), licensed separately under AGPL-3.0

The converter's license applies to the converter software, not automatically to
component data retrieved through the converter.

## Würth Elektronik library data

The two WR-MM connector footprints and their embedded symbol data correspond to
the official Würth Elektronik KiCad library:

- [Würth Elektronik KiCad Library](https://github.com/WurthElektronik/KiCad-Library)
- [License terms](https://github.com/WurthElektronik/KiCad-Library/blob/master/License_Terms_WE_KiCad_library.pdf)
- [Disclaimer](https://github.com/WurthElektronik/KiCad-Library/blob/master/Disclaimer_WE_KiCad_library.pdf)

The source repository was inspected at commit
`40adcee44afdab5f4ed8038699f78bd784e0f594` on 2026-09-04. The exact upstream
files matching the connector identifiers were present at that revision.

The relevant parts are:

```text
J_Wurth_WR-MM_690357101472
J_Wurth_WR-MM_690368171472
```

Würth retains ownership of its models. Its terms permit creating and sharing
designs using the models, subject to the separate license terms, but do not
grant a general patent, trademark, or unrestricted sublicensing license. Any
Würth model or library file distributed separately must retain the applicable
Würth notices and must be clearly marked if modified.

## KiCad libraries

The standard KiCad elements used by the project include `power:GND` and the
standard test-point symbol and footprint. KiCad libraries are licensed under
CC BY-SA 4.0 with an exception for electronic designs and generated files that
use the library data.

- [KiCad library license](https://www.kicad.org/libraries/license/)

## Build-time software dependencies

The ESPHome component is intended to be built by ESPHome and is not a copy of
ESPHome itself. ESPHome is available from
[its repository](https://github.com/esphome/esphome) under the MIT License.

The live test runner uses `aioesphomeapi`, declared in
`requirements-dev.txt`. It is available from
[its repository](https://github.com/esphome/aioesphomeapi) under the MIT
License. These dependencies are obtained by users or CI and are not bundled
in this repository.

## Project copyright and contact

Project copyright holder: [`cornim`](https://github.com/cornim). Licensing
questions and notices can be submitted through GitHub for this repository.

## Manufacturer names and trademarks

Manufacturer names, product names, part numbers, and trademarks identify the
components compatible with the design. They do not imply endorsement or
affiliation. Zehnder and ComfoSpot are trademarks of their respective owners.
