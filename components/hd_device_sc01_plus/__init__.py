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

CONF_TODOIST_API_KEY = "todoist_api_key"

CODEOWNERS = ["@strange-v"]

hd_device_ns = cg.esphome_ns.namespace("hd_device")
HaDeckDevice = hd_device_ns.class_("HaDeckDevice", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HaDeckDevice),
        cv.Optional(CONF_BRIGHTNESS, default=75): cv.int_range(min=0, max=100),
        cv.Optional(CONF_TODOIST_API_KEY): cv.string,
    }
)

LVGL_BUILD_FLAGS = [
    "-D LV_USE_DEV_VERSION=1",
    "-D LV_LVGL_H_INCLUDE_SIMPLE=1",
]

async def to_code(config):
    whereami = os.path.realpath(__file__)
    component_dir = os.path.dirname(whereami)

    lv_conf_path = os.path.join(component_dir, 'lv_conf.h')
    core.CORE.add_job(cfg.add_includes, [lv_conf_path])

    cg.add_library("lovyan03/LovyanGFX", "^1.1")
    cg.add_library("lvgl/lvgl", "^8.3")
    cg.add_platformio_option("build_flags", LVGL_BUILD_FLAGS)
    cg.add_platformio_option("build_flags", ["-D LV_CONF_PATH='"+lv_conf_path+"'"])

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    if CONF_BRIGHTNESS in config:
        cg.add(var.set_brightness(config[CONF_BRIGHTNESS]))
        
    if CONF_TODOIST_API_KEY in config:
        cg.add(var.set_todoist_api_key(config[CONF_TODOIST_API_KEY]))
