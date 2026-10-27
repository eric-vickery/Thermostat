#include "display.hpp"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include "esp_lcd_gc9a01.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"

#include "ui.h"
#include "screens.h"

static const char *TAG = "Display";

#define PIN_BL          46
#define BL_LEDC_TIMER   LEDC_TIMER_0
#define BL_LEDC_CHANNEL LEDC_CHANNEL_0
#define BL_LEDC_FREQ_HZ 5000

#define LCD_HOST        SPI2_HOST
#define PIN_SCLK        GPIO_NUM_10
#define PIN_MOSI        GPIO_NUM_11
#define PIN_CS          GPIO_NUM_9
#define PIN_DC          GPIO_NUM_3
#define PIN_RST         GPIO_NUM_14

#define LCD_H_RES       240
#define LCD_V_RES       240
#define LCD_PIXEL_CLK   (30 * 1000 * 1000)
#define CHUNK_LINES     40      // rows per draw call (keeps buffer small)

#define TOUCH_I2C_NUM          I2C_NUM_0
#define PIN_NUM_SDA            GPIO_NUM_6
#define PIN_NUM_SCL            GPIO_NUM_7
#define PIN_NUM_TOUCH_RST      GPIO_NUM_13
#define PIN_NUM_TOUCH_INT      GPIO_NUM_5

void Display::init_panel()
{
    // SPI bus
    spi_bus_config_t bus_cfg = {};
    bus_cfg.sclk_io_num = PIN_SCLK;
    bus_cfg.mosi_io_num = PIN_MOSI;
    bus_cfg.miso_io_num = -1;
    bus_cfg.quadwp_io_num = -1;
    bus_cfg.quadhd_io_num = -1;
    bus_cfg.max_transfer_sz = LCD_H_RES * CHUNK_LINES * sizeof(uint16_t);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    // Panel IO (SPI)
    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.dc_gpio_num = PIN_DC;
    io_cfg.cs_gpio_num = PIN_CS;
    io_cfg.pclk_hz = LCD_PIXEL_CLK;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;
    io_cfg.spi_mode = 0;
    io_cfg.trans_queue_depth = 10;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST,
                                             &io_cfg, &io_handle_));

    // GC9A01 panel driver
    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = PIN_RST;
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_cfg.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle_, &panel_cfg, &panel_handle_));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle_));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle_));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle_, true));  // GC9A01 usually needs this
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle_, true));
    ESP_LOGI(TAG, "Panel initialized");
}

/* ------------------------------------------------------------------------
 * Backlight (LEDC PWM)
 * ------------------------------------------------------------------------ */
void Display::init_backlight()
{
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.timer_num = BL_LEDC_TIMER;
    timer.duty_resolution = LEDC_TIMER_8_BIT;
    timer.freq_hz = BL_LEDC_FREQ_HZ;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t channel = {};
    channel.speed_mode = LEDC_LOW_SPEED_MODE;
    channel.channel = BL_LEDC_CHANNEL;
    channel.timer_sel = BL_LEDC_TIMER;
    channel.gpio_num = PIN_BL;
    channel.duty = 0;
    channel.hpoint = 0;
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
    ledc_fade_func_install(0);
}

void Display::set_backlight(uint32_t percent, uint32_t fade_ms)
{
    if (percent > 100) {
        percent = 100;
    }

    backlight_brightness_ = percent;
    uint32_t duty = (255 * (uint32_t)percent) / 100;
    if (fade_ms > 0) {
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty, fade_ms);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, LEDC_FADE_NO_WAIT);
    } else {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
    }
}

void Display::init_touch()
{
    // Initialize CST816S Touch via I2C
    i2c_master_bus_config_t bus_config = {};
    bus_config.clk_source = I2C_CLK_SRC_DEFAULT;
    bus_config.i2c_port = TOUCH_I2C_NUM; // Or -1 to let ESP-IDF auto-allocate an available port
    bus_config.scl_io_num = (gpio_num_t)PIN_NUM_SCL;
    bus_config.sda_io_num = (gpio_num_t)PIN_NUM_SDA;
    bus_config.glitch_ignore_cnt = 7;
    bus_config.flags.enable_internal_pullup = true;

    i2c_master_bus_handle_t bus_handle;
    ESP_LOGI(TAG, "Calling i2c_new_master_bus");
    ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    // The vendor macro uses out-of-order designated initializers, which C++ rejects
    esp_lcd_panel_io_i2c_config_t tp_io_config = {};
    tp_io_config.dev_addr = ESP_LCD_TOUCH_IO_I2C_CST816S_ADDRESS;
    tp_io_config.scl_speed_hz = 100000;
    tp_io_config.control_phase_bytes = 1;
    tp_io_config.dc_bit_offset = 0;
    tp_io_config.lcd_cmd_bits = 8;
    tp_io_config.lcd_param_bits = 0;
    tp_io_config.flags.dc_low_on_data = 0;
    tp_io_config.flags.disable_control_phase = 1;
    ESP_LOGI(TAG, "Calling esp_lcd_new_panel_io_i2c");
    esp_lcd_new_panel_io_i2c(bus_handle, &tp_io_config, &tp_io_handle);

    esp_lcd_touch_config_t tp_cfg = {};
    tp_cfg.x_max = LCD_H_RES;
    tp_cfg.y_max = LCD_V_RES;
    tp_cfg.rst_gpio_num = (gpio_num_t)PIN_NUM_TOUCH_RST;
    tp_cfg.int_gpio_num = (gpio_num_t)PIN_NUM_TOUCH_INT;
    tp_cfg.levels.reset = 0;
    tp_cfg.levels.interrupt = 0;
    tp_cfg.flags.swap_xy = 0;
    tp_cfg.flags.mirror_x = 0;
    tp_cfg.flags.mirror_y = 0;
    ESP_LOGI(TAG, "Calling esp_lcd_touch_new_i2c_cst816s");
    esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp_handle_);
}

