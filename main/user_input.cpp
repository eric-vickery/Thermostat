#include "user_input.hpp"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_timer.h"
#include "freertos/queue.h"

#define ENCODER_A_PIN   GPIO_NUM_45
#define ENCODER_B_PIN   GPIO_NUM_42
#define SWITCH_PIN      GPIO_NUM_41

static QueueHandle_t s_input_queue;

static volatile unsigned long lastPressTime = 0;
static volatile int clickCount = 0;
static const unsigned long debounceTime = 20;       // ms

/* Quadrature transition table. Index = (previous AB << 2) | current AB. */
static const int8_t s_qtable[16] = {
     0, -1,  1,  0,
     1,  0,  0, -1,
    -1,  0,  0,  1,
     0,  1, -1,  0,
};

static volatile uint8_t s_enc_last = 0;
static volatile int8_t  s_enc_accum = 0;

static long millis(void) {
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
        UserInput::Event ev = (s_enc_accum > 0) ? UserInput::ENC_CW : UserInput::ENC_CCW;
        s_enc_accum = 0;
        BaseType_t hp_woken = pdFALSE;
        xQueueSendFromISR(s_input_queue, &ev, &hp_woken);
        if (hp_woken) {
            portYIELD_FROM_ISR();
        }
    }
}

static void IRAM_ATTR buttonISR(void *arg) {
    (void)arg;
    static unsigned long lastInterruptTime = 0;
    unsigned long interruptTime = millis();
    if (interruptTime - lastInterruptTime > debounceTime) {
        if (!gpio_get_level(SWITCH_PIN)) {     // active low
            lastPressTime = interruptTime;
            clickCount = clickCount + 1;
            UserInput::Event ev = UserInput::BTN_PRESS;
            BaseType_t hp_woken = pdFALSE;
            xQueueSendFromISR(s_input_queue, &ev, &hp_woken);
            if (hp_woken) {
                portYIELD_FROM_ISR();
            }
        }
    }
    lastInterruptTime = interruptTime;
}

void UserInput::init()
{
    // Create the queue before enabling the interrupts that post to it
    s_input_queue = xQueueCreate(16, sizeof(Event));

    // Encoder
    gpio_config_t io_enc = {};
    io_enc.pin_bit_mask = (1ULL << ENCODER_A_PIN) | (1ULL << ENCODER_B_PIN);
    io_enc.mode = GPIO_MODE_INPUT;
    io_enc.pull_up_en = GPIO_PULLUP_ENABLE;
    io_enc.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_enc.intr_type = GPIO_INTR_ANYEDGE;
    ESP_ERROR_CHECK(gpio_config(&io_enc));

    s_enc_last = (gpio_get_level(ENCODER_A_PIN) << 1) | gpio_get_level(ENCODER_B_PIN);

    // Install the shared GPIO ISR service (safe to ignore ESP_ERR_INVALID_STATE
    // if another component already installed it)
    gpio_install_isr_service(0);
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENCODER_A_PIN, enc_isr_handler, NULL));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENCODER_B_PIN, enc_isr_handler, NULL));

    // Switch: input with pull-up, interrupt on any edge (CHANGE)
    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = 1ULL << SWITCH_PIN;
    io_conf.mode         = GPIO_MODE_INPUT;
    io_conf.pull_up_en   = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type    = GPIO_INTR_ANYEDGE;
    gpio_config(&io_conf);
    gpio_isr_handler_add(SWITCH_PIN, buttonISR, NULL);
}

bool UserInput::wait(Event &ev, TickType_t ticks)
{
    return xQueueReceive(s_input_queue, &ev, ticks) == pdTRUE;
}
