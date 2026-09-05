# SPDX-FileCopyrightText: 2026 cornim
# SPDX-License-Identifier: GPL-3.0-or-later

import esphome.codegen as cg
from esphome.components import button
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import ComfoSpot, comfospot_ns


CONF_COMFOSPOT_ID = "comfospot_id"
ComfoSpotButton = comfospot_ns.class_("ComfoSpotButton", cg.Component, button.Button)


CONFIG_SCHEMA = (
    button.button_schema(ComfoSpotButton)
    .extend(
        {
            cv.Required(CONF_COMFOSPOT_ID): cv.use_id(ComfoSpot),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    controller = await cg.get_variable(config[CONF_COMFOSPOT_ID])
    var = cg.new_Pvariable(config[CONF_ID], controller)
    await button.register_button(var, config)
    await cg.register_component(var, config)
