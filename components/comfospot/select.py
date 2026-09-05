# SPDX-FileCopyrightText: 2026 cornim
# SPDX-License-Identifier: GPL-3.0-or-later

import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import ComfoSpot, comfospot_ns


CONF_COMFOSPOT_ID = "comfospot_id"
ComfoSpotSelect = comfospot_ns.class_("ComfoSpotSelect", cg.Component, select.Select)


CONFIG_SCHEMA = (
    select.select_schema(ComfoSpotSelect)
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
    await select.register_select(var, config, options=["Exchange", "Intake", "Exhaust"])
    await cg.register_component(var, config)
