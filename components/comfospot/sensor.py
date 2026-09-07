# SPDX-FileCopyrightText: 2026 cornim
# SPDX-License-Identifier: GPL-3.0-or-later

import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import ComfoSpot, comfospot_ns


CONF_COMFOSPOT_ID = "comfospot_id"


CONF_SENSOR_TYPE = "sensor_type"
FILTER_RUNTIME_UPDATE_INTERVAL_MS = 60000
ComfoSpotSensor = comfospot_ns.class_("ComfoSpotSensor", cg.PollingComponent, sensor.Sensor)
CONFIG_SCHEMA = (
    sensor.sensor_schema(ComfoSpotSensor)
    .extend(
        {
            cv.Required(CONF_COMFOSPOT_ID): cv.use_id(ComfoSpot),
            cv.Required(CONF_SENSOR_TYPE): cv.one_of(
                "current_speed", "filter_runtime", lower=True
            ),
        }
    )
    .extend(cv.polling_component_schema("1s"))
)


async def to_code(config):
    controller = await cg.get_variable(config[CONF_COMFOSPOT_ID])
    var = cg.new_Pvariable(
        config[CONF_ID], controller, config[CONF_SENSOR_TYPE] == "filter_runtime"
    )
    await cg.register_component(var, config)
    if config[CONF_SENSOR_TYPE] == "filter_runtime":
        cg.add(var.set_update_interval(FILTER_RUNTIME_UPDATE_INTERVAL_MS))
    await sensor.register_sensor(var, config)
