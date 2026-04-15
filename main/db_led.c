#include "db_led.h"

#include <stdio.h>
#include <driver/gpio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "driver/ledc.h"
#include <math.h>

#define TAG "DB_LED"

#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_GPIO           38

// 13bit: 0..8191
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT
#define LEDC_DUTY_MAX       ((1 << 13) - 1)
#define LEDC_FREQ_HZ        5000

static void pwm_init_gpio(void)
{
    ledc_timer_config_t timer = {
        .speed_mode       = LEDC_MODE,
        .duty_resolution  = LEDC_DUTY_RES,
        .timer_num        = LEDC_TIMER,
        .freq_hz          = LEDC_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    ledc_channel_config_t ch = {
        .gpio_num       = LEDC_GPIO,
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER,
        .duty           = 0,
        .hpoint         = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch));
}

static inline void led_set(uint32_t duty)
{
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

void process_led(void *arg) {

    while (1) {

        led_set(0);
        vTaskDelay(pdMS_TO_TICKS(1000));

        led_set(LEDC_DUTY_MAX*1/1);
        vTaskDelay(pdMS_TO_TICKS(1000));

    }
}

void db_start_led_module() {
    pwm_init_gpio();
    xTaskCreate(&process_led, "db_led", 4096, NULL, 1, NULL);
}
