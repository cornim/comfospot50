<!-- SPDX-FileCopyrightText: 2026 cornim -->
<!-- SPDX-License-Identifier: CERN-OHL-S-2.0 -->

# How to Order Hardware

To order new controller boards, use the following process with JLCPCB:

1. Go to [jlcpcb.com](https://jlcpcb.com) and click **Get an Instant Quote**.
2. Upload the complete Gerber ZIP archive from
   `hardware/production/comfospot50_full.zip`. This archive contains the Gerber
   files generated for production.
3. Scroll down and select **PCB assembly**.
4. Click **Next**.
5. Upload the BOM file, `hardware/production/bom.csv`, and the positions file,
   `hardware/production/positions.csv`.
6. Click **Process, BOM, and CPL**.
7. Select all of the parts that you want to use. Most parts should be selected
   automatically. For some parts, you only need to click the **Select** checkbox.

## Unmatched components

Typically, the only component that appears in **Unmatched components** is the
`U_24V_3V3` buck converter. To match it:

1. Take the part number for `U_24V_3V3` from the BOM file.
2. Search for that part number in the JLCPCB component selector.
3. Select the matching part.

Leave the other unmatched components, such as the test points and possibly the
cable and panel connectors from Wuerth, unmatched. When you click **Next**, select
**Don't place** for those components.

After that, follow the remaining JLCPCB ordering process, pay, and place the
order.

## Minimum order and surplus boards

The minimum order quantity is five boards. At the time of writing, five boards
cost approximately EUR 125, plus the Wuerth connectors that must be ordered
separately. Prices and availability can change, so check the current total
before ordering.

If you only need one or two boards, it may make sense to buy the remaining
boards from someone who has already placed a five-board order. If you ordered
five boards but do not need all of them, you can offer the surplus boards to
someone else. For either purpose, feel free to open an issue in this GitHub
repository to find someone who wants to buy or sell additional boards.