static void eez_ui_tick_cb(lv_timer_t * timer) {
    if (lvgl_port_lock(0)) {
        ui_tick(); // Call EEZ Studio's tick function
        lvgl_port_unlock();
    }
}

void Display::init_lvgl(lv_event_cb_t mode_button_cb)
{
    // Initialize LVGL & Port Layer
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    // Add display to LVGL
    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle = io_handle_;
    disp_cfg.panel_handle = panel_handle_;
    disp_cfg.buffer_size = LCD_H_RES * CHUNK_LINES;
    disp_cfg.double_buffer = true;
    disp_cfg.hres = LCD_H_RES;
    disp_cfg.vres = LCD_V_RES;
    disp_cfg.monochrome = false;
    disp_cfg.flags.buff_dma = true;
    disp_cfg.flags.swap_bytes = true;
    lv_display_t *lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    // Add Touchpad Input to LVGL
    lvgl_port_touch_cfg_t touch_cfg = {};
    touch_cfg.disp = lvgl_disp;
    touch_cfg.handle = tp_handle_;
    lvgl_port_add_touch(&touch_cfg);

    // Init the EEZ ui and setup the timer callback
    if (lvgl_port_lock(0)) {
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle_, true, false));

        // Initialize EEZ Studio UI
        ui_init();

        // Setup the heat on indicator ring
        lv_obj_set_style_outline_opa(objects.main_container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_radius(objects.main_container, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_outline_width(objects.main_container, 4, LV_PART_MAIN);
        // Push the outline 3px INWARD so it stays inside your 240x240 screen boundary
        lv_obj_set_style_outline_pad(objects.main_container, -4, LV_PART_MAIN);
        lv_obj_set_style_outline_color(objects.main_container, lv_color_hex(0xFF4D4D), LV_PART_MAIN);

        lv_obj_add_event_cb(objects.mode_button, mode_button_cb, LV_EVENT_CLICKED, NULL);

        // Register EEZ Studio's tick to execute natively every 10ms
        // inside the official esp_lvgl_port task thread
        lv_timer_create(eez_ui_tick_cb, 10, NULL);

        // Release the lock so the port can begin rendering
        lvgl_port_unlock();
    }
}

/* Fill the screen in chunks of rows to keep RAM use low. */
void Display::fill(uint16_t color)
{
    if (panel_handle_ == NULL) {
        ESP_LOGI(TAG, "No panel_handle.");
        return;
    }
    color = __builtin_bswap16(color);
    size_t buf_size = LCD_H_RES * CHUNK_LINES * sizeof(uint16_t);
    uint16_t *buf = (uint16_t *)heap_caps_malloc(buf_size, MALLOC_CAP_DMA);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate line buffer");
        return;
    }
    for (size_t i = 0; i < LCD_H_RES * CHUNK_LINES; i++) {
        buf[i] = color;
    }

    for (int y = 0; y < LCD_V_RES; y += CHUNK_LINES) {
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle_, 0, y, LCD_H_RES, y + CHUNK_LINES, buf));
    }

    heap_caps_free(buf);
}

// Custom animation callback to apply opacity to the outline
static void outline_opa_anim_cb(lv_anim_t *animation_object, int32_t value)
{
    lv_obj_t *obj = (lv_obj_t *)animation_object->var;
    lv_obj_set_style_outline_opa(obj, value, LV_PART_MAIN);
}

void Display::set_heat_ring(bool enable)
{
    // The fade uses a custom exec callback, which lv_anim_delete() only matches when exec_cb is
    // NULL, so remove every animation on the ring instead of leaving old fades running
    lv_anim_delete(objects.main_container, NULL);

    lv_anim_t animation_object;
    lv_anim_init(&animation_object);
    lv_anim_set_var(&animation_object, objects.main_container);
    lv_anim_set_custom_exec_cb(&animation_object, outline_opa_anim_cb);

    // 300 milliseconds duration for a snappy but smooth transition
    lv_anim_set_duration(&animation_object, 300);

    if (enable) {
        // Ensure properties are set before fading in
        lv_obj_set_style_outline_width(objects.main_container, 3, LV_PART_MAIN);
        lv_obj_set_style_outline_pad(objects.main_container, -3, LV_PART_MAIN);
        lv_obj_set_style_outline_color(objects.main_container, lv_color_hex(0xFF0000), LV_PART_MAIN); // Red ring

        // Fade from current opacity (or 0) up to 255 (LV_OPA_COVER)
        uint32_t current_opa = lv_obj_get_style_outline_opa(objects.main_container, LV_PART_MAIN);
        lv_anim_set_values(&animation_object, current_opa, LV_OPA_COVER);
    } else {
        // Fade from current opacity down to 0 (LV_OPA_TRANSP)
        uint32_t current_opa = lv_obj_get_style_outline_opa(objects.main_container, LV_PART_MAIN);
        lv_anim_set_values(&animation_object, current_opa, LV_OPA_TRANSP);
    }

    lv_anim_start(&animation_object);
}
