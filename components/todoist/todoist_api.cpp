#include "todoist_api.h"
#include "esphome/core/log.h"
#include <ArduinoJson.h>

namespace esphome {
namespace todoist {

static const char *const TAG = "todoist.api";
static const char *const API_BASE_URL = "https://api.todoist.com/rest/v2";

TodoistApi::TodoistApi() {
  client_ = std::unique_ptr<http_request::HttpRequestComponent>(new http_request::HttpRequestComponent());
}

void TodoistApi::fetch_tasks(
  std::function<void(std::vector<TodoistTask>)> success_callback,
  std::function<void(std::string)> error_callback
) {
  ESP_LOGI(TAG, "Fetching tasks from Todoist");
  
  if (api_key_.empty()) {
    ESP_LOGE(TAG, "API key not set");
    if (error_callback) {
      error_callback("API key not set");
    }
    return;
  }
  
  auto url = std::string(API_BASE_URL) + "/tasks";
  
  try {
    client_->set_url(url);
    client_->set_method(http_request::HTTP_METHOD_GET);
    client_->set_timeout(10000); // 10 second timeout
    client_->set_verify_ssl(false);
    
    client_->clear_headers();
    client_->add_header("Authorization", "Bearer " + api_key_);
    
    client_->set_on_error([error_callback](int error_code) {
      ESP_LOGE(TAG, "HTTP error: %d", error_code);
      if (error_callback) {
        error_callback("Network error: " + std::to_string(error_code));
      }
    });
    
    client_->set_on_response([this, success_callback, error_callback](http_request::Response response) {
      if (response.code == 200) {
        try {
          auto tasks = parse_tasks_json(response.body);
          ESP_LOGI(TAG, "Successfully fetched %d tasks", tasks.size());
          success_callback(tasks);
        } catch (const std::exception &e) {
          ESP_LOGE(TAG, "Error parsing tasks JSON: %s", e.what());
          if (error_callback) {
            error_callback(std::string("Parse error: ") + e.what());
          }
        }
      } else {
        ESP_LOGE(TAG, "Error fetching tasks: HTTP %d - %s", response.code, response.body.c_str());
        if (error_callback) {
          error_callback("HTTP error " + std::to_string(response.code));
        }
      }
    });
    
    client_->send();
    ESP_LOGI(TAG, "Task fetch request sent");
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Exception while fetching tasks: %s", e.what());
    if (error_callback) {
      error_callback(std::string("Exception: ") + e.what());
    }
  }
}

void TodoistApi::complete_task(
  const std::string &task_id, 
  std::function<void(bool)> success_callback,
  std::function<void(std::string)> error_callback
) {
  ESP_LOGI(TAG, "Marking task %s as completed", task_id.c_str());
  
  if (api_key_.empty()) {
    ESP_LOGE(TAG, "API key not set");
    if (error_callback) {
      error_callback("API key not set");
    }
    return;
  }
  
  auto url = std::string(API_BASE_URL) + "/tasks/" + task_id + "/close";
  
  try {
    client_->set_url(url);
    client_->set_method(http_request::HTTP_METHOD_POST);
    client_->set_timeout(10000); // 10 second timeout
    client_->set_verify_ssl(false);
    
    client_->clear_headers();
    client_->add_header("Authorization", "Bearer " + api_key_);
    
    client_->set_on_error([error_callback](int error_code) {
      ESP_LOGE(TAG, "HTTP error: %d", error_code);
      if (error_callback) {
        error_callback("Network error: " + std::to_string(error_code));
      }
    });
    
    client_->set_on_response([success_callback, error_callback, task_id](http_request::Response response) {
      if (response.code == 204) {
        ESP_LOGI(TAG, "Successfully completed task %s", task_id.c_str());
        success_callback(true);
      } else {
        ESP_LOGE(TAG, "Error completing task: HTTP %d - %s", response.code, response.body.c_str());
        if (error_callback) {
          error_callback("HTTP error " + std::to_string(response.code));
        } else {
          success_callback(false);
        }
      }
    });
    
    client_->send();
  } catch (const std::exception &e) {
    ESP_LOGE(TAG, "Exception while completing task: %s", e.what());
    if (error_callback) {
      error_callback(std::string("Exception: ") + e.what());
    }
  }
}

std::vector<TodoistTask> TodoistApi::parse_tasks_json(const std::string &json) {
  std::vector<TodoistTask> tasks;
  
  // Parse JSON
  DynamicJsonDocument doc(32768); // Adjust size as needed
  
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    ESP_LOGE(TAG, "Failed to parse JSON: %s", error.c_str());
    throw std::runtime_error(std::string("JSON parse error: ") + error.c_str());
  }
  
  // Parse each task
  for (JsonVariant task_json : doc.as<JsonArray>()) {
    TodoistTask task;
    
    task.id = task_json["id"].as<std::string>();
    task.content = task_json["content"].as<std::string>();
    
    if (!task_json["description"].isNull()) {
      task.description = task_json["description"].as<std::string>();
    }
    
    if (!task_json["project_id"].isNull()) {
      task.project_id = task_json["project_id"].as<std::string>();
    }
    
    if (!task_json["section_id"].isNull()) {
      task.section_id = task_json["section_id"].as<std::string>();
    }
    
    if (!task_json["parent_id"].isNull()) {
      task.parent_id = task_json["parent_id"].as<std::string>();
    }
    
    // Parse due date
    if (!task_json["due"].isNull()) {
      if (!task_json["due"]["date"].isNull()) {
        task.due_date = task_json["due"]["date"].as<std::string>();
      }
      
      if (!task_json["due"]["string"].isNull()) {
        task.due_string = task_json["due"]["string"].as<std::string>();
      }
    }
    
    // Parse priority (Todoist uses 1-4, with 4 being highest, we invert for our enum)
    int priority = task_json["priority"].as<int>();
    switch (priority) {
      case 1: task.priority = PRIORITY_4; break; // p4 in Todoist
      case 2: task.priority = PRIORITY_3; break; // p3 in Todoist
      case 3: task.priority = PRIORITY_2; break; // p2 in Todoist
      case 4: task.priority = PRIORITY_1; break; // p1 in Todoist
      default: task.priority = PRIORITY_4; break;
    }
    
    tasks.push_back(task);
  }
  
  return tasks;
}

}  // namespace todoist
}  // namespace esphome
