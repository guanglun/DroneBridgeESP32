#include "db_led.h"

#include <stdio.h>
#include <driver/gpio.h>
#include "esp_log.h"
#include "esp_event.h"
#include "driver/ledc.h"
#include <math.h>

#include "db_esp32_control.h"
extern udp_conn_list_t *udp_conn_list;
extern int8_t num_connected_tcp_clients;
extern uint32_t serial_total_byte_count;
extern uint32_t tcp_total_byte_count;
extern uint32_t udp_total_byte_count;

#define TAG "DB_LED"

#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_0
#define LEDC_GPIO           38

// 13bit: 0..8191
#define LEDC_DUTY_RES       LEDC_TIMER_13_BIT
#define LEDC_DUTY_MAX       ((1 << 13) - 1)
#define LEDC_FREQ_HZ        5000

typedef enum {
    LED_MODE_OFF,
    LED_MODE_ON,
    LED_MODE_BLINK_SLOW,    // 慢闪
    LED_MODE_BLINK_FAST,    // 快闪
    LED_MODE_BREATH_SLOW,   // 慢呼吸
    LED_MODE_BREATH_FAST    // 快呼吸
} db_led_mode_t;

// 全局状态变量
static db_led_mode_t current_led_mode = LED_MODE_BREATH_SLOW;

// 外部调用接口
void db_set_led_mode(db_led_mode_t mode) {
    current_led_mode = mode;
}

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
    uint32_t count = 0;
    while (1) {
        switch (current_led_mode) {
            
            case LED_MODE_BLINK_SLOW: // 慢闪 (1秒亮, 1秒灭)
                // 这里的 count 每 20ms 加 1，所以 50 次是 1 秒
                led_set((count % 100 < 50) ? LEDC_DUTY_MAX : 0);
                vTaskDelay(pdMS_TO_TICKS(20)); 
                break;

            case LED_MODE_BLINK_FAST: // 快闪 (200ms亮, 200ms灭)
                led_set((count % 20 < 10) ? LEDC_DUTY_MAX : 0);
                vTaskDelay(pdMS_TO_TICKS(20));
                break;

            case LED_MODE_BREATH_SLOW: // 慢呼吸
            {
                float duty_factor = (sinf(count * 0.05f) + 1.0f) / 2.0f;
                led_set((uint32_t)(duty_factor * LEDC_DUTY_MAX));
                vTaskDelay(pdMS_TO_TICKS(10)); 
                break;
            }

            case LED_MODE_BREATH_FAST: // 快呼吸
            {
                float duty_factor = (sinf(count * 0.15f) + 1.0f) / 2.0f;
                led_set((uint32_t)(duty_factor * LEDC_DUTY_MAX));
                vTaskDelay(pdMS_TO_TICKS(10)); 
                break;
            }

            case LED_MODE_ON:
                led_set(LEDC_DUTY_MAX);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            default: // LED_MODE_OFF
                led_set(0);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
        }
        count++;
    }
}

void process_status(void *arg) {
    uint32_t serial_total_byte_count_last = 0;
    uint32_t tcp_total_byte_count_last = 0;
    uint32_t udp_total_byte_count_last = 0;
    while(1)
    {

        if(tcp_total_byte_count != tcp_total_byte_count_last || udp_total_byte_count != udp_total_byte_count_last)
        {
            if(serial_total_byte_count != serial_total_byte_count_last)
            {
                db_set_led_mode(LED_MODE_BLINK_SLOW);
            }else{
                db_set_led_mode(LED_MODE_BREATH_FAST);
            }
        }else if(serial_total_byte_count != serial_total_byte_count_last)
        {
            db_set_led_mode(LED_MODE_BREATH_SLOW);
        }else{
            db_set_led_mode(LED_MODE_BLINK_FAST);
        }

        serial_total_byte_count_last = serial_total_byte_count;
        tcp_total_byte_count_last = tcp_total_byte_count;
        udp_total_byte_count_last = udp_total_byte_count;
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void db_start_led_module() {
    pwm_init_gpio();

    xTaskCreate(&process_led, "db_led", 4096, NULL, 5, NULL);
    xTaskCreate(&process_status, "db_status", 1024, NULL, 5, NULL);

    db_set_led_mode(LED_MODE_BLINK_FAST);
}
