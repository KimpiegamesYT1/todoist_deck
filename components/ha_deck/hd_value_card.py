# This file has been emptied for the Todoist project refactoring
# It contains ESPHome integration code for value cards that will be replaced
# with Todoist-specific components in Phase 3

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

# Minimal placeholder to not break ESPHome compilation
CONF_TEXT = "text"
CONF_ICON = "icon"
CONF_VALUE = "value"
CONF_UNIT = "unit"

VALUE_CARD_CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(CONF_ID): cv.declare_id(cg.std_ns.class_("void")),
})

# Empty implementation
async def build_value_card(var, config):
    pass