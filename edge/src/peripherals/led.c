#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

#include "driver/ledc.h"
#include "driver/gpio.h"

#include "led.h"
#include "config/edge_config.h"

#define CH_RED      LEDC_CHANNEL_0
#define CH_GREEN    LEDC_CHANNEL_1
#define CH_BLUE     LEDC_CHANNEL_2

#define LEDC_TIMER   LEDC_TIMER_0    
#define LEDC_MODE    LEDC_LOW_SPEED_MODE
#define LEDC_FREQ_HZ 5000
#define LEDC_RES     LEDC_TIMER_10_BIT

static uint32_t duty_max;
static uint32_t duty_current;
static rgb_color_t current_color;
static rgb_color_t led_color = RGB_OFF;
static led_mode_t led_mode = LED_MODE_OFF;
static portMUX_TYPE led_mux = portMUX_INITIALIZER_UNLOCKED;


static void channel_init(int gpio, ledc_channel_t ch)
{
    ledc_channel_config_t cfg = {
        .channel    = ch,
        .duty       = 0,
        .gpio_num   = gpio,
        .speed_mode = LEDC_MODE,
        .hpoint     = 0,
        .timer_sel  = LEDC_TIMER
    };
    ledc_channel_config(&cfg);
}
static void rgb_led_set(rgb_color_t color)
{
    current_color = color;

    // Start with everything OFF //
    ledc_set_duty(LEDC_MODE, CH_RED, 0);
    ledc_set_duty(LEDC_MODE, CH_GREEN, 0);
    ledc_set_duty(LEDC_MODE, CH_BLUE, 0);

    switch (color) 
    {
        case RGB_RED:
            ledc_set_duty(LEDC_MODE, CH_RED, duty_current);
            break;
        case RGB_GREEN:
            ledc_set_duty(LEDC_MODE, CH_GREEN, duty_current);
            break;
        case RGB_BLUE:
            ledc_set_duty(LEDC_MODE, CH_BLUE, duty_current);
            break;
        case RGB_CYAN:
            ledc_set_duty(LEDC_MODE, CH_GREEN, duty_current);
            ledc_set_duty(LEDC_MODE, CH_BLUE, duty_current);
            break;
        case RGB_YELLOW:
            ledc_set_duty(LEDC_MODE, CH_GREEN, duty_current);
            ledc_set_duty(LEDC_MODE, CH_RED, duty_current);
            break;
        case RGB_MAGENTA:
            ledc_set_duty(LEDC_MODE, CH_RED, duty_current);
            ledc_set_duty(LEDC_MODE, CH_BLUE, duty_current);
            break;
        case RGB_WHITE:
            ledc_set_duty(LEDC_MODE, CH_RED, duty_current);
            ledc_set_duty(LEDC_MODE, CH_BLUE, duty_current);
            ledc_set_duty(LEDC_MODE, CH_GREEN, duty_current);
            break;
        default:
            break;
    }
    ledc_update_duty(LEDC_MODE, CH_RED);
    ledc_update_duty(LEDC_MODE, CH_GREEN);
    ledc_update_duty(LEDC_MODE, CH_BLUE);
}

static void led_task(void *arg)
{
    while(1)
    {
        switch (led_mode)
        {
            case LED_MODE_SOLID:
                rgb_led_set(led_color);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            
            case LED_MODE_BLINK:
                rgb_led_set(led_color);
                vTaskDelay(pdMS_TO_TICKS(200));
                rgb_led_set(RGB_OFF);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
            
            case LED_MODE_PULSE:
                rgb_led_set(led_color);
                vTaskDelay(pdMS_TO_TICKS(150));
                rgb_led_set(RGB_OFF);
                led_mode = LED_MODE_OFF;
                break;

            case LED_MODE_OFF:
                rgb_led_set(RGB_OFF);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
            default:
                rgb_led_set(RGB_OFF);
                vTaskDelay(pdMS_TO_TICKS(100));
        }   
    }
}

void led_init(void)
{
    ledc_timer_config_t timer = {
        .speed_mode      = LEDC_MODE,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = LEDC_FREQ_HZ,
        .duty_resolution = LEDC_RES,
        .clk_cfg         = LEDC_AUTO_CLK
    };
    ledc_timer_config(&timer);

    duty_max = (1 << LEDC_RES) - 1;
    duty_current = duty_max / 8;
    current_color = RGB_OFF;

    channel_init(LED_RED,   CH_RED);
    channel_init(LED_BLUE,  CH_BLUE);
    channel_init(LED_GREEN, CH_GREEN);

    xTaskCreate(led_task,
                "led_task",
                2048,
                NULL,
                3,
                NULL);
}

void led_set(rgb_color_t color, led_mode_t mode)
{
    taskENTER_CRITICAL(&led_mux);
    led_color = color;
    led_mode  = mode;
    taskEXIT_CRITICAL(&led_mux);
}

void led_off(void) 
{
    led_set(RGB_OFF, LED_MODE_OFF);
}