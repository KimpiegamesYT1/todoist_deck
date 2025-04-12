#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "LGFX.h"
#include "lvgl.h"

// Removed the LV_IMG_DECLARE(bg_480x320) line which is no longer needed

namespace esphome {
namespace hd_device {

class HaDeckDevice : public Component
{
public:
    void setup() override;
    void loop() override;
    float get_setup_priority() const override;
    uint8_t get_brightness();
    void set_brightness(uint8_t value);
    
    // Add method to set Todoist API key 
    void set_todoist_api_key(const std::string &api_key);
    
private:
    unsigned long time_ = 0;
    uint8_t brightness_ = 0;
    std::string todoist_api_key_;
};

}  // namespace hd_device
}  // namespace esphome