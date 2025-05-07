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

  // Forceer een grote garbage collection voor we beginnen
  // ESP-IDF specifieke memory debug info
  ESP_LOGI(TAG, "Free heap before UI init: %d", esp_get_free_heap_size());

  // Zorg ervoor dat alle oude UI elementen worden verwijderd
  lv_obj_clean(lv_scr_act());
  
  // Verwijder de witte rand rond het scherm door de achtergrond expliciet in te stellen
  lv_obj_set_style_border_width(lv_scr_act(), 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x303030), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_pad_all(lv_scr_act(), 0, LV_PART_MAIN);

  // Zeer eenvoudige container maken zonder extra stijlen
  main_container_ = lv_obj_create(lv_scr_act());
  if (main_container_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create main container");
    this->mark_failed(); // Mark component as failed if UI setup fails
    return;
  }
  
  // Minimale styling - alleen wat echt nodig is
  lv_obj_set_size(main_container_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(main_container_, 0, 0);
  lv_obj_set_style_bg_color(main_container_, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(main_container_, 0, LV_PART_MAIN | LV_STATE_DEFAULT); // Geen rand
  lv_obj_set_style_pad_all(main_container_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_clear_flag(main_container_, LV_OBJ_FLAG_SCROLLABLE);

  // Header is volledig verwijderd

  // Geheugenefficiënt lijst maken
  task_list_ = lv_list_create(main_container_);
  if (task_list_ == nullptr) {
    ESP_LOGE(TAG, "Failed to create task list");
    this->mark_failed();
    return;
  }
  
  // Zorg dat de takenlijst de volledige ruimte inneemt (geen header meer)
  lv_obj_set_size(task_list_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_pos(task_list_, 0, 0); // Start vanaf bovenkant
  lv_obj_set_style_bg_color(task_list_, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_row(task_list_, 8, LV_PART_MAIN | LV_STATE_DEFAULT); // Verhoog ruimte tussen items
  lv_obj_set_style_pad_column(task_list_, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(task_list_, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
  
  // Maak het scrollen mogelijk voor de lijst
  lv_obj_set_style_pad_bottom(task_list_, 20, LV_PART_MAIN | LV_STATE_DEFAULT); // Extra ruimte onderaan
  lv_obj_clear_flag(task_list_, LV_OBJ_FLAG_SCROLL_ELASTIC); // Verwijder elastisch scrollen
  lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0); // Achtergrond volledig ondoorzichtig

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

  // Log geheugengebruik
  ESP_LOGI(TAG, "Free heap after UI setup: %d", esp_get_free_heap_size());

  // Vertraag de eerste fetch om systeem tijd te geven om op te starten 
  last_update_ = millis() / 1000; // Stel de laatste update in op nu
  fetch_tasks_(); // Start de eerste fetch
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
  
  // Tijdelijke label om aan te geven dat een taak wordt voltooid
  static bool completing_task = false;
  
  // Voeg abort mechanisme toe om multiple requests te voorkomen
  if (completing_task) {
    ESP_LOGI(TAG, "Task completion still in progress, waiting...");
    return;
  }
  
  api_->fetch_tasks([this](std::vector<TodoistTask> tasks) {
    ESP_LOGI(TAG, "Task fetch complete with %d tasks", tasks.size());
    this->tasks_ = tasks;
    this->render_tasks_();
    this->show_loading_(false);
    completing_task = false; // Reset de flag
  }, [this](std::string error) {
    ESP_LOGE(TAG, "Failed to fetch tasks: %s", error.c_str());
    this->show_error_("Connection error: " + error);
    completing_task = false; // Reset de flag ook in geval van fouten
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

  // Beperk het aantal taken dat we tegelijk tonen als we geheugendruk hebben
  const size_t MAX_TASKS_PER_SECTION = 5; // Maximum taken per sectie
  
  // Filter voor inbox: alleen overdue taken, taken voor vandaag, en taken voor morgen
  std::vector<TodoistTask> overdue_tasks;  // Over tijd
  std::vector<TodoistTask> today_tasks;    // Vandaag
  std::vector<TodoistTask> tomorrow_tasks; // Morgen (alleen zichtbaar bij scrollen)
  
  for (const auto &task : tasks_) {
    if (task.is_overdue() && overdue_tasks.size() < MAX_TASKS_PER_SECTION) {
      overdue_tasks.push_back(task);
    } else if (task.is_due_today() && today_tasks.size() < MAX_TASKS_PER_SECTION) {
      today_tasks.push_back(task);
    } else if (task.is_due_tomorrow() && tomorrow_tasks.size() < MAX_TASKS_PER_SECTION) {
      tomorrow_tasks.push_back(task);
    }
  }
  

  
  // Toon waarschuwingen als we taken hebben weggelaten
  if (overdue_tasks.size() == MAX_TASKS_PER_SECTION) {
    ESP_LOGW(TAG, "Limiting overdue tasks display to %d items", MAX_TASKS_PER_SECTION);
  }
  
  if (today_tasks.size() == MAX_TASKS_PER_SECTION) {
    ESP_LOGW(TAG, "Limiting today's tasks display to %d items", MAX_TASKS_PER_SECTION);
  }

  // Header is verwijderd, dus geen header update meer nodig
  
  if (overdue_tasks.empty() && today_tasks.empty() && tomorrow_tasks.empty()) {
    // Geen taken in de inbox
    lv_obj_t *no_tasks = lv_label_create(task_list_);
    if (no_tasks) {
      lv_label_set_text(no_tasks, "Inbox leeg!");
      lv_obj_set_style_text_color(no_tasks, lv_color_hex(0xCCCCCC), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_font(no_tasks, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT); // Aangepast naar 16
      lv_obj_center(no_tasks);
    }
    return;
  }

  // Voeg een sectieheader toe voor overdue taken als die er zijn
  if (!overdue_tasks.empty()) {
    lv_obj_t *header = lv_label_create(task_list_);
    if (header) {
      lv_label_set_text(header, "OVER DE TIJD");
      lv_obj_set_style_text_color(header, lv_color_hex(0xFF5555), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_font(header, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_width(header, LV_PCT(100));
      lv_obj_set_style_pad_top(header, 5, 0);
      lv_obj_set_style_pad_bottom(header, 5, 0);
    }
    
    // Toon overdue taken
    for (const auto &task : overdue_tasks) {
      add_task_item_(task, true); // overdue = true
    }
  }

  // Voeg een sectieheader toe voor taken van vandaag als die er zijn
  if (!today_tasks.empty()) {
    lv_obj_t *header = lv_label_create(task_list_);
    if (header) {
      lv_label_set_text(header, "VANDAAG");
      lv_obj_set_style_text_color(header, lv_color_hex(0x55FF55), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_font(header, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_width(header, LV_PCT(100));
      lv_obj_set_style_pad_top(header, 10, 0);
      lv_obj_set_style_pad_bottom(header, 5, 0);
    }
    
    // Toon taken van vandaag
    for (const auto &task : today_tasks) {
      add_task_item_(task, false); // overdue = false
    }
  }

  // Voeg een sectieheader toe voor taken van morgen als die er zijn
  if (!tomorrow_tasks.empty()) {
    lv_obj_t *header = lv_label_create(task_list_);
    if (header) {
      lv_label_set_text(header, "MORGEN");
      lv_obj_set_style_text_color(header, lv_color_hex(0x4488FF), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_style_text_font(header, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_set_width(header, LV_PCT(100));
      lv_obj_set_style_pad_top(header, 10, 0);
      lv_obj_set_style_pad_bottom(header, 5, 0);
    }
    
    // Toon taken van morgen
    for (const auto &task : tomorrow_tasks) {
      add_task_item_(task, false); // overdue = false
    }
  }

  ESP_LOGI(TAG, "Tasks rendered successfully: %d overdue, %d today, %d tomorrow", 
           overdue_tasks.size(), today_tasks.size(), tomorrow_tasks.size());
}

// Nieuwe helper methode om taak items toe te voegen met consistente styling en complete knop
void TodoistComponent::add_task_item_(const TodoistTask &task, bool is_overdue) {
  // Create list item for task
  lv_obj_t *list_btn = lv_list_add_btn(task_list_, nullptr, task.content.c_str());
  if (list_btn == nullptr) {
    ESP_LOGE(TAG, "Failed to create list button for task %s", task.id.c_str());
    return;
  }

  // Verbeter de opmaak van taakitems
  lv_obj_set_style_bg_color(list_btn, lv_color_hex(0x404040), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_bg_opa(list_btn, LV_OPA_COVER, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_height(list_btn, LV_SIZE_CONTENT);  // Automatische hoogte op basis van inhoud
  lv_obj_set_width(list_btn, LV_PCT(98));  // Bijna volledige breedte

  // Prioriteitsindicator links
  lv_obj_set_style_border_side(list_btn, LV_BORDER_SIDE_LEFT, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(list_btn, 5, LV_PART_MAIN | LV_STATE_DEFAULT); // Smaller
  lv_obj_set_style_border_color(list_btn, lv_color_hex(task.get_priority_color()), LV_PART_MAIN | LV_STATE_DEFAULT);

  // Vergroot de tekstgrootte van de taaknaam
  lv_obj_t *label = lv_obj_get_child(list_btn, 0);
  if (label != nullptr) {
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Gebruik een groter lettertype voor betere leesbaarheid
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Maak de label minder breed om ruimte te maken voor de voltooi-knop en tijd
    lv_obj_set_width(label, LV_PCT(80)); // Verkleind van 85% naar 75% voor extra ruimte
    
    // Voeg extra padding toe aan tekstcontainer
    lv_obj_set_style_pad_top(list_btn, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(list_btn, 8, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(list_btn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(list_btn, 15, LV_PART_MAIN | LV_STATE_DEFAULT);
  }

  // Als de taak een deadline heeft, voeg dan een label toe
  if (!task.due_string.empty()) {
    // Toon tijd voor taken van vandaag
    if (task.is_due_today()) {
      // Haal tijd-informatie uit de due_date (als beschikbaar)
      std::string time_str = task.get_due_time();
      if (!time_str.empty()) {
        lv_obj_t *time_label = lv_label_create(list_btn);
        if (time_label != nullptr) {
          lv_label_set_text(time_label, time_str.c_str());
          lv_obj_set_style_text_color(time_label, lv_color_hex(0x55FF55), LV_PART_MAIN | LV_STATE_DEFAULT);
          lv_obj_set_style_text_font(time_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
          // Positie links van de complete knop
          lv_obj_align(time_label, LV_ALIGN_RIGHT_MID, -45, 0);
        }
      }
    } 
    // Toon due_string voor taken van morgen of later (zoals voorheen)
    else if (!task.is_overdue()) {
      lv_obj_t *due_label = lv_label_create(list_btn);
      if (due_label != nullptr) {
        lv_label_set_text(due_label, task.due_string.c_str());
        lv_obj_set_style_text_color(due_label, lv_color_hex(0xAAAAAA), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(due_label, &lv_font_montserrat_14, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_align(due_label, LV_ALIGN_BOTTOM_RIGHT, -45, -5);
      }
    }
  }

  // Voeg voltooien knop toe aan rechter kant - Fix vinkje symbool
  lv_obj_t *complete_btn = lv_btn_create(list_btn);
  if (complete_btn) {
    lv_obj_set_size(complete_btn, 24, 24); // Kleinere knop
    lv_obj_align(complete_btn, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_obj_set_style_radius(complete_btn, 12, 0); // Halve breedte voor een cirkel
    // Gebruik accentkleur voor de knop
    lv_obj_set_style_bg_color(complete_btn, lv_color_hex(0x2196F3), LV_PART_MAIN | LV_STATE_DEFAULT);
    
    // Gebruik LVGL's ingebouwde symbool in plaats van Unicode vinkje
    lv_obj_t *check_label = lv_label_create(complete_btn);
    if (check_label) {
      lv_label_set_text(check_label, "+"); // Gebruik + in plaats van vinkje (eenvoudiger)
      lv_obj_set_style_text_color(check_label, lv_color_hex(0xFFFFFF), LV_PART_MAIN | LV_STATE_DEFAULT);
      lv_obj_center(check_label);
    }
    
    // Verbeter visuele feedback bij aanraking
    lv_obj_set_style_bg_color(complete_btn, lv_color_hex(0x1976D2), LV_PART_MAIN | LV_STATE_PRESSED);
    
    // Find original task for user data pointer
    const TodoistTask* original_task_ptr = nullptr;
    for(const auto& original_task : tasks_) {
      if (original_task.id == task.id) {
        original_task_ptr = &original_task;
        break;
      }
    }
    
    if (original_task_ptr) {
      // Opslaan van taak pointer in complete button data
      lv_obj_set_user_data(complete_btn, (void*)original_task_ptr);
      
      // Event handler toevoegen voor de voltooien knop
      lv_obj_add_event_cb(complete_btn, [](lv_event_t *e) {
        TodoistComponent *component = static_cast<TodoistComponent*>(lv_event_get_user_data(e));
        const TodoistTask *task_ptr = static_cast<const TodoistTask*>(lv_obj_get_user_data(lv_event_get_current_target(e)));
        
        if (component && task_ptr) {
          ESP_LOGI(TAG, "Complete button clicked for task: %s", task_ptr->id.c_str());
          component->api_->complete_task(task_ptr->id, [component](bool success) {
            if (success) {
              ESP_LOGI(TAG, "Task completion successful, refreshing list.");
              component->fetch_tasks_();
            } else {
              ESP_LOGW(TAG, "Task completion failed.");
            }
          });
        }
      }, LV_EVENT_CLICKED, this);
      
      // Stop propagation van click events, anders opent de taak ook de detailweergave
      lv_obj_add_event_cb(complete_btn, [](lv_event_t *e) {
        lv_event_stop_bubbling(e); // Voorkom dat de klik doorbubbelt naar de parent
      }, LV_EVENT_CLICKED, nullptr);
    }
  }

  // Event handlers voor het openen van details
  lv_obj_add_event_cb(list_btn, task_event_cb_, LV_EVENT_CLICKED, this);
  
  // Find original task for user data pointer
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
  }
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
  lv_obj_t *modal = lv_obj_create(lv_layer_top());
  if (!modal) { ESP_LOGE(TAG, "Failed to create modal"); return; }
  lv_obj_set_size(modal, LV_PCT(90), LV_PCT(70));
  lv_obj_center(modal);
  lv_obj_set_style_bg_color(modal, lv_color_hex(0x303030), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_width(modal, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_border_color(modal, lv_color_hex(task.get_priority_color()), LV_PART_MAIN | LV_STATE_DEFAULT);
  lv_obj_set_style_pad_all(modal, 20, LV_PART_MAIN | LV_STATE_DEFAULT);

  // Task title - aangepast lettertype
  lv_obj_t *title = lv_label_create(modal);
  if (!title) { ESP_LOGE(TAG, "Failed to create modal title"); lv_obj_del(modal); return; }
  lv_label_set_text(title, task.content.c_str());
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT); // Aangepast naar 16
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

  // Complete button - aangepast lettertype
  lv_obj_t *complete_btn = lv_btn_create(modal);
  if (!complete_btn) { ESP_LOGE(TAG, "Failed to create complete button"); lv_obj_del(modal); return; }
  lv_obj_t *complete_label = lv_label_create(complete_btn);
  if (!complete_label) { ESP_LOGE(TAG, "Failed to create complete label"); lv_obj_del(modal); return; }
  lv_label_set_text(complete_label, "Voltooien");
  lv_obj_set_style_text_font(complete_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT); // Aangepast naar 16
  lv_obj_set_size(complete_btn, LV_SIZE_CONTENT, 50); // Maak knop groter
  lv_obj_set_style_pad_all(complete_btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT); // Meer padding
  lv_obj_align(complete_btn, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

  // Close button - aangepast lettertype
  lv_obj_t *close_btn = lv_btn_create(modal);
  if (!close_btn) { ESP_LOGE(TAG, "Failed to create close button"); lv_obj_del(modal); return; }
  lv_obj_t *close_label = lv_label_create(close_btn);
  if (!close_label) { ESP_LOGE(TAG, "Failed to create close label"); lv_obj_del(modal); return; }
  lv_label_set_text(close_label, "Sluiten");
  lv_obj_set_style_text_font(close_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT); // Aangepast naar 16
  lv_obj_set_size(close_btn, LV_SIZE_CONTENT, 50); // Maak knop groter
  lv_obj_set_style_pad_all(close_btn, 10, LV_PART_MAIN | LV_STATE_DEFAULT); // Meer padding
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
