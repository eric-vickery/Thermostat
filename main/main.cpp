// #define THERMOSTAT_WIFI 1
// Erase the saved WiFi credentials at every boot so provisioning starts over
// (thermostat settings are left alone). Needs THERMOSTAT_WIFI. Flash once, then comment out again.
// #define THERMOSTAT_WIFI_RESET 1

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"

#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_gc9a01.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lvgl_port.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "neopixel.h"
#include "bme280.hpp"

#if THERMOSTAT_WIFI
#include "wifi_provisioner.hpp"
#endif

#include "ui.h"
#include "screens.h"
#include "vars.h"

typedef enum {
    STATE_OFF,
    STATE_ON,
} state_t;

typedef enum {
    MODE_OFF,
    MODE_MANUAL,
    MODE_SCHEDULE,
} thermostat_mode_t;

void request_new_mode(thermostat_mode_t new_mode);

#define SETPOINT_MIN       50
#define SETPOINT_MAX       90
#define LOCK_IN_DELAY_MS   2500

static state_t current_state;
static thermostat_mode_t requested_mode; 
static thermostat_mode_t current_mode; 
static int32_t temperature_hysteresis;
static uint32_t setpoint_changed_time;
static uint32_t mode_changed_time;
// This is the setpoint that the thermostat will control the heater from
// It will be set by a monitor task when the requested_setpoint has stopped changing for LOCK_IN_DELAY_MS
static int32_t setpoint;
static int32_t requested_setpoint;
static int32_t current_temp;

int32_t get_var_setpoint() {
    return requested_setpoint;
}

void set_var_setpoint(int32_t value) {
    if (value >= SETPOINT_MIN && value <= SETPOINT_MAX) {
        requested_setpoint = value;
        setpoint_changed_time = esp_timer_get_time()/1000;
    }
}

int32_t get_var_current_temp() {
    return current_temp;
}

void set_var_current_temp(int32_t value) {
    current_temp = value;
}

static int32_t mode_value;

int32_t get_var_mode() {
    return mode_value;
}

void set_var_mode(int32_t value) {
    mode_value = value;
}

static int32_t humidity;
char pressure_string[16] = { 0 };

int32_t get_var_humidity() {
    return humidity;
}

void set_var_humidity(int32_t value) {
    humidity = value;
}

const char *get_var_pressure_string() {
    return pressure_string;
}

void set_var_pressure_string(const char *value) {
    strncpy(pressure_string, value, sizeof(pressure_string) / sizeof(char));
    pressure_string[sizeof(pressure_string) / sizeof(char) - 1] = 0;
}


static const char *TAG = "Thermostat";

#define PIN_BME_SDA     GPIO_NUM_38
#define PIN_BME_SCL     GPIO_NUM_39

static BME280 bme280(I2C_NUM_1, PIN_BME_SDA, PIN_BME_SCL);

#define PIN_POWER_LED   GPIO_NUM_40

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

#define PIXEL_COUNT     5
#define NEOPIXEL_PIN    GPIO_NUM_48

#define ENCODER_A_PIN   GPIO_NUM_45
#define ENCODER_B_PIN   GPIO_NUM_42
#define SWITCH_PIN      GPIO_NUM_41

#define TOUCH_I2C_NUM          I2C_NUM_0
#define PIN_NUM_SDA            GPIO_NUM_6
#define PIN_NUM_SCL            GPIO_NUM_7
#define PIN_NUM_TOUCH_RST      GPIO_NUM_13
#define PIN_NUM_TOUCH_INT      GPIO_NUM_5

static int32_t inactivity_timeout_ms;

static bool is_idle_mode = false;
static uint32_t default_screen_brightness;
static uint32_t current_screen_brightness;
static uint32_t screen_brightness_before_idle;

// Mutex to protect LVGL and EEZ Studio core actions
SemaphoreHandle_t lvgl_mutex;

esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;

esp_lcd_touch_handle_t tp_handle = NULL;

nvs_handle_t storage_handle;

/* ------------------------------------------------------------------------
 * Input events
 * ------------------------------------------------------------------------ */
typedef enum {
    INPUT_ENC_CW,
    INPUT_ENC_CCW,
    INPUT_BTN_PRESS,
} input_event_t;

static QueueHandle_t s_input_queue;

