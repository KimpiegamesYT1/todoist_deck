# This file has been emptied for the Todoist project refactoring
# It will be removed completely in a future phase

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Minimal placeholder to not break ESPHome compilation
CONF_TEXT = "text"
CONF_ICON = "icon"

BUTTON_CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.declare_id(cg.std_ns.class_("void")),
})

# Empty implementation
async def build_button(var, config):
    pass
