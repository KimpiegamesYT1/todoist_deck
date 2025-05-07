#include "todoist_task.h"
#include "esphome/core/log.h"
#include <ctime>

namespace esphome {
namespace todoist {

static const char *const TAG = "todoist.task";

bool TodoistTask::is_due_today() const {
  if (due_date.empty())
    return false;
    
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char today[11]; // YYYY-MM-DD\0
  strftime(today, sizeof(today), "%Y-%m-%d", &timeinfo);
  
  return due_date.substr(0, 10) == today;
}

bool TodoistTask::is_overdue() const {
  if (due_date.empty())
    return false;
    
  time_t now;
  time(&now);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char today[11]; // YYYY-MM-DD\0
  strftime(today, sizeof(today), "%Y-%m-%d", &timeinfo);
  
  return due_date.substr(0, 10) < today;
}

bool TodoistTask::is_due_tomorrow() const {
  if (due_date.empty())
    return false;
    
  time_t now;
  time(&now);
  now += 24*60*60; // Voeg één dag toe aan huidige tijd
  struct tm tomorrow;
  localtime_r(&now, &tomorrow);
  
  char tomorrow_date[11]; // YYYY-MM-DD\0
  strftime(tomorrow_date, sizeof(tomorrow_date), "%Y-%m-%d", &tomorrow);
  
  return due_date.substr(0, 10) == tomorrow_date;
}

std::string TodoistTask::get_due_time() const {
  if (due_date.empty() || due_date.length() <= 10)
    return "";
    
  // De due_date in Todoist API heeft doorgaans het formaat "YYYY-MM-DDThh:mm:ss"
  // We willen alleen het "hh:mm" deel extraheren
  size_t pos = due_date.find('T');
  if (pos != std::string::npos && pos + 6 <= due_date.length()) {
    return due_date.substr(pos + 1, 5); // Alleen "hh:mm" pakken
  }
  
  return "";
}

uint32_t TodoistTask::get_priority_color() const {
  // Todoist colors in LVGL format (0xRRGGBB)
  switch (priority) {
    case PRIORITY_1: return 0xFF2B2B; // Red (p1)
    case PRIORITY_2: return 0xFB8C00; // Orange (p2)
    case PRIORITY_3: return 0x4073FF; // Blue (p3)
    case PRIORITY_4: 
    default:         return 0x808080; // Gray (p4)
  }
}

}  // namespace todoist
}  // namespace esphome
