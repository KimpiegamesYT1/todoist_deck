#pragma once

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/time/real_time_clock.h"
#include "../hd_device_sc01_plus/hd_device_sc01_plus.h"
#include "todoist_api.h"
#include "todoist_task.h"
#include <vector>
#include <memory>

namespace esphome {
namespace todoist {

class TodoistComponent : public Component {
 public:
  TodoistComponent();
  
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  
  // Set API key from ESPHome config
  void set_api_key(const std::string &api_key);
  
  // Set time component reference for date calculations
  void set_time(time::RealTimeClock *time) { time_ = time; }
  
  // Set update interval in seconds
  void set_update_interval(uint32_t interval) { update_interval_ = interval; }
  
  // Fetch tasks (exposed for retry button)
  void fetch_tasks_();
  
 protected:
  // API handling
  std::unique_ptr<TodoistApi> api_;
  uint32_t update_interval_ = 300; // 5 minutes default
  uint32_t last_update_ = 0;
  
  // Data storage
  std::vector<TodoistTask> tasks_;
  
  // UI elements
  lv_obj_t *main_container_ = nullptr;
  lv_obj_t *task_list_ = nullptr;
  lv_obj_t *loading_label_ = nullptr;
  lv_obj_t *error_label_ = nullptr;
  lv_obj_t *header_label_ = nullptr;
  lv_obj_t *retry_btn_ = nullptr;
  
  // Time component for date calculations
  time::RealTimeClock *time_ = nullptr;
  
  // Methods
  void render_ui_();
  void render_tasks_();
  void on_task_click_(const TodoistTask &task);
  void show_loading_(bool show);
  void show_error_(const std::string &message);
  
  // LVGL event handlers
  static void task_event_cb_(lv_event_t *e);
};

}  // namespace todoist
}  // namespace esphome
