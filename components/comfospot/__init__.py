# SPDX-FileCopyrightText: 2026 cornim
# SPDX-License-Identifier: GPL-3.0-or-later

import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components.esp32 import VARIANT_ESP32C3, get_esp32_variant
from esphome.components.esp32.gpio import (
    ESP32InternalGPIOPin,
    esp32_pin_to_code,
)
from esphome.const import (
    CONF_ID,
    CONF_HARDWARE_UART,
    CONF_INVERTED,
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLUP,
)
from esphome.components.logger import USB_CDC, USB_SERIAL_JTAG
from esphome.const import CONF_LOGGER
from esphome.core import ID

AUTO_LOAD = ["fan", "sensor", "binary_sensor", "select", "button"]
DEPENDENCIES = ["esp32"]
comfospot_ns = cg.esphome_ns.namespace("comfospot")
ComfoSpot = comfospot_ns.class_("ComfoSpot", cg.Component)

CONF_LED_1_PIN = "led_1_pin"
CONF_LED_2_PIN = "led_2_pin"
CONF_LED_3_PIN = "led_3_pin"
CONF_LED_4_PIN = "led_4_pin"
CONF_FILTER_PIN = "filter_pin"
CONF_ERROR_PIN = "error_pin"
CONF_AUTO_PIN = "auto_pin"
CONF_PLUS_PIN = "plus_pin"
CONF_MINUS_PIN = "minus_pin"


def _panel_input_pin(number):
    return {
        CONF_NUMBER: number,
        CONF_MODE: {CONF_INPUT: True, CONF_PULLUP: True},
        CONF_INVERTED: True,
    }


def _panel_button_pin(number):
    return {
        CONF_NUMBER: number,
        CONF_MODE: {CONF_INPUT: True, CONF_OUTPUT: True, CONF_OPEN_DRAIN: True},
        CONF_INVERTED: True,
    }


PANEL_PIN_NUMBERS = {
    CONF_LED_1_PIN: 0,
    CONF_LED_2_PIN: 3,
    CONF_LED_3_PIN: 1,
    CONF_LED_4_PIN: 4,
    CONF_ERROR_PIN: 7,
    CONF_FILTER_PIN: 10,
    CONF_AUTO_PIN: 21,
    CONF_PLUS_PIN: 5,
    CONF_MINUS_PIN: 6,
}


CONFIG_SCHEMA = (
    cv.Schema({cv.GenerateID(): cv.declare_id(ComfoSpot)})
    .extend(cv.COMPONENT_SCHEMA)
)


def _validate_logger_uart(config):
    if get_esp32_variant() != VARIANT_ESP32C3:
        raise cv.Invalid("ComfoSpot is supported only on the ESP32-C3")
    # fv.full_config.get() is the resolved top-level configuration itself
    # (it supports the mapping protocol directly); `.data` is an unrelated
    # per-component scratch dict that final validators use to stash their
    # own bookkeeping, not the config tree, so it must not be used here.
    full_config = fv.full_config.get()
    logger_config = full_config.get(CONF_LOGGER)
    if logger_config is not None:
        # By the time FINAL_VALIDATE_SCHEMA runs, the logger component's own
        # schema has already resolved hardware_uart to a concrete value
        # (including its per-variant default), so no sentinel/"unset" value
        # can reach this point other than the logger component being absent
        # entirely. Any UART pin (UART0/UART1) here would drive ESP32-C3
        # IO21/TX0, which this component uses for the automatic-mode LED
        # input, so only the two USB-based UART selections are ever safe.
        uart = logger_config.get(CONF_HARDWARE_UART)
        if uart not in (USB_SERIAL_JTAG, USB_CDC):
            raise cv.Invalid(
                "ComfoSpot uses ESP32-C3 IO21/TX0 for the automatic-mode LED; "
                "the logger must use hardware_uart: USB_SERIAL_JTAG or USB_CDC. "
                "A UART pin (UART0/UART1, including the default) conflicts with "
                "the panel signal and is never allowed."
            )
    return config


FINAL_VALIDATE_SCHEMA = _validate_logger_uart


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    for key, setter in (
        (CONF_LED_1_PIN, "set_led_1_pin"),
        (CONF_LED_2_PIN, "set_led_2_pin"),
        (CONF_LED_3_PIN, "set_led_3_pin"),
        (CONF_LED_4_PIN, "set_led_4_pin"),
        (CONF_FILTER_PIN, "set_filter_pin"),
        (CONF_ERROR_PIN, "set_error_pin"),
        (CONF_AUTO_PIN, "set_auto_pin"),
        (CONF_PLUS_PIN, "set_plus_pin"),
        (CONF_MINUS_PIN, "set_minus_pin"),
    ):
        if key in (CONF_PLUS_PIN, CONF_MINUS_PIN):
            pin_config = _panel_button_pin(PANEL_PIN_NUMBERS[key])
        else:
            pin_config = _panel_input_pin(PANEL_PIN_NUMBERS[key])
        pin_config[CONF_ID] = ID(f"comfospot_{key}", type=ESP32InternalGPIOPin)
        pin = await esp32_pin_to_code(pin_config)
        cg.add(getattr(var, setter)(pin))
