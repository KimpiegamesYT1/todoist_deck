#include "todoist_api.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h" // Include Application to access global components
#include "esphome/components/http_request/http_request.h" // Ensure full header is included
#include <ArduinoJson.h>

namespace esphome {
namespace todoist {

static const char *const TAG = "todoist.api";
static const char *const API_BASE_URL = "https://api.todoist.com/rest/v2";

TodoistApi::TodoistApi() {
  // Create a new HTTP client instance
  http_client_ = new http_request::HttpRequestComponent();
}

TodoistApi::~TodoistApi() {
  if (http_client_ != nullptr) {
    delete http_client_;
    http_client_ = nullptr;
  }
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

  if (http_client_ == nullptr) {
    ESP_LOGE(TAG, "HTTP client not initialized");
    if (error_callback) {
      error_callback("HTTP client not initialized");
    }
    return;
  }

  std::string url = std::string(API_BASE_URL) + "/tasks";

  // Create a container that stores our HTTP request information
  std::unique_ptr<http_request::HttpRequestContainer> container = http_client_->start(
    url, 
    "GET", 
    "", 
    {{"Authorization", "Bearer " + api_key_}}
  );

  if (!container) {
    ESP_LOGE(TAG, "Failed to create HTTP request");
    if (error_callback) {
      error_callback("Failed to create HTTP request");
    }
    return;
  }

  // Set verify SSL to false - this may be needed depending on your ESP setup
  container->verify_ssl = false;
  
  // Set up our response callback
  container->on_response([this, success_callback, error_callback](int status_code, const std::string &body) {
    if (status_code >= 200 && status_code < 300) {
      // Success - parse the tasks
      std::vector<TodoistTask> tasks;
      std::string parse_error;
      if (parse_tasks_json_internal(body, tasks, parse_error)) {
        ESP_LOGI(TAG, "Successfully fetched %d tasks", tasks.size());
        success_callback(tasks);
      } else {
        ESP_LOGE(TAG, "Error parsing tasks JSON: %s", parse_error.c_str());
        if (error_callback) {
          error_callback("Parse error: " + parse_error);
        }
      }
    } else {
      // Error
      ESP_LOGE(TAG, "Error fetching tasks: HTTP %d - %s", status_code, body.c_str());
      if (error_callback) {
        error_callback("HTTP error " + std::to_string(status_code));
      }
    }
  });

  // Set up error callback
  container->on_error([error_callback](int error_code) {
    ESP_LOGE(TAG, "HTTP request failed: Error code %d", error_code);
    if (error_callback) {
      error_callback("Network error: " + std::to_string(error_code));
    }
  });

  // Send the request
  http_client_->send(std::move(container));
  ESP_LOGI(TAG, "Task fetch request initiated");
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

  if (http_client_ == nullptr) {
    ESP_LOGE(TAG, "HTTP client not initialized");
    if (error_callback) {
      error_callback("HTTP client not initialized");
    }
    return;
  }

  std::string url = std::string(API_BASE_URL) + "/tasks/" + task_id + "/close";

  // Create a container that stores our HTTP request information
  std::unique_ptr<http_request::HttpRequestContainer> container = http_client_->start(
    url, 
    "POST", 
    "", 
    {
      {"Authorization", "Bearer " + api_key_},
      {"Content-Length", "0"} // Important for POST with empty body
    }
  );

  if (!container) {
    ESP_LOGE(TAG, "Failed to create HTTP request");
    if (error_callback) {
      error_callback("Failed to create HTTP request");
    }
    return;
  }

  // Set verify SSL to false
  container->verify_ssl = false;
  
  // Set up our response callback
  container->on_response([success_callback, error_callback, task_id](int status_code, const std::string &body) {
    // Todoist returns 204 No Content on success
    if (status_code == 204) {
      ESP_LOGI(TAG, "Successfully completed task %s", task_id.c_str());
      success_callback(true);
    } else {
      ESP_LOGE(TAG, "Error completing task: HTTP %d - %s", status_code, body.c_str());
      if (error_callback) {
        error_callback("HTTP error " + std::to_string(status_code));
      } else {
        success_callback(false);
      }
    }
  });

  // Set up error callback
  container->on_error([error_callback](int error_code) {
    ESP_LOGE(TAG, "HTTP request failed: Error code %d", error_code);
    if (error_callback) {
      error_callback("Network error: " + std::to_string(error_code));
    }
  });

  // Send the request
  http_client_->send(std::move(container));
  ESP_LOGI(TAG, "Task completion request initiated for task %s", task_id.c_str());
}

// Internal function to handle JSON parsing without exceptions
bool TodoistApi::parse_tasks_json_internal(const std::string &json, std::vector<TodoistTask>& tasks, std::string& error_message) {
  tasks.clear();
  DynamicJsonDocument doc(32768); // Adjust size as needed

  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    ESP_LOGE(TAG, "Failed to parse JSON: %s", error.c_str());
    error_message = std::string("JSON parse error: ") + error.c_str();
    return false;
  }

  JsonArray array = doc.as<JsonArray>();
  if (array.isNull()) {
      ESP_LOGE(TAG, "JSON is not an array");
      error_message = "JSON is not an array";
      return false;
  }

  for (JsonVariant task_json : array) {
    if (!task_json.is<JsonObject>()) continue; // Skip non-object elements

    TodoistTask task;
    JsonObject obj = task_json.as<JsonObject>();

    if (obj["id"].is<const char*>()) task.id = obj["id"].as<std::string>();
    if (obj["content"].is<const char*>()) task.content = obj["content"].as<std::string>();
    if (obj["description"].is<const char*>()) task.description = obj["description"].as<std::string>();
    if (obj["project_id"].is<const char*>()) task.project_id = obj["project_id"].as<std::string>();
    if (obj["section_id"].is<const char*>()) task.section_id = obj["section_id"].as<std::string>();
    if (obj["parent_id"].is<const char*>()) task.parent_id = obj["parent_id"].as<std::string>();

    // Parse due date safely
    if (obj["due"].is<JsonObject>()) {
        JsonObject due_obj = obj["due"];
        if (due_obj["date"].is<const char*>()) task.due_date = due_obj["date"].as<std::string>();
        if (due_obj["string"].is<const char*>()) task.due_string = due_obj["string"].as<std::string>();
    }

    // Parse priority safely
    if (obj["priority"].is<int>()) {
        int priority = obj["priority"].as<int>();
        switch (priority) {
          case 1: task.priority = PRIORITY_4; break; // p4 in Todoist
          case 2: task.priority = PRIORITY_3; break; // p3 in Todoist
          case 3: task.priority = PRIORITY_2; break; // p2 in Todoist
          case 4: task.priority = PRIORITY_1; break; // p1 in Todoist
          default: task.priority = PRIORITY_4; break;
        }
    } else {
        task.priority = PRIORITY_4; // Default if not present or wrong type
    }

    tasks.push_back(task);
  }

  return true;
}

// Keep the original parse_tasks_json but make it call the internal one
// This maintains the original interface but removes exception throwing
std::vector<TodoistTask> TodoistApi::parse_tasks_json(const std::string &json) {
    std::vector<TodoistTask> tasks;
    std::string error_message;
    if (!parse_tasks_json_internal(json, tasks, error_message)) {
        // Log the error but return an empty vector instead of throwing
        ESP_LOGE(TAG, "parse_tasks_json failed: %s", error_message.c_str());
    }
    return tasks;
}

}  // namespace todoist
}  // namespace esphome
