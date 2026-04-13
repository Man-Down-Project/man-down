#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_task_wdt.h"

#include "led_strip.h"  // New dependency
#include "driver/gpio.h"

#include "onboard_led.h"

// The DevKit onboard LED is on GPIO 8
#define LED_STRIP_GPIO 8
#define LED_STRIP_NUM  1

static const char *TAG = "LED_DRIVER";

static led_strip_handle_t led_strip;
static rgb_color_t led_color = RGB_OFF;
static led_mode_t led_mode = LED_MODE_OFF;
static portMUX_TYPE led_mux = portMUX_INITIALIZER_UNLOCKED;
static led_priority_t current_priority = LED_PRIO_LOW;

// Brightness control (0-255). 
// WS2812 can be blindingly bright; 32 is a good "medium".
static const uint8_t BRIGHTNESS = 32;

/**
 * @brief Map your enum to actual RGB values and send to the strip
 */
static void rgb_led_set(rgb_color_t color)
{
    uint8_t r = 0, g = 0, b = 0;

    switch (color) 
    {
        case RGB_RED:     r = BRIGHTNESS; break;
        case RGB_GREEN:   g = BRIGHTNESS; break;
        case RGB_BLUE:    b = BRIGHTNESS; break;
        case RGB_CYAN:    g = BRIGHTNESS; b = BRIGHTNESS; break;
        case RGB_YELLOW:  r = BRIGHTNESS; g = BRIGHTNESS; break;
        case RGB_MAGENTA: r = BRIGHTNESS; b = BRIGHTNESS; break;
        case RGB_WHITE:   r = BRIGHTNESS; g = BRIGHTNESS; b = BRIGHTNESS; break;
        case RGB_DARK_PURPLE: r = 20;g = 0;b = 40;break;
        case RGB_OFF:
        default:          r = 0; g = 0; b = 0; break;
    }

    if (led_strip) {
        led_strip_set_pixel(led_strip, 0, r, g, b);
        led_strip_refresh(led_strip);
    }
}

static void onboard_led_task(void *arg)
{
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    while(1)
    {
        // Snapshot the current mode/color to avoid holding the mux too long
        taskENTER_CRITICAL(&led_mux);
        led_mode_t mode = led_mode;
        rgb_color_t color = led_color;
        taskEXIT_CRITICAL(&led_mux);

        switch (mode)
        {
            case LED_MODE_SOLID:
                rgb_led_set(color);
                vTaskDelay(pdMS_TO_TICKS(100));
                break;
            
            case LED_MODE_BLINK:
                rgb_led_set(color);
                vTaskDelay(pdMS_TO_TICKS(200));
                rgb_led_set(RGB_OFF);
                vTaskDelay(pdMS_TO_TICKS(200));
                break;
            
            case LED_MODE_PULSE:
                rgb_led_set(color);
                vTaskDelay(pdMS_TO_TICKS(150));
                rgb_led_set(RGB_OFF);
                // Reset mode to off after one pulse
                taskENTER_CRITICAL(&led_mux);
                led_mode = LED_MODE_OFF;
                taskEXIT_CRITICAL(&led_mux);
                break;

            case LED_MODE_OFF:
            default:
                rgb_led_set(RGB_OFF);
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
        }
        esp_task_wdt_reset();   
    }
}

void onboard_led_init(void)
{
    /* LED strip initialization with the RMT peripheral */
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = LED_STRIP_NUM,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Standard for WS2812
        .led_model = LED_MODEL_WS2812,
        .flags.invert_out = false,
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    
    // Clear the LED to start
    led_strip_clear(led_strip);

    ESP_LOGI(TAG, "WS2812 LED initialized on GPIO %d", LED_STRIP_GPIO);

    xTaskCreate(onboard_led_task, "onboard_led_task", 3072, NULL, 3, NULL);
}

void onboard_led_set(rgb_color_t color, led_mode_t mode, led_priority_t prio)
{
    taskENTER_CRITICAL(&led_mux);
    if(prio >= current_priority)
    {
        led_color = color;
        led_mode  = mode;
        current_priority = prio;
    }
    taskEXIT_CRITICAL(&led_mux);
}

void onboard_led_release(led_priority_t prio)
{
    taskENTER_CRITICAL(&led_mux);
    if (prio == current_priority)
    {
        current_priority = LED_PRIO_LOW;
        led_mode = LED_MODE_OFF;
        led_color = RGB_OFF;
    }
    taskEXIT_CRITICAL(&led_mux);
}

void onboard_led_off(void) 
{
    onboard_led_set(RGB_OFF, LED_MODE_OFF, LED_PRIO_HIGH);
}