volatile unsigned long lastPressTime = 0;
volatile int clickCount = 0;
const unsigned long debounceTime = 20;       // ms
const unsigned long doubleClickTime = 300;   // ms

/* Quadrature transition table. Index = (previous AB << 2) | current AB. */
static const int8_t s_qtable[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

static volatile uint8_t s_enc_last = 0;
static volatile int8_t  s_enc_accum = 0;

long millis(void) {
    return esp_timer_get_time() / 1000;
}

static void IRAM_ATTR enc_isr_handler(void *arg)
{
    (void)arg;
    uint8_t now = (gpio_get_level(ENCODER_A_PIN) << 1) | gpio_get_level(ENCODER_B_PIN);
    int8_t delta = s_qtable[(s_enc_last << 2) | now];
    s_enc_last = now;
    if (delta == 0) {
        return;
    }
    s_enc_accum = s_enc_accum + delta;
    if (s_enc_accum >= 4 || s_enc_accum <= -4) {
        input_event_t ev = (s_enc_accum > 0) ? INPUT_ENC_CW : INPUT_ENC_CCW;
        s_enc_accum = 0;
        BaseType_t hp_woken = pdFALSE;
        xQueueSendFromISR(s_input_queue, &ev, &hp_woken);
        if (hp_woken) {
            portYIELD_FROM_ISR();
        }
    }
}

void IRAM_ATTR buttonISR(void *arg) {
    (void)arg;
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
    if (interruptTime - lastInterruptTime > debounceTime) {
        if (!gpio_get_level(SWITCH_PIN)) {     // active low
            lastPressTime = interruptTime;
            clickCount = clickCount + 1;
            input_event_t ev = INPUT_BTN_PRESS;
            BaseType_t hp_woken = pdFALSE;
            xQueueSendFromISR(s_input_queue, &ev, &hp_woken);
            if (hp_woken) {
                portYIELD_FROM_ISR();
            }
        }
    }
    lastInterruptTime = interruptTime;
}

static void nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // Open NVS handle
    ESP_LOGI(TAG, "\nOpening Non-Volatile Storage (NVS) handle...");
    err = nvs_open("storage", NVS_READWRITE, &storage_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return;
    }
}

static void store_int32(const char* key, int32_t value)
{
    esp_err_t err = nvs_set_i32(storage_handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to storage!");
    }
    err = nvs_commit(storage_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS changes!");
    }
}

static int32_t read_int32_with_default(const char* key, int32_t default_value)
{
    int32_t value;
    ESP_LOGI(TAG, "\nReading counter from NVS...");
    esp_err_t err = nvs_get_i32(storage_handle, key, &value);
    if (err != ESP_OK) {
        value = default_value;
    }
    return value;
}

static void encoder_init(void)
{
    gpio_config_t io_enc = {};
    io_enc.pin_bit_mask = (1ULL << ENCODER_A_PIN) | (1ULL << ENCODER_B_PIN);
    io_enc.mode = GPIO_MODE_INPUT;
    io_enc.pull_up_en = GPIO_PULLUP_ENABLE;
    io_enc.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_enc.intr_type = GPIO_INTR_ANYEDGE;
    ESP_ERROR_CHECK(gpio_config(&io_enc));

    s_enc_last = (gpio_get_level(ENCODER_A_PIN) << 1) | gpio_get_level(ENCODER_B_PIN);

    gpio_install_isr_service(0);
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENCODER_A_PIN, enc_isr_handler, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENCODER_B_PIN, enc_isr_handler, NULL));
}

static void switch_init(void)
{
    // Configure pin as input with pull-up, interrupt on any edge (CHANGE)
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << SWITCH_PIN,
    io_conf.mode         = GPIO_MODE_INPUT,
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE,
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE,
    io_conf.intr_type    = GPIO_INTR_ANYEDGE,
    gpio_config(&io_conf);

    // Install the shared GPIO ISR service (safe to ignore ESP_ERR_INVALID_STATE
    // if another component already installed it)
    gpio_install_isr_service(0);

    // Attach the handler to the pin
    gpio_isr_handler_add(SWITCH_PIN, buttonISR, NULL);
}

static void touch_init(void) {
    // ----------------------------------------------------
    // Initialize CST816S Touch via I2C
    // ----------------------------------------------------
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
    esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &tp_handle);
}

