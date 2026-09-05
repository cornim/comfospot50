#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 cornim
# SPDX-License-Identifier: GPL-3.0-or-later

"""Live ESPHome API end-to-end tests for the ComfoSpot component.

The test runner deliberately keeps credentials outside the repository. Use
COMFOSPOT_HOST and ESPHOME_API_KEY, or pass --host and --api-key. The API key
is never printed or written to an artifact.

The default suite exercises operations that can be driven through the native
ESPHome API and works with firmware configured at logger level INFO.
"""

from __future__ import annotations

import argparse
import asyncio
import contextlib
from dataclasses import asdict, dataclass, field
import fnmatch
import json
import math
import os
from pathlib import Path
import time
from typing import Any, Awaitable, Callable

try:
    from aioesphomeapi import (
        APIClient,
        BinarySensorInfo,
        ButtonInfo,
        EntityInfo,
        EntityState,
        FanInfo,
        LogLevel,
        SelectInfo,
        SensorInfo,
    )
except ImportError as exc:  # pragma: no cover - exercised by the CLI
    raise SystemExit(
        "aioesphomeapi is required; install it with 'python3 -m pip install -r requirements-dev.txt'"
    ) from exc


STANDBY_SETTLE_SECONDS = 30.0
OPERATION_SETTLE_SECONDS = 12.0


class TestFailure(Exception):
    """A live assertion failed."""


@dataclass
class TestResult:
    test_id: str
    title: str
    status: str
    duration_s: float
    message: str = ""
    evidence: dict[str, Any] = field(default_factory=dict)


@dataclass
class Case:
    test_id: str
    title: str
    run: Callable[[], Awaitable[dict[str, Any]]]


