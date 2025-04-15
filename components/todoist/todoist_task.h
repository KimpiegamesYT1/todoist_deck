#pragma once

#include "esphome/core/component.h"
#include <string>

namespace esphome {
namespace todoist {

enum TaskPriority {
  PRIORITY_1 = 1,  // Highest priority (p1)
  PRIORITY_2 = 2,
  PRIORITY_3 = 3,
  PRIORITY_4 = 4   // Lowest priority (p4)
};

class TodoistTask {
 public:
  std::string id;
  std::string content;
  std::string description;
  std::string project_id;
  std::string section_id;
  std::string parent_id;
  std::string due_date;
  std::string due_string;
  TaskPriority priority = PRIORITY_4;
  bool is_completed = false;
  bool is_deleted = false;
  
  // Helper methods to check due status
  bool is_due_today() const;
  bool is_overdue() const;
  bool is_due_tomorrow() const; // Nieuwe methode
  
  // Get color based on priority
  uint32_t get_priority_color() const;
};

}  // namespace todoist
}  // namespace esphome