/* ------------------------------------------------------------------------
 * Backlight (LEDC PWM)
 * ------------------------------------------------------------------------ */
static void backlight_init(void)
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

static void backlight_set(uint32_t percent, uint32_t fade_ms)
{
    if (percent > 100) {
        percent = 100;
    }

    current_screen_brightness = percent;
    uint32_t duty = (255 * (uint32_t)percent) / 100;
    if (fade_ms > 0) {
        ledc_set_fade_with_time(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty, fade_ms);
        ledc_fade_start(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, LEDC_FADE_NO_WAIT);
    } else {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, BL_LEDC_CHANNEL);
    }
}

static void power_led_init(void)
{
    gpio_config_t io_led = {};
    io_led.pin_bit_mask = 1ULL << PIN_POWER_LED;
    io_led.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&io_led));
    gpio_set_level(PIN_POWER_LED, 0);
}

static void power_init(void) {
    // Turn on both rails
    gpio_config_t io_pin_1 = {};
    io_pin_1.pin_bit_mask = 1ULL << GPIO_NUM_1;
    io_pin_1.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&io_pin_1));
    gpio_set_level(GPIO_NUM_1, 1);
    gpio_config_t io_pin_2 = {};
    io_pin_2.pin_bit_mask = 1ULL << GPIO_NUM_2;
    io_pin_2.mode = GPIO_MODE_OUTPUT;
    ESP_ERROR_CHECK(gpio_config(&io_pin_2));
    gpio_set_level(GPIO_NUM_2, 1);
}

#if !defined(MAX)
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif
#define ARRAY_SIZE(x) (sizeof(x)/sizeof(x[0]))
static tNeopixelContext neopixel;

static bool neopixel_init(void)
{
    neopixel = neopixel_Initialize(PIXEL_COUNT, NEOPIXEL_PIN, NEOPIXEL_MODE_WS2812B);
    if(NULL == neopixel)
    {
        ESP_LOGE(TAG, "[%s] Initialization failed\n", __func__);
        return false;
    }
    // Clear the pixels
    tNeopixel pixel[] = {{0, 0x000000},
                        {1, 0x000000},
                        {2, 0x000000},
                        {3, 0x000000},
                        {4, 0x000000}};
    neopixel_SetPixel(neopixel, pixel, 5);

   return true;
}

static void toggle_mode()
{
    // Step from the requested mode, not the applied one, so every tap advances the UI
    // even while an earlier request is still waiting to lock in
    switch (requested_mode)
    {
    case MODE_OFF:
        request_new_mode(MODE_MANUAL);
        break;
    
    case MODE_MANUAL:
        request_new_mode(MODE_SCHEDULE);
        break;
    
    case MODE_SCHEDULE:
        request_new_mode(MODE_OFF);
        break;
    
    default:
        break;
    }
}

static void mode_button_event_handler(lv_event_t * event) 
{
    lv_event_code_t code = lv_event_get_code(event);
    
    // Check if the button was clicked (short press and release)
    if(code == LV_EVENT_CLICKED) {
        toggle_mode();
    }
}

static void eez_ui_tick_cb(lv_timer_t * timer) {
    if (lvgl_port_lock(0)) {
        ui_tick(); // Call EEZ Studio's tick function
        lvgl_port_unlock();
    }
}

static void lvgl_initialize(void) {
    // ----------------------------------------------------
    // Initialize LVGL & Port Layer
    // ----------------------------------------------------
    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    lvgl_port_init(&lvgl_cfg);

    // Add display to LVGL
    lvgl_port_display_cfg_t disp_cfg = {};
    disp_cfg.io_handle = io_handle;
    disp_cfg.panel_handle = panel_handle;
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
    touch_cfg.handle = tp_handle;
    // lv_indev_t *lvgl_touch = lvgl_port_add_touch(&touch_cfg);
    lvgl_port_add_touch(&touch_cfg);

    // Init the EEZ ui and setup the timer callback
    if (lvgl_port_lock(0)) {
        ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));

        // Initialize EEZ Studio UI
        ui_init(); 

        // Setup the heat on indicator ring
        lv_obj_set_style_outline_opa(objects.main_container, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_style_radius(objects.main_container, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_outline_width(objects.main_container, 4, LV_PART_MAIN);
        // Push the outline 3px INWARD so it stays inside your 240x240 screen boundary
        lv_obj_set_style_outline_pad(objects.main_container, -4, LV_PART_MAIN);
        lv_obj_set_style_outline_color(objects.main_container, lv_color_hex(0xFF4D4D), LV_PART_MAIN);

        lv_obj_add_event_cb(objects.mode_button, mode_button_event_handler, LV_EVENT_CLICKED, NULL);

        // Register EEZ Studio's tick to execute natively every 10ms 
        // inside the official esp_lvgl_port task thread
        lv_timer_create(eez_ui_tick_cb, 10, NULL); 

        // Release the lock so the port can begin rendering
        lvgl_port_unlock();
    }    
}