class LiveSuite:
    """State, log, entity, and command helpers for one API connection."""

    def __init__(self, host: str, api_key: str, port: int, output: Path, verbose: bool) -> None:
        self.host = host
        self.client = APIClient(
            host,
            port,
            client_info="comfospot-e2e-tests",
            noise_psk=api_key,
        )
        self.output = output
        self.verbose = verbose
        self.entities: list[EntityInfo] = []
        self.entity_by_id: dict[str, EntityInfo] = {}
        self.state_by_key: dict[tuple[int, int], EntityState] = {}
        self.states_changed = asyncio.Event()
        self.logs: list[dict[str, Any]] = []
        self.log_changed = asyncio.Event()
        self.log_unsubscribe: Callable[[], None] | None = None
        self.device_info: Any = None
        self.keys: dict[str, int] = {}
        self.last_case_log_index = 0

    async def connect(self) -> None:
        await self.client.connect(login=False)
        self.device_info, self.entities, _ = await self.client.device_info_and_list_entities()
        self.entity_by_id = {entity.object_id: entity for entity in self.entities if entity.object_id}
        self._resolve_entities()
        self.log_unsubscribe = self.client.subscribe_logs(
            self._on_log,
            log_level=LogLevel.LOG_LEVEL_INFO,
            dump_config=False,
        )
        self.client.subscribe_states(self._on_state)
        await asyncio.sleep(1.0)

    async def close(self) -> None:
        with contextlib.suppress(Exception):
            self.client.subscribe_logs(lambda _: None, log_level=LogLevel.LOG_LEVEL_NONE)
        if self.log_unsubscribe is not None:
            self.log_unsubscribe()
            self.log_unsubscribe = None
        with contextlib.suppress(Exception):
            await self.client.disconnect()

    def _resolve_entities(self) -> None:
        fan = self._one(FanInfo)
        select = self._one(SelectInfo)
        if fan is not None:
            self.keys["fan"] = fan.key
        if select is not None:
            self.keys["direction"] = select.key

        for entity in self.entities:
            name = f"{entity.name} {entity.object_id}".lower()
            if isinstance(entity, SensorInfo):
                if "current fan speed" in name or "current_fan_speed" in name:
                    self.keys["current_speed"] = entity.key
                elif "filter runtime" in name or "filter_runtime" in name:
                    self.keys["filter_runtime"] = entity.key
            elif isinstance(entity, BinarySensorInfo):
                if "change filter" in name or "filter_change" in name:
                    self.keys["filter"] = entity.key
                elif "error" in name:
                    self.keys["error"] = entity.key
                elif "automatic" in name or "auto" in name:
                    self.keys["auto"] = entity.key
            elif isinstance(entity, ButtonInfo) and (
                "reset filter" in name or "reset_filter" in name
            ):
                self.keys["reset_filter"] = entity.key

        required = ("fan", "current_speed", "direction")
        missing = [name for name in required if name not in self.keys]
        if missing:
            raise TestFailure(f"Required entities were not discovered: {', '.join(missing)}")

    def _one(self, cls: type[EntityInfo]) -> EntityInfo | None:
        matches = [entity for entity in self.entities if isinstance(entity, cls)]
        return matches[0] if matches else None

    def _on_state(self, state: EntityState) -> None:
        self.state_by_key[(state.device_id, state.key)] = state
        self.states_changed.set()

    def _on_log(self, message: Any) -> None:
        text = message.message.decode("utf-8", "backslashreplace") if isinstance(message.message, bytes) else str(message.message)
        item = {
            "received_at": time.monotonic(),
            "level": int(message.level),
            "message": text,
        }
        self.logs.append(item)
        self.log_changed.set()
        if self.verbose:
            print(f"LOG[{message.level}] {text}")

    def entity_inventory(self) -> list[dict[str, Any]]:
        return [
            {
                "type": type(entity).__name__,
                "name": entity.name,
                "object_id": entity.object_id,
                "key": entity.key,
                "device_id": entity.device_id,
            }
            for entity in self.entities
        ]

    def state(self, name: str) -> EntityState | None:
        key = self.keys.get(name)
        if key is None:
            return None
        device_id = next((entity.device_id for entity in self.entities if entity.key == key), 0)
        return self.state_by_key.get((device_id, key)) or self.state_by_key.get((0, key))

    def state_value(self, name: str) -> Any:
        state = self.state(name)
        return getattr(state, "state", None) if state is not None else None

    async def wait_until(
        self,
        predicate: Callable[[], bool],
        timeout: float,
        description: str,
        interval: float = 0.1,
    ) -> None:
        deadline = time.monotonic() + timeout
        while True:
            if predicate():
                return
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                raise TestFailure(f"Timed out waiting for {description}")
            self.states_changed.clear()
            self.log_changed.clear()
            try:
                await asyncio.wait_for(
                    asyncio.gather(self.states_changed.wait(), self.log_changed.wait()),
                    timeout=min(remaining, interval),
                )
            except asyncio.TimeoutError:
                pass

    async def wait_state(self, name: str, expected: Any, timeout: float = 20.0) -> None:
        await self.wait_until(
            lambda: self.state_value(name) == expected,
            timeout,
            f"{name}={expected!r} (last={self.state_value(name)!r})",
        )

    def begin_case(self) -> None:
        self.last_case_log_index = len(self.logs)

    def case_logs(self) -> list[str]:
        return [item["message"] for item in self.logs[self.last_case_log_index :]]

    def evidence(self) -> dict[str, Any]:
        return {
            "state": {name: self.state_value(name) for name in self.keys},
            "logs": self.case_logs()[-80:],
        }

    async def fan(self, speed: int) -> None:
        log_start = len(self.logs)
        current_speed = self.state_value("current_speed")
        if speed == 0 and self.state_value("fan") is False and (
            current_speed == 0 or isinstance(current_speed, float) and math.isnan(current_speed)
        ):
            return
        if speed != 0 and current_speed == speed and self.state_value("fan") is True:
            return
        if speed == 0:
            self.client.fan_command(self.keys["fan"], state=False)
        else:
            self.client.fan_command(self.keys["fan"], state=True, speed_level=speed)
        failure_markers = (
            "Panel did not confirm requested fan speed",
            "Panel did not show a valid speed after wake-up",
            "Panel did not identify standby or active speed after wake-up",
            "Panel did not finish standby wake-up",
            "Panel operation exceeded its safety timeout",
        )
        await self.wait_until(
            lambda: self.state_value("current_speed") == speed
            or any(any(marker in item["message"] for marker in failure_markers) for item in self.logs[log_start:]),
            100.0,
            f"speed operation {speed}",
        )
        # The speed entity is published optimistically after each button press.
        # At logger level INFO there is no DEBUG confirmation message, so wait
        # through the final panel confirmation and dark-panel barrier here.
        await asyncio.sleep(OPERATION_SETTLE_SECONDS)
        if any(any(marker in item["message"] for marker in failure_markers) for item in self.logs[log_start:]):
            failure = next(
                (item["message"] for item in self.logs[log_start:] if any(marker in item for marker in failure_markers)),
                "Panel did not confirm requested fan speed",
            )
            raise TestFailure(failure)
        await self.wait_state("current_speed", speed, timeout=5.0)

    async def direction(self, direction: str) -> None:
        log_start = len(self.logs)
        self.client.select_command(self.keys["direction"], direction)
        failure_markers = (
            "Panel did not confirm requested direction",
            "Panel did not finish wake-up before direction change",
            "ComfoSpot rejected intake mode",
            "Cannot select exchange while panel direction is unknown",
        )
        await self.wait_until(
            lambda: self.state_value("direction") == direction
            or any(any(marker in item["message"] for marker in failure_markers) for item in self.logs[log_start:]),
            30.0,
            f"direction operation {direction}",
        )
        if any(any(marker in item["message"] for marker in failure_markers) for item in self.logs[log_start:]):
            failure = next(
                (item["message"] for item in self.logs[log_start:] if any(marker in item for marker in failure_markers)),
                "Panel did not confirm requested direction",
            )
            raise TestFailure(failure)
        await self.wait_state("direction", direction, timeout=5.0)
        # The direction select state is published only after the marker has
        # been accepted and the final dark-panel barrier has completed.

    async def wake_panel_before_direction(self) -> None:
        """Synchronize speed 2 before a mode command.

        The firmware owns the wake transaction and intentionally leaves the
        panel dark after confirming a command. A stable active display is not
        required before the next mode operation.
        """
        await self.fan(2)

    async def setup_exchange_speed(self, speed: int = 2) -> None:
        await self.fan(speed)
        current = self.state_value("direction")
        if current is None:
            await self.direction("Intake")
            await self.direction("Exchange")
        elif current != "Exchange":
            await self.direction("Exchange")

    async def safe_off(self) -> None:
        if "fan" not in self.keys:
            return
        with contextlib.suppress(TestFailure, asyncio.TimeoutError):
            await self.fan(0)


