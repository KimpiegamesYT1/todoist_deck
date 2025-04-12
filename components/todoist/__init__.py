import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_TIME_ID, CONF_INTERVAL
from esphome.components import time

DEPENDENCIES = ["network", "time"]
AUTO_LOAD = ["http_request"]

CONF_TODOIST_API_KEY = "todoist_api_key"

todoist_ns = cg.esphome_ns.namespace('todoist')
TodoistComponent = todoist_ns.class_('TodoistComponent', cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TodoistComponent),
    cv.Required(CONF_TODOIST_API_KEY): cv.string,
    cv.Required(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
    cv.Optional(CONF_INTERVAL, default="300s"): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Set API key
    cg.add(var.set_api_key(config[CONF_TODOIST_API_KEY]))
    
    # Set time component
    time_var = await cg.get_variable(config[CONF_TIME_ID])
    cg.add(var.set_time(time_var))
    
    # Set update interval in seconds
    interval = config[CONF_INTERVAL].total_seconds
    cg.add(var.set_update_interval(interval))
    
    # Verwijder de expliciete toevoeging van ArduinoJson, ESPHome detecteert dit meestal automatisch
    # cg.add_library("ArduinoJson", "^6.18.5")
