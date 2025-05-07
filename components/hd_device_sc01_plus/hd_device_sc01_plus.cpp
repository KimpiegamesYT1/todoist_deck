#include "hd_device_sc01_plus.h"

namespace esphome {
namespace hd_device {

static const char *const TAG = "HD_DEVICE";
static lv_disp_draw_buf_t draw_buf;

// Verkleinen van de displaybuffer voor LVGL
static lv_color_t *buf = (lv_color_t *)heap_caps_malloc(TFT_HEIGHT * 10 * sizeof(lv_color_t), MALLOC_CAP_DMA); // Van 20 naar 10

LGFX lcd;

void IRAM_ATTR flush_pixels(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint32_t len = w * h;

    lcd.startWrite();                            /* Start new TFT transaction */
    lcd.setAddrWindow(area->x1, area->y1, w, h); /* set the working window */
    lcd.writePixels((uint16_t *)&color_p->full, len, true);
    lcd.endWrite();                              /* terminate TFT transaction */

    lv_disp_flush_ready(disp);
}

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
    // Log geheugengebruik bij start
    ESP_LOGCONFIG(TAG, "Free memory at startup: %d bytes", esp_get_free_heap_size());
    
    // Initialiseer eerst de LCD en zet direct de achtergrond op zwart
    lcd.init();
    lcd.fillScreen(0x000000);  // Vul scherm direct met zwart
    lcd.setBrightness(brightness_);
    
    lv_init();
    
    // Verwijder witte rand rond het scherm door de achtergrond expliciet in te stellen
    lv_obj_set_style_border_width(lv_scr_act(), 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x303030), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_pad_all(lv_scr_act(), 0, LV_PART_MAIN);

    // Kleinere draw buffer
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, TFT_HEIGHT * 10);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = TFT_WIDTH;
    disp_drv.ver_res = TFT_HEIGHT;
    disp_drv.rotated = 1;
    disp_drv.sw_rotate = 1;
    disp_drv.flush_cb = flush_pixels;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    // Zet default schermkleur
    lv_disp_set_bg_color(disp, lv_color_hex(0x303030));
    lv_disp_set_bg_opa(disp, LV_OPA_COVER);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.long_press_time = 1000;
    indev_drv.long_press_repeat_time = 300;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
    
    // Verwijder het initialisatie-label zodat het niet interfereert met de Todoist UI
    // Het Todoist component zal zijn eigen UI bouwen
    
    // Log geheugengebruik na setup
    ESP_LOGCONFIG(TAG, "Free memory after setup: %d bytes", esp_get_free_heap_size());
}

void HaDeckDevice::loop() {
    lv_timer_handler();

    unsigned long ms = millis();
    if (ms - time_ > 60000) {
        time_ = ms;
        ESP_LOGCONFIG(TAG, "Free memory: %d bytes", esp_get_free_heap_size());
    }
}

float HaDeckDevice::get_setup_priority() const { return setup_priority::DATA; }

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