class Runner:
    def __init__(
        self,
        suite: LiveSuite,
    ) -> None:
        self.suite = suite
        self.results: list[TestResult] = []

    def cases(self) -> list[Case]:
        cases: list[Case] = [
            Case("API-001", "Connect to the node", self.api_connect),
            Case("API-002", "Discover expected entities", self.api_entities),
            Case("API-003", "Subscribe to device logs", self.api_logs),
            Case("API-004", "Validate initial state consistency", self.api_initial_state),
            Case("API-006", "Verify no startup errors", self.api_startup_errors),
        ]
        cases.extend(
            [
                Case("FAN-STANDBY-SPEED-1", "Standby to speed 1 and back", lambda: self.fan_standby_cycle(1)),
                Case("FAN-STANDBY-SPEED-4", "Standby to speed 4 and back", lambda: self.fan_standby_cycle(4)),
            ]
        )
        cases.extend(
            [
                Case(
                    "DIR-PANEL-ON",
                    "All six direction transitions with panel on",
                    lambda: self.direction_condition("panel_on"),
                ),
                Case("RUN-001", "Runtime increases while active", self.runtime_active),
                Case("RUN-002", "Runtime does not increase while standby", self.runtime_standby),
                Case("RUN-003", "Runtime survives speed changes", self.runtime_speed_changes),
                Case("RUN-008", "API reconnect", self.api_reconnect),
            ]
        )
        return cases

    def selected_cases(self, patterns: list[str] | None) -> list[Case]:
        cases = self.cases()
        if not patterns:
            return cases
        selected = [
            case
            for case in cases
            if any(
                fnmatch.fnmatchcase(case.test_id, pattern)
                or fnmatch.fnmatchcase(case.title.lower(), pattern.lower())
                for pattern in patterns
            )
        ]
        if not selected:
            raise TestFailure(f"No tests matched: {', '.join(patterns)}")
        return selected

    async def run(self, patterns: list[str] | None = None) -> None:
        for case in self.selected_cases(patterns):
            self.suite.begin_case()
            started = time.monotonic()
            try:
                evidence = await case.run()
                status = "PASS"
                message = ""
            except (TestFailure, AssertionError, asyncio.TimeoutError) as exc:
                status = "FAIL"
                message = str(exc) or type(exc).__name__
                evidence = self.suite.evidence()
            except Exception as exc:  # keep the rest of the suite observable
                status = "ERROR"
                message = f"{type(exc).__name__}: {exc}"
                evidence = self.suite.evidence()
            duration = time.monotonic() - started
            result = TestResult(case.test_id, case.title, status, duration, message, evidence)
            self.results.append(result)
            print(
                f"{status:5} {case.test_id:22} {case.title} ({duration:.1f}s)"
                f"{f': {message}' if message else ''}",
                flush=True,
            )

    async def api_connect(self) -> dict[str, Any]:
        if self.suite.device_info is None:
            raise TestFailure("Device information was not received")
        return {"device_name": self.suite.device_info.name, "connected_address": self.suite.host}

    async def api_entities(self) -> dict[str, Any]:
        required = {"fan", "current_speed", "direction"}
        optional = {"filter", "error", "auto", "filter_runtime"}
        missing = required - self.suite.keys.keys()
        if missing:
            raise TestFailure(f"Missing required entity keys: {sorted(missing)}")
        return {"keys": self.suite.keys, "optional_missing": sorted(optional - self.suite.keys.keys()), "entities": self.suite.entity_inventory()}

    async def api_logs(self) -> dict[str, Any]:
        await asyncio.sleep(1.0)
        if not self.suite.logs:
            raise TestFailure("No device log messages received")
        return {"log_messages": len(self.suite.logs), "levels": sorted({item["level"] for item in self.suite.logs})}

    async def api_initial_state(self) -> dict[str, Any]:
        speed = self.suite.state_value("current_speed")
        if speed is not None and not (isinstance(speed, float) and math.isnan(speed)) and not 0 <= speed <= 4:
            raise TestFailure(f"Invalid initial speed {speed}")
        if speed is None:
            raise TestFailure("No initial speed state was received")
        return self.suite.evidence()

    async def api_startup_errors(self) -> dict[str, Any]:
        errors = [
            text
            for text in self.suite.case_logs()
            if "Required panel GPIOs are not configured" in text or "Panel operation exceeded" in text
        ]
        if errors:
            raise TestFailure("Startup/runtime errors observed: " + " | ".join(errors))
        return {"errors": errors}

    async def fan_standby_cycle(self, target: int) -> dict[str, Any]:
        await self.suite.fan(0)
        await self.suite.wait_state("fan", False, timeout=15.0)
        await asyncio.sleep(STANDBY_SETTLE_SECONDS)
        await self.suite.fan(target)
        await self.suite.fan(0)
        return {"target": target, **self.suite.evidence()}

    async def direction_condition(self, condition: str) -> dict[str, Any]:
        if condition != "panel_on":
            raise TestFailure(f"Unsupported direction condition {condition!r}")
        transitions = (
            ("Exchange", "Exhaust"),
            ("Exhaust", "Exchange"),
            ("Exchange", "Intake"),
            ("Intake", "Exchange"),
            ("Exhaust", "Intake"),
            ("Intake", "Exhaust"),
        )
        transition_results: list[dict[str, Any]] = []
        for source, target in transitions:
            await self.suite.wake_panel_before_direction()
            await self.ensure_direction(source)

            await self.suite.direction(target)
            transition_results.append(
                {
                    "source": source,
                    "target": target,
                    "final_direction": self.suite.state_value("direction"),
                }
            )
            if self.suite.state_value("direction") != target:
                raise TestFailure(
                    f"Direction transition {source}->{target} ended at "
                    f"{self.suite.state_value('direction')!r}"
                )
        return {
            "condition": condition,
            "transitions": transition_results,
            **self.suite.evidence(),
        }

    async def ensure_direction(self, direction: str) -> None:
        current = self.suite.state_value("direction")
        if current == direction:
            return
        if current not in ("Exchange", "Intake", "Exhaust") and direction == "Exchange":
            # Exchange is intentionally rejected while the firmware has no
            # direction evidence. Establish a safe known direction first,
            # then use the normal marker-based transition back to Exchange.
            await self.suite.direction("Intake")
            current = self.suite.state_value("direction")
        if current != direction:
            await self.suite.direction(direction)


    async def runtime_active(self) -> dict[str, Any]:
        if "filter_runtime" not in self.suite.keys:
            raise TestFailure("Filter Runtime sensor is not present")
        await self.suite.fan(2)
        first = self.suite.state_value("filter_runtime")
        await asyncio.sleep(3.0)
        second = self.suite.state_value("filter_runtime")
        if first is None or second is None or second <= first:
            raise TestFailure(f"Runtime did not increase while active: {first!r} -> {second!r}")
        return {"before": first, "after": second, **self.suite.evidence()}

    async def runtime_standby(self) -> dict[str, Any]:
        if "filter_runtime" not in self.suite.keys:
            raise TestFailure("Filter Runtime sensor is not present")
        await self.suite.fan(0)
        first = self.suite.state_value("filter_runtime")
        await asyncio.sleep(3.0)
        second = self.suite.state_value("filter_runtime")
        if first is None or second is None or second != first:
            raise TestFailure(f"Runtime changed while standby: {first!r} -> {second!r}")
        return {"before": first, "after": second, **self.suite.evidence()}

    async def runtime_speed_changes(self) -> dict[str, Any]:
        if "filter_runtime" not in self.suite.keys:
            raise TestFailure("Filter Runtime sensor is not present")
        await self.suite.fan(1)
        first = self.suite.state_value("filter_runtime")
        await self.suite.fan(4)
        await asyncio.sleep(2.0)
        second = self.suite.state_value("filter_runtime")
        if first is None or second is None or second < first:
            raise TestFailure(f"Runtime regressed across speed change: {first!r} -> {second!r}")
        return {"before": first, "after": second, **self.suite.evidence()}

    async def api_reconnect(self) -> dict[str, Any]:
        await self.suite.client.disconnect()
        await self.suite.client.connect(login=False)
        self.suite.log_unsubscribe = self.suite.client.subscribe_logs(
            self.suite._on_log,
            log_level=LogLevel.LOG_LEVEL_INFO,
            dump_config=False,
        )
        self.suite.client.subscribe_states(self.suite._on_state)
        await asyncio.sleep(1.0)
        if self.suite.state_value("current_speed") is None:
            raise TestFailure("No speed state after API reconnect")
        return self.suite.evidence()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", default=os.environ.get("COMFOSPOT_HOST"), help="Node IP or hostname")
    parser.add_argument("--port", type=int, default=6053)
    parser.add_argument("--api-key", default=os.environ.get("ESPHOME_API_KEY"), help="ESPHome Noise API key")
    parser.add_argument("--output", type=Path, default=Path("/tmp/comfospot-e2e"))
    parser.add_argument("--verbose", action="store_true", help="Print every received device log")
    parser.add_argument(
        "--tests",
        action="append",
        help="Comma-separated shell-style test ID patterns, e.g. API-*,FAN-STANDBY-*; repeatable",
    )
    parser.add_argument("--list-tests", action="store_true", help="List test IDs and exit without connecting")
    return parser.parse_args()