static void display_init(void) {
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
                                             &io_cfg, &io_handle));

    // GC9A01 panel driver
    esp_lcd_panel_dev_config_t panel_cfg = {};
    panel_cfg.reset_gpio_num = PIN_RST;
    panel_cfg.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR;
    panel_cfg.bits_per_pixel = 16;
    ESP_ERROR_CHECK(esp_lcd_new_panel_gc9a01(io_handle, &panel_cfg, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));  // GC9A01 usually needs this
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_LOGI(TAG, "Panel initialized");
}

/* Fill the screen in chunks of rows to keep RAM use low. */
void display_fill(uint16_t color)
{
    if (panel_handle == NULL) {
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
        ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel_handle, 0, y, LCD_H_RES, y + CHUNK_LINES, buf));
    }

    heap_caps_free(buf);
}

static void deactivate_idle_mode()
{
    if (is_idle_mode) {
        is_idle_mode = false;
        
        lv_display_trigger_activity(NULL); 

        // Bring back the backlight smoothly (e.g., over 300ms)
        backlight_set(screen_brightness_before_idle, 300);
        
        // Load the main operational screen back
        if (lvgl_port_lock(0)) {
            eez_flow_set_screen(SCREEN_ID_MAIN, LV_SCR_LOAD_ANIM_FADE_IN, 200, 0);
            lvgl_port_unlock();
        }
    }
}

static void activate_idle_mode()
{
    if (!is_idle_mode) {
        is_idle_mode = true;
        
        // Load the empty/minimalistic low-power screen
        if (lvgl_port_lock(0)) {
            eez_flow_set_screen(SCREEN_ID_IDLE, LV_SCR_LOAD_ANIM_FADE_IN, 500, 0);
            lvgl_port_unlock();
        }
        
        // Fade out backlight completely over 500ms (or dim down to 5-10% duty)
        screen_brightness_before_idle = current_screen_brightness;
        backlight_set(3, 500); 
    }
}

static void input_task(void *arg)
{
    (void)arg;
    input_event_t ev;
    // int brightness = 80;

    // backlight_set(brightness, 0);

    while (true) {
        if (xQueueReceive(s_input_queue, &ev, portMAX_DELAY) == pdTRUE) {
            // First event while in idle mode just deactivates idle mode
            if (is_idle_mode) {
                deactivate_idle_mode();
                continue;
            }
            // Even though these events are not touch screen activity they are activity that should reset the idle timer
            lv_display_trigger_activity(NULL); 

            switch (ev) {
            case INPUT_ENC_CW:
                // Increase the setpoint
                if (lvgl_port_lock(0)) {
                    set_var_setpoint(get_var_setpoint()+1);
                    lvgl_port_unlock();
                }
                ESP_LOGI(TAG, "Encoder CW, setpoint %d%%", requested_setpoint);
                break;

            case INPUT_ENC_CCW:
                // Decrease the setpoint
                if (lvgl_port_lock(0)) {
                    set_var_setpoint(get_var_setpoint()-1);
                    lvgl_port_unlock();
                }
                ESP_LOGI(TAG, "Encoder CCW, setpoint %d%%", requested_setpoint);
                break;

            // case INPUT_ENC_CW:
            //     brightness = (brightness >= 100) ? 100 : brightness + 5;
            //     backlight_set(brightness, 0);
            //     ESP_LOGI(TAG, "Encoder CW, brightness %d%%", brightness);
            //     break;

            // case INPUT_ENC_CCW:
            //     brightness = (brightness <= 0) ? 0 : brightness - 5;
            //     backlight_set(brightness, 0);
            //     ESP_LOGI(TAG, "Encoder CCW, brightness %d%%", brightness);
            //     break;

            case INPUT_BTN_PRESS:
                // vTaskDelay(pdMS_TO_TICKS(30));   /* simple debounce */
                // if (gpio_get_level(PIN_ENC_BTN) == 0) {
                    ESP_LOGI(TAG, "Button pressed");
                // }
                break;
            }
        }
    }
}

