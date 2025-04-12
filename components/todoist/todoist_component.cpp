#include "todoist_component.h"

namespace esphome {
namespace todoist {

static const char *const TAG = "todoist";

void TodoistComponent::setup() {
  // This is a placeholder implementation for Phase 3
  ESP_LOGD(TAG, "Todoist component initializing");
}

void TodoistComponent::loop() {
  // This is a placeholder implementation for Phase 3
}

float TodoistComponent::get_setup_priority() const { return setup_priority::LATE; }

}  // namespace todoist
}  // namespace esphome
