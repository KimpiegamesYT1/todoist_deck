#pragma once

#include "esphome/core/component.h"
#include "esphome/components/http_request/http_request.h"
#include "todoist_task.h"
#include <vector>
#include <functional>

namespace esphome {
namespace todoist {

class TodoistApi {
 public:
  TodoistApi();
  
  void set_api_key(const std::string &api_key) { api_key_ = api_key; }
  
  // Fetch all active tasks, with success and error callbacks
  void fetch_tasks(
    std::function<void(std::vector<TodoistTask>)> success_callback,
    std::function<void(std::string)> error_callback = nullptr
  );
  
  // Mark a task as completed, with success and error callbacks
  void complete_task(
    const std::string &task_id, 
    std::function<void(bool)> success_callback,
    std::function<void(std::string)> error_callback = nullptr
  );
  
 protected:
  std::string api_key_;
  std::unique_ptr<http_request::HttpRequestComponent> client_;
  
  // Parse JSON response into tasks
  std::vector<TodoistTask> parse_tasks_json(const std::string &json);
};

}  // namespace todoist
}  // namespace esphome