// The reason there is a request_new_mode and a set_new_mode is to keep the heater from changing modes while the user is toggling through the modes in the ui
void request_new_mode(thermostat_mode_t new_mode)
{
    mode_changed_time = esp_timer_get_time()/1000;

    switch(new_mode) {
        case MODE_MANUAL:
            requested_mode = MODE_MANUAL;
            set_var_mode(MODE_MANUAL);
            break;

        case MODE_SCHEDULE:
            requested_mode = MODE_SCHEDULE;
            set_var_mode(MODE_SCHEDULE);
            break;

        case MODE_OFF:
            requested_mode = MODE_OFF;
            set_var_mode(MODE_OFF);
            break;
    }
}

void set_new_mode(thermostat_mode_t new_mode)
{
    switch(new_mode) {
        case MODE_MANUAL:
            current_mode = MODE_MANUAL;
            break;

        case MODE_SCHEDULE:
            current_mode = MODE_SCHEDULE;
            break;

        case MODE_OFF:
            current_mode = MODE_OFF;
            break;
    }
    ESP_LOGI(TAG, "Mode applied: %d", (int)current_mode);
    store_int32("mode", current_mode);
}

// Custom animation callback to apply opacity to the outline
void outline_opa_anim_cb(lv_anim_t* animation_object, int32_t value)
{
    lv_obj_t * obj = (lv_obj_t *)animation_object->var;
    lv_obj_set_style_outline_opa(obj, value, LV_PART_MAIN);
}

// Function to call whenever your app state changes
void toggle_ring_fade(bool enable)
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

