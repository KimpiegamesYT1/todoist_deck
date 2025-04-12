#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "../hd_device_sc01_plus/hd_device_sc01_plus.h"

namespace esphome {
namespace todoist {

class TodoistComponent : public Component {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  
  // This is a placeholder implementation for Phase 3
  
 protected:
  std::string api_key_;
};

}  // namespace todoist
}  // namespace esphome
