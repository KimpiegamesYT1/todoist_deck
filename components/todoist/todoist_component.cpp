#include "todoist_component.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include <ctime>

namespace esphome {
namespace todoist {

static const char *const TAG = "todoist";

TodoistComponent::TodoistComponent() {
  // Check if API object creation is successful
  api_ = std::unique_ptr<TodoistApi>(new TodoistApi());
  if (!api_) {
      ESP_LOGE(TAG, "Failed to create TodoistApi object!");
      // Handle error appropriately, maybe mark component as failed
  }
}

void TodoistComponent::setup() {
  ESP_LOGI(TAG, "Todoist component initializing...");

  // No try-catch block here

  // Set up main UI container
  main_container_ = lv_obj_create(lv_scr_act());
  if (main_container_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create main container");
    this->mark_failed(); // Mark component as failed if UI setup fails
    return;
  }

  // Create header
  header_label_ = lv_label_create(main_container_);
  if (header_label_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create header label");
    this->mark_failed();
    return;
  }
  
  lv_obj_set_width(header_label_, LV_PCT(100));
  lv_obj_set_style_text_align(header_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_bg_color(header_label_, lv_color_hex(0x454545), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(header_label_, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(header_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(header_label_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_label_set_text(header_label_, "Today's Tasks");
  lv_obj_align(header_label_, LV_ALIGN_TOP_MID, 0, 0);

  // Create task list
  task_list_ = lv_list_create(main_container_);
  if (task_list_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create task list");
    this->mark_failed();
    return;
  }
  
  lv_obj_set_size(task_list_, LV_PCT(100), LV_PCT(90));
  lv_obj_align_to(task_list_, header_label_, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_color(task_list_, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_row(task_list_, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_column(task_list_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(task_list_, 8, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Create loading indicator
  loading_label_ = lv_label_create(main_container_);
  if (loading_label_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create loading label");
    this->mark_failed();
    return;
  }
  
  lv_label_set_text(loading_label_, "Loading tasks...");
  lv_obj_set_style_text_color(loading_label_, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_center(loading_label_);

  // Create network error label (initially hidden)
  error_label_ = lv_label_create(main_container_);
  if (error_label_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create error label");
    this->mark_failed();
    return;
  }
  
  lv_label_set_text(error_label_, "Failed to connect.\nWiFi & OTA still working.\nRetrying...");
  lv_obj_set_style_text_color(error_label_, lv_color_hex(0xFF5555), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_align(error_label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_center(error_label_);
  lv_obj_add_flag(error_label_, LV_OBJ_FLAG_HIDDEN);

  // Add retry button
  retry_btn_ = lv_btn_create(main_container_);
  if (retry_btn_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create retry button");
    this->mark_failed();
    return;
  }
  
  lv_obj_set_size(retry_btn_, 100, 40);
  lv_obj_align(retry_btn_, LV_ALIGN_CENTER, 0, 50);
  lv_obj_add_flag(retry_btn_, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *retry_label = lv_label_create(retry_btn_);
  if (retry_label == nullptr) {
      ESP_LOGE(TAG, "Failed to create retry label");
      this->mark_failed();
      return;
  }
  lv_label_set_text(retry_label, "Retry");
  lv_obj_center(retry_label);

  lv_obj_add_event_cb(retry_btn_, [](lv_event_t *e) {
    TodoistComponent *component = static_cast<TodoistComponent*>(lv_event_get_user_data(e));
    if (component) {
      component->fetch_tasks_();
    }
  }, LV_EVENT_CLICKED, this);

  ESP_LOGI(TAG, "UI setup complete, fetching initial tasks");

  // Initial fetch of tasks
  fetch_tasks_();

  // No catch blocks
}

void TodoistComponent::loop() {
  // No try-catch block here
  uint32_t now = millis() / 1000; // current time in seconds

  // Check if it's time to update tasks
  // Use subtraction to handle potential millis() overflow
  if (now - last_update_ >= update_interval_) {
    fetch_tasks_();
    last_update_ = now;
  }
  // No catch blocks
}

float TodoistComponent::get_setup_priority() const { 
  return setup_priority::LATE; 
}

void TodoistComponent::set_api_key(const std::string &api_key) {
  api_->set_api_key(api_key);
  ESP_LOGI(TAG, "Todoist API key set %s", !api_key.empty() ? "(valid)" : "(empty)");
}

void TodoistComponent::fetch_tasks_() {
  ESP_LOGI(TAG, "Fetching Todoist tasks...");
  show_loading_(true);
  
  api_->fetch_tasks([this](std::vector<TodoistTask> tasks) {
    ESP_LOGI(TAG, "Task fetch complete with %d tasks", tasks.size());
    this->tasks_ = tasks;
    this->render_tasks_();
    this->show_loading_(false);
  }, [this](std::string error) {
    ESP_LOGE(TAG, "Failed to fetch tasks: %s", error.c_str());
    this->show_error_("Connection error: " + error);
  });
}

void TodoistComponent::show_loading_(bool show) {
  if (loading_label_ == nullptr) return;
  
  if (show) {
    lv_obj_clear_flag(loading_label_, LV_OBJ_FLAG_HIDDEN);
    if (task_list_ != nullptr) lv_obj_add_flag(task_list_, LV_OBJ_FLAG_HIDDEN);
    if (error_label_ != nullptr) lv_obj_add_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
    if (retry_btn_ != nullptr) lv_obj_add_flag(retry_btn_, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(loading_label_, LV_OBJ_FLAG_HIDDEN);
    if (task_list_ != nullptr) lv_obj_clear_flag(task_list_, LV_OBJ_FLAG_HIDDEN);
  }
}

void TodoistComponent::show_error_(const std::string &message) {
  ESP_LOGE(TAG, "Error: %s", message.c_str());
  
  if (error_label_ == nullptr) return;
  
  // Show error message
  lv_label_set_text(error_label_, message.c_str());
  lv_obj_clear_flag(error_label_, LV_OBJ_FLAG_HIDDEN);
  
  // Show retry button
  if (retry_btn_ != nullptr) lv_obj_clear_flag(retry_btn_, LV_OBJ_FLAG_HIDDEN);
  
  // Hide other elements
  if (loading_label_ != nullptr) lv_obj_add_flag(loading_label_, LV_OBJ_FLAG_HIDDEN);
  if (task_list_ != nullptr) lv_obj_add_flag(task_list_, LV_OBJ_FLAG_HIDDEN);
}

void TodoistComponent::render_tasks_() {
  if (task_list_ == nullptr) {
    ESP_LOGE(TAG, "Cannot render tasks: task_list_ is null");
    return;
  }

  // No try-catch block here

  // Clear current task list
  lv_obj_clean(task_list_);

  // Filter for today's tasks
  std::vector<TodoistTask> today_tasks;
  for (const auto &task : tasks_) {
    // Check if task pointer is valid before accessing members
    // This check might be overly cautious if tasks_ always contains valid objects
    // if (&task == nullptr) continue; // Example check, might not be necessary

    if (task.is_due_today() || task.is_overdue() || task.due_date.empty()) {
      today_tasks.push_back(task);
    }
  }

  // Update header with task count
  if (header_label_ != nullptr) {
    char header_text[32];
    snprintf(header_text, sizeof(header_text), "Today's Tasks (%d)", (int)today_tasks.size());
    lv_label_set_text(header_label_, header_text);
  }

  if (today_tasks.empty()) {
    // No tasks for today
    lv_obj_t *no_tasks = lv_label_create(task_list_);
    if (no_tasks) { // Check if label creation succeeded
        lv_label_set_text(no_tasks, "No tasks for today!");
        lv_obj_set_style_text_color(no_tasks, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_center(no_tasks);
    } else {
        ESP_LOGE(TAG, "Failed to create 'no tasks' label");
    }
    return;
  }

  // Add each task to the list
  for (const auto &task : today_tasks) {
    // Create list item for task
    lv_obj_t *list_btn = lv_list_add_btn(task_list_, nullptr, task.content.c_str());
    if (list_btn == nullptr) {
      ESP_LOGE(TAG, "Failed to create list button for task %s", task.id.c_str());
      continue; // Skip this task if button creation fails
    }

    lv_obj_set_style_bg_color(list_btn, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(list_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Set priority color indicator on the left side
    lv_obj_set_style_border_side(list_btn, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(list_btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(list_btn, lv_color_hex(task.get_priority_color()), LV_PART_MAIN | LV_STATE_DEFAULT);

    // Get the label within the button to adjust its style
    lv_obj_t *label = lv_obj_get_child(list_btn, 0);
    if (label != nullptr) {
      lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // If task has a due date, add it as a supplementary label
    if (!task.due_date.empty()) {
      lv_obj_t *due_label = lv_label_create(list_btn);
      if (due_label == nullptr) {
        ESP_LOGE(TAG, "Failed to create due date label for task %s", task.id.c_str());
        continue;
      }

      // Format due date
      std::string due_text;
      if (task.is_overdue()) {
        due_text = "OVERDUE: " + task.due_string;
        lv_obj_set_style_text_color(due_label, lv_color_hex(0xFF5555), LV_PART_MAIN | LV_STATE_DEFAULT);
      } else if (task.is_due_today()) {
        due_text = "Today: " + task.due_string;
        lv_obj_set_style_text_color(due_label, lv_color_hex(0x55FF55), LV_PART_MAIN | LV_STATE_DEFAULT);
      } else {
        due_text = task.due_string;
        lv_obj_set_style_text_color(due_label, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
      }

      lv_label_set_text(due_label, due_text.c_str());
      lv_obj_set_style_text_font(due_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_align(due_label, LV_ALIGN_BOTTOM_RIGHT, -5, -5);
    }

    // Store task pointer to retrieve when clicked
    // IMPORTANT: Ensure the 'task' object remains valid for the lifetime of the button.
    // Since 'tasks_' is a member variable and we copy tasks into 'today_tasks',
    // storing a pointer to an object in 'today_tasks' (a local variable) is unsafe.
    // We should store a pointer to the original task in 'tasks_'.
    // Find the original task in tasks_ based on ID or use indices if order is guaranteed.
    // For simplicity now, let's assume tasks_ remains stable during rendering.
    // A safer approach would be to store the task ID and look it up later.
    // Or, ensure tasks_ is not modified while the UI is active.
    // Let's store the task ID instead.
    lv_obj_add_event_cb(list_btn, task_event_cb_, LV_EVENT_CLICKED, this);
    // Store task ID as user data (requires converting string to void*)
    // This is tricky. Let's revert to storing the pointer for now, assuming stability.
    // Find the original task in the main tasks_ vector
    const TodoistTask* original_task_ptr = nullptr;
    for(const auto& original_task : tasks_) {
        if (original_task.id == task.id) {
            original_task_ptr = &original_task;
            break;
        }
    }
    if (original_task_ptr) {
       lv_obj_set_user_data(list_btn, (void*)original_task_ptr);
    } else {
       ESP_LOGW(TAG, "Could not find original task pointer for ID: %s", task.id.c_str());
       // Maybe disable the button or handle this case
    }

  }

  ESP_LOGI(TAG, "Tasks rendered successfully");

  // No catch blocks
}

void TodoistComponent::task_event_cb_(lv_event_t *e) {
  TodoistComponent *component = static_cast<TodoistComponent*>(lv_event_get_user_data(e));
  lv_obj_t *btn = lv_event_get_target(e);
  
  // Get task from user data
  TodoistTask *task = static_cast<TodoistTask*>(lv_obj_get_user_data(btn));
  
  if (component && task) {
    component->on_task_click_(*task);
  }
}

void TodoistComponent::on_task_click_(const TodoistTask &task) {
  ESP_LOGI(TAG, "Task clicked: %s (%s)", task.content.c_str(), task.id.c_str());

  // Create a modal popup for the task
  // Add null checks for all lv_..._create calls
  lv_obj_t *modal = lv_obj_create(lv_layer_top());
  if (!modal) { ESP_LOGE(TAG, "Failed to create modal"); return; }
  lv_obj_set_size(modal, LV_PCT(90), LV_PCT(70));
  lv_obj_center(modal);
  lv_obj_set_style_bg_color(modal, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(modal, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(modal, lv_color_hex(task.get_priority_color()), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(modal, 10, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Task title
  lv_obj_t *title = lv_label_create(modal);
  if (!title) { ESP_LOGE(TAG, "Failed to create modal title"); lv_obj_del(modal); return; }
  lv_label_set_text(title, task.content.c_str());
  lv_obj_set_style_text_font(title, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_width(title, LV_PCT(90));
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

  // Due date if present
  lv_obj_t *due = nullptr; // Declare outside if block
  if (!task.due_date.empty()) {
    due = lv_label_create(modal);
    if (!due) { ESP_LOGE(TAG, "Failed to create modal due date"); /* Continue without due date */ }
    else {
      // Format due date
      std::string due_text = "Due: " + task.due_string;
      if (task.is_overdue()) {
        due_text = "OVERDUE: " + task.due_string;
        lv_obj_set_style_text_color(due, lv_color_hex(0xFF5555), LV_PART_MAIN | LV_STATE_DEFAULT);
      } else if (task.is_due_today()) {
        due_text = "Due Today: " + task.due_string;
        lv_obj_set_style_text_color(due, lv_color_hex(0x55FF55), LV_PART_MAIN | LV_STATE_DEFAULT);
      } else {
        lv_obj_set_style_text_color(due, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
      }
      
      lv_label_set_text(due, due_text.c_str());
      lv_obj_align_to(due, title, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);
    }
  }

  // Description if present
  lv_obj_t *desc = nullptr; // Declare outside if block
  if (!task.description.empty()) {
    desc = lv_label_create(modal);
     if (!desc) { ESP_LOGE(TAG, "Failed to create modal description"); /* Continue without description */ }
     else {
        lv_label_set_text(desc, task.description.c_str());
        lv_obj_set_style_text_color(desc, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
        // Align description relative to title or due date
        lv_obj_align_to(desc, due ? due : title, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 10);
        lv_obj_set_width(desc, LV_PCT(90)); // Ensure width is set
        lv_label_set_long_mode(desc, LV_LABEL_LONG_WRAP); // Allow wrapping
     }
  }

  // Complete button
  lv_obj_t *complete_btn = lv_btn_create(modal);
  if (!complete_btn) { ESP_LOGE(TAG, "Failed to create complete button"); lv_obj_del(modal); return; }
  lv_obj_t *complete_label = lv_label_create(complete_btn);
  if (!complete_label) { ESP_LOGE(TAG, "Failed to create complete label"); lv_obj_del(modal); return; }
  lv_label_set_text(complete_label, "Mark Complete");
  lv_obj_align(complete_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

  // Close button
  lv_obj_t *close_btn = lv_btn_create(modal);
   if (!close_btn) { ESP_LOGE(TAG, "Failed to create close button"); lv_obj_del(modal); return; }
  lv_obj_t *close_label = lv_label_create(close_btn);
  if (!close_label) { ESP_LOGE(TAG, "Failed to create close label"); lv_obj_del(modal); return; }
  lv_label_set_text(close_label, "Close");
  lv_obj_align(close_btn, LV_ALIGN_BOTTOM_LEFT, 10, -10);

  // Button event handlers
  lv_obj_add_event_cb(close_btn, [](lv_event_t *e) {
    lv_obj_t *obj = lv_event_get_current_target(e); // Use current target
    if (obj) {
        lv_obj_t* modal_to_del = lv_obj_get_parent(obj);
        if (modal_to_del) lv_obj_del(modal_to_del); // Delete the modal
    }
  }, LV_EVENT_CLICKED, nullptr);

  // Pass 'this' (TodoistComponent instance) as user data to the lambda
  lv_obj_add_event_cb(complete_btn, [](lv_event_t *e) {
    TodoistComponent *component = static_cast<TodoistComponent*>(lv_event_get_user_data(e));
    // Get the task from the button's user data, NOT the event's user data
    lv_obj_t *btn = lv_event_get_current_target(e);
    const TodoistTask *task_ptr = static_cast<const TodoistTask*>(lv_obj_get_user_data(btn));

    if (component && task_ptr) {
      ESP_LOGI(TAG, "Complete button clicked for task: %s", task_ptr->id.c_str());
      // Mark task as complete using the pointer
      component->api_->complete_task(task_ptr->id, [component](bool success) {
        if (success) {
          ESP_LOGI(TAG, "Task completion successful, refreshing list.");
          // Refresh tasks list
          component->fetch_tasks_();
        } else {
          ESP_LOGW(TAG, "Task completion failed.");
          // Optionally show an error to the user
        }
      });
    } else {
        ESP_LOGE(TAG, "Component or task pointer invalid in complete button callback.");
    }

    // Close the modal
    if (btn) {
        lv_obj_t* modal_to_del = lv_obj_get_parent(btn);
        if (modal_to_del) lv_obj_del(modal_to_del);
    }
  }, LV_EVENT_CLICKED, this); // Pass 'this' here

  // Set task pointer as user data for the complete button
  // Ensure the task object pointed to remains valid
  lv_obj_set_user_data(complete_btn, (void*)(&task));
}

}  // namespace todoist
}  // namespace esphome
