# SPDX-FileCopyrightText: 2026 cornim
# SPDX-License-Identifier: GPL-3.0-or-later

import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import ComfoSpot, comfospot_ns


CONF_COMFOSPOT_ID = "comfospot_id"


CONF_SENSOR_TYPE = "sensor_type"
ComfoSpotBinarySensor = comfospot_ns.class_("ComfoSpotBinarySensor", cg.Component, binary_sensor.BinarySensor)
CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(ComfoSpotBinarySensor)
    .extend(
        {
            cv.Required(CONF_COMFOSPOT_ID): cv.use_id(ComfoSpot),
            cv.Required(CONF_SENSOR_TYPE): cv.one_of(
                "filter_change", "error", "auto", lower=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    controller = await cg.get_variable(config[CONF_COMFOSPOT_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        controller,
        config[CONF_SENSOR_TYPE] == "error",
        config[CONF_SENSOR_TYPE] == "auto",
    )
    await binary_sensor.register_binary_sensor(var, config)
    await cg.register_component(var, config)
