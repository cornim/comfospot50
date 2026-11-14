#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 cornim
# SPDX-License-Identifier: GPL-3.0-or-later

"""Test blueprint templates: python3 -m unittest discover -s tests -p 'test_*.py'."""

import math
from pathlib import Path
import unittest

from jinja2 import StrictUndefined
from jinja2.nativetypes import NativeEnvironment
import yaml


class BlueprintLoader(yaml.SafeLoader):
    """Keep Home Assistant's !input references as names for host-side tests."""


BlueprintLoader.add_constructor("!input", lambda loader, node: loader.construct_scalar(node))
BLUEPRINT = yaml.load(
    (Path(__file__).resolve().parents[1] / "examples/ventilation_control.yaml").read_text(),
    Loader=BlueprintLoader,
)
TEMPLATES = BLUEPRINT["actions"][0]["variables"]


class VentilationHysteresisTests(unittest.TestCase):
    def evaluate(self, current_level, raw_score=None, fan_off=False, co2=0, second=0, **overrides):
        sensors = {"t_o": 20, "t_i": 20, "rh_o": 50, "rh_i": 70, "co2": co2, "second": second}
        context = {
            "t_o_ent": "t_o", "t_i_ent": "t_i", "rh_o_ent": "rh_o", "rh_i_ent": "rh_i",
            "co2_ent": "co2", "second_sensor_ent": "second", "ventilation_fan_ent": "fan",
            "max_level_switch_ent": "cap", "h_t_in": 50, "humidity_points_per_level": 10,
            "negative_v_co2_multiplier": 0, "maximum_level": 4,
            "l1": 600, "l2": 800, "l3": 1000, "l4": 1300,
            "s1": 600, "s2": 800, "s3": 1000, "s4": 1300,
        }
        context.update(overrides)
        env = NativeEnvironment(undefined=StrictUndefined)
        env.globals.update(
            e=math.e, sqrt=math.sqrt,
            states=lambda entity: str(sensors.get(entity, "unknown")),
            has_value=lambda entity: entity in sensors and sensors[entity] not in ("unknown", "unavailable"),
            is_state=lambda entity, state: entity == "fan" and state == "off" and fan_off,
            state_attr=lambda entity, attribute: current_level * 25,
        )
        env.filters["bool"] = lambda value: str(value).strip().lower() in ("true", "1")
        env.filters["log"] = math.log
        for name, template in TEMPLATES.items():
            context[name] = env.from_string(template).render(context)
            if name == "v_raw" and raw_score is not None:
                context[name] = raw_score
        return context

    def test_every_rising_boundary(self):
        for boundary in range(1, 5):
            threshold = boundary * 1.05
            for score, expected in ((threshold - 0.001, boundary - 1), (threshold, boundary)):
                with self.subTest(boundary=boundary, score=score):
                    self.assertEqual(self.evaluate(boundary - 1, score)["v"], expected)

    def test_every_falling_boundary(self):
        for boundary in range(1, 5):
            threshold = boundary * 0.95
            for score, expected in ((threshold - 0.001, boundary - 1), (threshold, boundary)):
                with self.subTest(boundary=boundary, score=score):
                    self.assertEqual(self.evaluate(boundary, score)["v"], expected)

    def test_deadband_retains_level(self):
        for boundary in range(1, 5):
            self.assertEqual(self.evaluate(boundary - 1, boundary)["v"], boundary - 1)
            self.assertEqual(self.evaluate(boundary, boundary)["v"], boundary)

    def test_off_fan_ignores_stale_percentage(self):
        result = self.evaluate(4, 1, fan_off=True)
        self.assertEqual(result["current_fan_level"], 0)
        self.assertEqual(result["v"], 0)
        self.assertEqual(self.evaluate(4, 1.05, fan_off=True)["v"], 1)

    def test_score_limits_and_multi_level_changes(self):
        for current in range(5):
            self.assertEqual(self.evaluate(current, -2)["v"], 0)
            self.assertEqual(self.evaluate(current, 0)["v"], 0)
            self.assertEqual(self.evaluate(current, 100)["v"], 4)

    def test_humidity_calculation_and_configured_scaling(self):
        result = self.evaluate(0)
        self.assertAlmostEqual(result["h_r"], 20)
        self.assertAlmostEqual(result["v_raw"], 2)
        self.assertEqual(result["v"], 1)
        result = self.evaluate(0, humidity_points_per_level=5)
        self.assertAlmostEqual(result["v_raw"], 4)
        self.assertEqual(result["v"], 3)

    def test_negative_score_multiplier_unchanged(self):
        result = self.evaluate(2, -2, negative_v_co2_multiplier=0.5)
        self.assertEqual(result["v_raw"], -2)
        self.assertEqual(result["v"], 0)
        self.assertEqual(result["air_quality_threshold_multiplier"], 2)
        self.assertAlmostEqual(result["air_quality_multiplier_2"], 1.9)
        self.assertAlmostEqual(result["air_quality_multiplier_3"], 2.1)

    def test_air_quality_uses_same_boundaries(self):
        for current in range(5):
            for boundary in range(1, 5):
                multiplier = 0.95 if current >= boundary else 1.05
                threshold = (600, 800, 1000, 1300)[boundary - 1] * multiplier
                for value, expected in ((threshold - 0.001, boundary - 1), (threshold, boundary)):
                    result = self.evaluate(current, 0, co2=value, second=value)
                    self.assertEqual(result["co2_level"], expected)
                    self.assertEqual(result["co2"], value)
                    self.assertEqual(result["second_sensor_value"], value)
                    self.assertEqual(result["second_sensor_level"], expected)

    def test_second_sensor_reading_visible_independently(self):
        result = self.evaluate(0, 0, co2=500, second=900)
        self.assertEqual(result["co2"], 500)
        self.assertEqual(result["second_sensor_value"], 900)
        self.assertEqual(result["co2_level"], 0)
        self.assertEqual(result["second_sensor_level"], 2)
        self.assertEqual(result["air_quality_level"], 2)

    def test_invalid_second_sensor_readings_are_ignored(self):
        for value in ("unknown", "unavailable", "not a number", ""):
            with self.subTest(value=value):
                result = self.evaluate(0, 0, co2=900, second=value)
                self.assertIsNone(result["second_sensor_value"])
                self.assertEqual(result["second_sensor_level"], 0)
                self.assertEqual(result["air_quality_level"], result["co2_level"])

    def test_unconfigured_second_sensor_is_ignored(self):
        for overrides in ({"second_sensor_ent": ""}, {"second_sensor_ent": "missing"},
                          {"s1": ""}, {"s2": "invalid"}):
            with self.subTest(overrides=overrides):
                result = self.evaluate(0, 0, co2=900, second=2000, **overrides)
                self.assertEqual(result["second_sensor_level"], 0)
                self.assertEqual(result["air_quality_level"], result["co2_level"])


if __name__ == "__main__":
    unittest.main()
