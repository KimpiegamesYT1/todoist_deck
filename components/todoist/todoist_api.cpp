#include "todoist_api.h"
#include "esphome/core/log.h"
#include <ArduinoJson.h>

namespace esphome {
namespace todoist {

static const char *const TAG = "todoist.api";
static const char *const API_BASE_URL = "https://api.todoist.com/rest/v2";

TodoistApi::TodoistApi() {
  // Nothing to initialize
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

  std::string url = std::string(API_BASE_URL) + "/tasks";
  std::string response;
  std::string error_message;
  
  // Make the HTTP request
  if (!do_http_request(url, "GET", response, error_message)) {
    ESP_LOGE(TAG, "Failed to fetch tasks: %s", error_message.c_str());
    if (error_callback) {
      error_callback(error_message);
    }
    return;
  }
  
  // Parse the response
  std::vector<TodoistTask> tasks;
  std::string parse_error;
  if (parse_tasks_json_internal(response, tasks, parse_error)) {
    ESP_LOGI(TAG, "Successfully fetched %d tasks", tasks.size());
    success_callback(tasks);
  } else {
    ESP_LOGE(TAG, "Error parsing tasks JSON: %s", parse_error.c_str());
    if (error_callback) {
      error_callback("Parse error: " + parse_error);
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

  std::string url = std::string(API_BASE_URL) + "/tasks/" + task_id + "/close";
  std::string response;
  std::string error_message;
  
  // Make the HTTP request
  if (!do_http_request(url, "POST", response, error_message)) {
    ESP_LOGE(TAG, "Failed to complete task: %s", error_message.c_str());
    if (error_callback) {
      error_callback(error_message);
    } else {
      success_callback(false);
    }
    return;
  }
  
  ESP_LOGI(TAG, "Taak %s succesvol als voltooid gemarkeerd", task_id.c_str());
  success_callback(true);
}

bool TodoistApi::do_http_request(const std::string& url, 
                                 const std::string& method,
                                 std::string& response,
                                 std::string& error_message) {
  http_.begin(url.c_str());
  
  // Voeg standaard headers toe
  http_.addHeader("Authorization", ("Bearer " + api_key_).c_str());
  http_.addHeader("Content-Type", "application/json");
  
  // Voeg extra headers toe voor het beheersen van cache en compressie
  http_.addHeader("Cache-Control", "no-cache");
  
  // Als het een POST is met lege body, voeg Content-Length toe
  if (method == "POST") {
    http_.addHeader("Content-Length", "0");
  }
  
  // Perform the request
  int httpResponseCode;
  if (method == "GET") {
    httpResponseCode = http_.GET();
  } else if (method == "POST") {
    httpResponseCode = http_.POST("");
  } else {
    http_.end();
    error_message = "Unsupported method: " + method;
    return false;
  }
  
  // Check response
  if (httpResponseCode < 200 || httpResponseCode >= 300) {
    if (httpResponseCode > 0) {
      error_message = "HTTP error code: " + std::to_string(httpResponseCode);
      // Fix string concatenation error - use append or separate concatenations
      if (http_.getSize() > 0) {
        error_message += " - ";
        error_message += http_.getString().c_str();
      }
    } else {
      error_message = "Connection failed";
    }
    http_.end();
    return false;
  }
  
  // Get the response
  if (httpResponseCode != 204) { // 204 No Content has no body
    response = http_.getString().c_str();
  }
  
  http_.end();
  return true;
}

// Drastisch verkleinen van de JSON buffer
bool TodoistApi::parse_tasks_json_internal(const std::string &json, std::vector<TodoistTask>& tasks, std::string& error_message) {
  tasks.clear();
  
  if (json.empty()) {
    error_message = "Empty JSON response";
    return false;
  }

  // JSON buffer ingesteld op 24KB
  DynamicJsonDocument doc(24576); // 24KB 
  
  // Gebruik geen extra options die meer geheugen gebruiken
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    ESP_LOGE(TAG, "Failed to parse JSON: %s", error.c_str());
    error_message = std::string("JSON parse error: ") + error.c_str();
    return false;
  }

  // Parseer niet alle taken tegelijk maar beperk tot max. 10 om geheugen te sparen
  int task_count = 0;
  const int MAX_TASKS = 10;

  JsonArray array = doc.as<JsonArray>();
  if (array.isNull()) {
      ESP_LOGE(TAG, "JSON is not an array");
      error_message = "JSON is not an array";
      return false;
  }

  for (JsonVariant task_json : array) {
    if (task_count >= MAX_TASKS) break; // Hard limit op aantal taken
    
    if (!task_json.is<JsonObject>()) continue; // Skip non-object elements

    TodoistTask task;
    JsonObject obj = task_json.as<JsonObject>();

    // Alleen de echt nodige velden ophalen
    if (obj["id"].is<const char*>()) task.id = obj["id"].as<std::string>();
    if (obj["content"].is<const char*>()) task.content = obj["content"].as<std::string>();
    
    // Beschrijving is groot en vaak onnodig - alleen ophalen als heel kort
    if (obj["description"].is<const char*>()) {
      const char* desc = obj["description"].as<const char*>();
      if (strlen(desc) < 100) { // Alleen korte beschrijvingen
        task.description = desc;
      }
    }
    
    // Alleen essentiële velden
    if (obj["project_id"].is<const char*>()) task.project_id = obj["project_id"].as<std::string>();
    // section_id en parent_id worden niet geparsed om geheugen te besparen
    // if (obj["section_id"].is<const char*>()) task.section_id = obj["section_id"].as<std::string>();
    // if (obj["parent_id"].is<const char*>()) task.parent_id = obj["parent_id"].as<std::string>();
    
    // Due date processing
    if (obj["due"].is<JsonObject>()) {
        JsonObject due_obj = obj["due"];
        if (due_obj["date"].is<const char*>()) task.due_date = due_obj["date"].as<std::string>();
        if (due_obj["string"].is<const char*>()) task.due_string = due_obj["string"].as<std::string>();
    }

    // Parse priority
    if (obj["priority"].is<int>()) {
        int priority = obj["priority"].as<int>();
        switch (priority) {
          case 1: task.priority = PRIORITY_4; break;
          case 2: task.priority = PRIORITY_3; break;
          case 3: task.priority = PRIORITY_2; break;
          case 4: task.priority = PRIORITY_1; break;
          default: task.priority = PRIORITY_4; break;
        }
    } else {
        task.priority = PRIORITY_4;
    }

    tasks.push_back(task);
    task_count++;
  }

  return true;
}

// Forward-facing method that doesn't throw
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
