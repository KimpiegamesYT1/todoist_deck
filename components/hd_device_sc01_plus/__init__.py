# Dit bestand importeert alle benodigde modules voor het component
import os
import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.core as core
import esphome.core.config as cfg
from esphome.core import CORE, coroutine_with_priority
from esphome.const import (
    CONF_ID,
    CONF_BRIGHTNESS
)

# Custom configuratiesleutel voor Todoist API-toegang
CONF_TODOIST_API_KEY = "todoist_api_key"

# Definieert wie verantwoordelijk is voor dit component in het ESPHome project
CODEOWNERS = ["@strange-v"]

# Codegennamespace voor het component wordt gedefinieerd
hd_device_ns = cg.esphome_ns.namespace("hd_device")
HaDeckDevice = hd_device_ns.class_("HaDeckDevice", cg.Component)

# Schema voor de configuratie in YAML-bestanden
# Hiermee kan de gebruiker het volgende configureren:
# 1. Component ID (verplicht)
# 2. Helderheid (optioneel, standaard 75%)
# 3. Todoist API-sleutel (optioneel)
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HaDeckDevice),
        cv.Optional(CONF_BRIGHTNESS, default=75): cv.int_range(min=0, max=100),
        cv.Optional(CONF_TODOIST_API_KEY): cv.string,
    }
)

# Buildvlaggen voor de LVGL grafische bibliotheek
LVGL_BUILD_FLAGS = [
    "-D LV_USE_DEV_VERSION=1",
    "-D LV_LVGL_H_INCLUDE_SIMPLE=1",
]

# Deze functie vertaalt de YAML-configuratie naar C++ code
async def to_code(config):
    # Vindt het pad naar dit bestand en de componentmap
    whereami = os.path.realpath(__file__)
    component_dir = os.path.dirname(whereami)

    # Zorgt dat LVGL correct wordt geconfigureerd
    lv_conf_path = os.path.join(component_dir, 'lv_conf.h')
    core.CORE.add_job(cfg.add_includes, [lv_conf_path])

    # Voegt benodigde bibliotheken toe aan het project
    cg.add_library("lovyan03/LovyanGFX", "^1.2.7")  # Displaybibliotheek
    cg.add_library("lvgl/lvgl", "^8.3")  # UI-bibliotheek
    cg.add_platformio_option("build_flags", LVGL_BUILD_FLAGS)
    cg.add_platformio_option("build_flags", ["-D LV_CONF_PATH='"+lv_conf_path+"'"])

    # Maakt een nieuwe instantie van de HaDeckDevice klasse
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Configureren van helderheid als die is opgegeven
    if CONF_BRIGHTNESS in config:
        cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
    
    # Configureren van Todoist API-sleutel als die is opgegeven    
    if CONF_TODOIST_API_KEY in config:
        cg.add(var.set_todoist_api_key(config[CONF_TODOIST_API_KEY]))