void monitor_state(void *arg) 
{
    (void)arg;
    while(true) {
        if (current_state == STATE_ON) {
            // See if we should turn off
            if (((setpoint + temperature_hysteresis) <= current_temp) || (current_mode == MODE_OFF)) {
                current_state = STATE_OFF;
                ESP_LOGI(TAG, "State -> OFF (temp %d, setpoint %d, hysteresis %d, mode %d)",
                         (int)current_temp, (int)setpoint, (int)temperature_hysteresis, (int)current_mode);
                if (lvgl_port_lock(0)) {
                    toggle_ring_fade(false);
                    lvgl_port_unlock();
                }
            }
        }
        else { // Must be in STATE_OFF
            // See if we should turn on
            if (((current_temp + temperature_hysteresis) <= setpoint) && (current_mode != MODE_OFF)) {
                current_state = STATE_ON;
                ESP_LOGI(TAG, "State -> ON (temp %d, setpoint %d, hysteresis %d, mode %d)",
                         (int)current_temp, (int)setpoint, (int)temperature_hysteresis, (int)current_mode);
                if (lvgl_port_lock(0)) {
                    toggle_ring_fade(true);
                    lvgl_port_unlock();
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(400));
    }
}

void monitor_mode_change(void *arg) 
{
    (void)arg;
    while(true) {
        if (((esp_timer_get_time()/1000) - mode_changed_time) > LOCK_IN_DELAY_MS) {
            thermostat_mode_t target = requested_mode;
            if (current_mode != target) {
                set_new_mode(target);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void monitor_setpoint_change(void *arg) 
{
    (void)arg;
    while(true) {
        if (((esp_timer_get_time()/1000) - setpoint_changed_time) > LOCK_IN_DELAY_MS) {
            if (setpoint != requested_setpoint) {
                setpoint = requested_setpoint;
                store_int32("setpoint", setpoint);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void monitor_idle_timeout(void *arg) 
{
    (void)arg;
    while (true) {
        uint32_t inactive_time = lv_display_get_inactive_time(NULL); 

        if (inactive_time >= (uint32_t)inactivity_timeout_ms) {
            activate_idle_mode();
        }
        else {
            deactivate_idle_mode();
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void monitor_sensors(void *arg)
{
    BME280 *bme = static_cast<BME280 *>(arg);
    BME280::Data data;
    while (true) {
        if (bme->read(data) == ESP_OK) {
            set_var_current_temp(data.temperature_f);
            set_var_humidity((int32_t)data.humidity_pct);
            char pressure_text[16];
            snprintf(pressure_text, sizeof(pressure_text), "%.2f", data.pressure_inhg);
            set_var_pressure_string(pressure_text);
            ESP_LOGI(TAG, "BME280: %d°F  %lu%%RH  %.2f inHg",
                     (int)data.temperature_f,
                     (unsigned long)data.humidity_pct,
                     data.pressure_inhg);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

#if THERMOSTAT_WIFI
#define WIFI_AP_SSID     "Thermostat-Setup"
#define WIFI_HOSTNAME    "thermostat"

// start_provisioning() blocks until the user submits credentials through the
// captive portal, so this runs in its own task and the thermostat keeps working.
static void wifi_task(void *arg)
{
    (void)arg;
    static WifiProvisioner provisioner;

    // This section is only used for testing if you want to get rid of the provisioned WiFi
#if THERMOSTAT_WIFI_RESET
    {
        // Same namespace the provisioner stores its credentials in
        nvs_handle_t h;
        if (nvs_open("wifi_prov", NVS_READWRITE, &h) == ESP_OK) {
            nvs_erase_all(h);
            nvs_commit(h);
            nvs_close(h);
            ESP_LOGW(TAG, "THERMOSTAT_WIFI_RESET: saved WiFi credentials erased");
        }
    }
#endif

    if (provisioner.is_provisioned()) {
        provisioner.get_credentials();
    } else {
        ESP_LOGI(TAG, "No WiFi credentials, starting captive portal \"%s\"", WIFI_AP_SSID);
        provisioner.start_provisioning(WIFI_AP_SSID, true);
    }
    provisioner.connect_sta();

    vTaskDelete(NULL);
}
#endif

static void set_startup_values(void)
{
    current_state = STATE_OFF;
    requested_mode = (thermostat_mode_t)read_int32_with_default("mode", (int32_t)MODE_MANUAL);
    set_new_mode(requested_mode);
    set_var_mode(requested_mode);
    temperature_hysteresis = read_int32_with_default("temp_hyst", 2);
    requested_setpoint = read_int32_with_default("setpoint", 68);
    inactivity_timeout_ms = read_int32_with_default("timeout_ms", 30000);
    default_screen_brightness = read_int32_with_default("def_bright", 100);
    current_temp = 87; /* replaced by BME280 task once it starts */
}

extern "C" void app_main(void)
{
    // Create the mutex before launching the task
    lvgl_mutex = xSemaphoreCreateMutex();

    nvs_init();
    set_startup_values();

    ESP_LOGI(TAG, "Switch & Encoder Init");
    switch_init();
    encoder_init();
    s_input_queue = xQueueCreate(16, sizeof(input_event_t));

    ESP_LOGI(TAG, "Power Init");
    power_init();
    power_led_init();
    neopixel_init();

    ESP_LOGI(TAG, "Backlight Init");
    backlight_init();
    backlight_set(default_screen_brightness, 0);

    ESP_LOGI(TAG, "Display Init");
    display_init();

    ESP_LOGI(TAG, "Touch Init");
    touch_init();

    ESP_LOGI(TAG, "BME280 Init");
    ESP_ERROR_CHECK_WITHOUT_ABORT(bme280.init());

    lvgl_initialize();
    lv_display_trigger_activity(NULL); 

    xTaskCreatePinnedToCore(
        input_task, 
        "input_task", 
        4096, 
        NULL, 
        5, 
        NULL,
        1
    );

    xTaskCreate(
        monitor_idle_timeout, 
        "idle_timeout", 
        4096, 
        NULL, 
        2, 
        NULL
    );

    xTaskCreate(
        monitor_setpoint_change, 
        "setpoint_change", 
        4096, 
        NULL, 
        2, 
        NULL
    );

    xTaskCreate(
        monitor_mode_change, 
        "mode_change", 
        4096, 
        NULL, 
        2, 
        NULL
    );

    xTaskCreate(
        monitor_state,
        "monitor_state",
        4096,
        NULL,
        2,
        NULL
    );

    xTaskCreate(
        monitor_sensors,
        "sensors",
        4096,
        &bme280,
        2,
        NULL
    );

#if THERMOSTAT_WIFI
    xTaskCreate(
        wifi_task,
        "wifi",
        8192,
        NULL,
        3,
        NULL
    );
#endif
}
