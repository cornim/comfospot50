<!-- SPDX-FileCopyrightText: 2026 cornim -->
<!-- SPDX-License-Identifier: CERN-OHL-S-2.0 -->

# Hardware

The schematic and PCB in this directory are the project-authored controller
board design. The board is intended to interface an ESP32-C3 with the existing
ComfoSpot panel.
See [`wiring.md`](wiring.md) for the panel connections and electrical interface
notes.

## License scope

The project-authored schematic connectivity, PCB layout, routing, board
outline, wiring diagram, and related hardware documentation are released under
the CERN Open Hardware Licence Version 2 - Strongly Reciprocal, found in
`../LICENSES/CERN-OHL-S-2.0.txt`.

The KiCad files contain embedded component symbols and footprints selected from
the JLCPCB/LCSC/EasyEDA component ecosystem and Würth's component library.
Those embedded third-party elements are not relicensed by the project license.
Their provenance and applicable terms are described in
`../THIRD_PARTY_NOTICES.md`.

The standard KiCad library elements used by the design remain subject to the
KiCad library license and its design exception.

The project copyright holder is [`cornim`](https://github.com/cornim).
Licensing and contribution questions can be handled through GitHub for this
repository.

Source location for the covered hardware design is the GitHub repository
containing this file. Preserve this source location and the applicable notices
when conveying the hardware design or a product made from it.

## Deliberate omissions

3D model references are not included in the public PCB file. This keeps the
design portable and avoids distributing model files that are not needed for
schematic review, PCB editing, or fabrication. The board remains a complete
2D KiCad design and retains its footprints, pads, nets, routing, vias, zones,
silkscreen, and outline.

Manufacturer datasheet PDFs are not included. Use the manufacturer or
distributor source for current component documentation. The LCSC identifiers
in the BOM are retained as manufacturing-selection information.

The `hardware/production/comfospot50_full.zip` archive contains Gerber and
drill fabrication output. It is generated output and does not contain the
component library or 3D model files.

## Safety and no warranty

The hardware is provided "as is", without warranty of any kind, express or
implied, including warranties of merchantability, fitness for a particular
purpose, safety, and non-infringement. Verify voltage levels, current limits,
isolation, grounding, enclosure requirements, creepage, clearance, and failure
behavior before construction or use. A qualified person must review and
perform any connection to an installed ventilation appliance or mains-related
equipment.
