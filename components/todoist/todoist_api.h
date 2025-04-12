#pragma once

#include "esphome/core/component.h"
#include "todoist_task.h"
#include <vector>
#include <functional>
#include <string>
#include <HTTPClient.h> // ESP32 HTTPClient library

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
  HTTPClient http_; // Use the ESP32 HTTPClient
  
  // Helper methods for HTTP requests
  bool do_http_request(const std::string& url, 
                       const std::string& method, 
                       std::string& response, 
                       std::string& error_message);
  
  // Parse JSON response into tasks
  std::vector<TodoistTask> parse_tasks_json(const std::string &json);
  
  // Add a helper for error handling in JSON parsing
  bool parse_tasks_json_internal(const std::string &json, std::vector<TodoistTask>& tasks, std::string& error_message);
};

}  // namespace todoist
}  // namespace esphome