async def main(args: argparse.Namespace) -> int:
    if not args.host or not args.api_key:
        raise SystemExit("Set COMFOSPOT_HOST and ESPHOME_API_KEY, or pass --host and --api-key")
    args.output.mkdir(parents=True, exist_ok=True)
    suite = LiveSuite(args.host, args.api_key, args.port, args.output, args.verbose)
    runner = Runner(suite)
    patterns = [pattern.strip() for value in (args.tests or []) for pattern in value.split(",") if pattern.strip()]
    if args.list_tests:
        for case in runner.cases():
            print(f"{case.test_id:22} {case.title}")
        return 0
    try:
        await suite.connect()
        print(f"Connected to {suite.device_info.name!r} at {args.host}")
        print(f"Discovered entities: {len(suite.entities)}; resolved keys: {sorted(suite.keys)}")
        try:
            await runner.run(patterns)
        finally:
            print("Attempting safe final state: ventilation standby")
            await suite.safe_off()

        report = {
            "host": args.host,
            "device_name": suite.device_info.name,
            "entity_keys": suite.keys,
            "results": [asdict(result) for result in runner.results],
            "summary": {
                status: sum(result.status == status for result in runner.results)
                for status in ("PASS", "FAIL", "SKIP", "ERROR")
            },
        }
        (args.output / "report.json").write_text(json.dumps(report, indent=2, default=str) + "\n", encoding="utf-8")
        (args.output / "logs.txt").write_text(
            "\n".join(f"[{item['level']}] {item['message']}" for item in suite.logs) + "\n",
            encoding="utf-8",
        )
        print(json.dumps(report["summary"], sort_keys=True))
        return 1 if report["summary"]["FAIL"] or report["summary"]["ERROR"] else 0
    finally:
        await suite.close()


if __name__ == "__main__":
    try:
        raise SystemExit(asyncio.run(main(parse_args())))
    except KeyboardInterrupt:
        raise SystemExit(130)
