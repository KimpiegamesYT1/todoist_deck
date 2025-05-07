#include "hd_device_sc01_plus.h"

namespace esphome {
namespace hd_device {

static const char *const TAG = "HD_DEVICE";
static lv_disp_draw_buf_t draw_buf;

// Reduced display buffer size for LVGL to optimize memory usage
// Buffer size reduced from 20 to 10 lines to decrease memory consumption
static lv_color_t *buf = nullptr;

LGFX lcd;

// LVGL log callback for debug information
static void lvgl_log_cb(const char * buf) {
#ifdef DEBUG_LVGL
    ESP_LOGD(TAG, "LVGL: %s", buf);
#endif
}

/**
 * @brief Flush pixels to display - optimized for performance with IRAM_ATTR
 * @param disp LVGL display driver
 * @param area Area to update
 * @param color_p Color data to write
 */
void IRAM_ATTR flush_pixels(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint32_t len = w * h;

    lcd.startWrite();
    lcd.setAddrWindow(area->x1, area->y1, w, h);
    lcd.writePixels((uint16_t *)&color_p->full, len, true);
    lcd.endWrite();

    lv_disp_flush_ready(disp);
}

/**
 * @brief Read touchpad input for LVGL
 * @param indev_driver Input device driver
 * @param data Data structure to populate with touch info
 */
void IRAM_ATTR touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data)
{
    uint16_t touchX, touchY;
    bool touched = lcd.getTouch(&touchX, &touchY);

    if (touched) {
        data->point.x = touchX;
        data->point.y = touchY;
        data->state = LV_INDEV_STATE_PR;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void HaDeckDevice::setup() {
    // Log memory usage at startup
    ESP_LOGCONFIG(TAG, "Free memory at startup: %d bytes", esp_get_free_heap_size());
    
    // Allocate display buffer with error handling
    buf = (lv_color_t *)heap_caps_malloc(TFT_HEIGHT * 10 * sizeof(lv_color_t), MALLOC_CAP_DMA);
    if (buf == nullptr) {
        ESP_LOGE(TAG, "Failed to allocate display buffer. System halted.");
        return;
    }
    
    lv_init();
#ifdef DEBUG_LVGL
    lv_log_register_print_cb(lvgl_log_cb);
#endif
    
    // Simplified theme initialization - using default parameters for production
    lv_theme_default_init(nullptr, lv_palette_main(LV_PALETTE_BLUE), 
                          lv_palette_main(LV_PALETTE_RED), 
                          false, LV_FONT_DEFAULT);

    // Initialize display
    if (!lcd.begin()) {
        ESP_LOGE(TAG, "Display initialization failed. System halted.");
        return;
    }

    // Initialize display buffer with optimized size
    lv_disp_draw_buf_init(&draw_buf, buf, nullptr, TFT_HEIGHT * 10);

    // Configure display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.rotated = 1;  // TODO: Make this configurable for different orientations
    disp_drv.sw_rotate = 1;
    disp_drv.flush_cb = flush_pixels;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);
    
    if (!disp) {
        ESP_LOGE(TAG, "Display driver registration failed");
    }

    // Configure touch input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.long_press_time = 1000;
    indev_drv.long_press_repeat_time = 300;
    indev_drv.read_cb = touchpad_read;
    lv_indev_t* touch = lv_indev_drv_register(&indev_drv);
    
    if (!touch) {
        ESP_LOGE(TAG, "Touch driver registration failed");
    }

    // Set initial brightness
    lcd.setBrightness(brightness_);
    
    // Configure screen appearance for clean UI
    lv_obj_t* screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x303030), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(screen, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    
    ESP_LOGCONFIG(TAG, "Free memory after setup: %d bytes", esp_get_free_heap_size());
}

void HaDeckDevice::loop() {
    lv_timer_handler();

#ifdef DEBUG_MEMORY
    static unsigned long last_memory_check = 0;
    unsigned long ms = millis();
    if (ms - last_memory_check > 60000) {
        last_memory_check = ms;
        ESP_LOGD(TAG, "Free memory: %d bytes", esp_get_free_heap_size());
    }
#endif
}

float HaDeckDevice::get_setup_priority() const { 
    return setup_priority::DATA; 
}

uint8_t HaDeckDevice::get_brightness() {
    return brightness_;
}

void HaDeckDevice::set_brightness(uint8_t value) {
    brightness_ = value;
    lcd.setBrightness(brightness_);
}

void HaDeckDevice::set_todoist_api_key(const std::string &api_key) {
    todoist_api_key_ = api_key;
    ESP_LOGCONFIG(TAG, "Todoist API key set");
}


}  // namespace hd_device
}  // namespace esphome
