#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

#include "ble.h"

static const char *TAG = "MAIN";

#define BUTTON_GPIO 10 // Button pin
#define BUZZER_GPIO 12 // Buzzer pin
#define Accelerometer_GPIO 22 //Accelerometer pin

typedef struct { // external comunication
    uint8_t device_id;
    uint8_t event_type;
    uint8_t event_location;
    uint8_t battery_status;
}edge_event_t;

typedef struct {
    uint8_t accelerometer;
    uint8_t last_location;
    uint8_t curren_location;
    uint8_t current_node;
}internal_data;



void app_main(void)
{
    ESP_LOGI(TAG, "Starting application");

    ble_init();
}

 
/* lite test bara
#define BUTTON_GPIO 10 // Button pin
#define BUZZER_GPIO 12 // Buzzer pin

// simple passive buzzer tone generator
void play_tone(int frequency, int duration_ms) {
    int delay_us = 1000000 / (2 * frequency); // half period in microseconds
    int cycles = frequency * duration_ms / 1000;

    for (int i = 0; i < cycles; i++) {
        gpio_set_level(BUZZER_GPIO, 1);
        esp_rom_delay_us(delay_us);
        gpio_set_level(BUZZER_GPIO, 0);
        esp_rom_delay_us(delay_us);
    }
}

void app_main(void) {
    // configure button
    gpio_reset_pin(BUTTON_GPIO);
    gpio_set_direction(BUTTON_GPIO, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BUTTON_GPIO, GPIO_PULLUP_ONLY);

    // configure buzzer
    gpio_reset_pin(BUZZER_GPIO);
    gpio_set_direction(BUZZER_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(BUZZER_GPIO, 0);

    int last_state = 1;
    int curr_state;

    while (1) {
        curr_state = gpio_get_level(BUTTON_GPIO);

        if (last_state == 1 && curr_state == 0) {
            printf("1\n");
            play_tone(2000, 100); // 2 kHz for 100 ms
        }

        last_state = curr_state;
        vTaskDelay(pdMS_TO_TICKS(50)); // simple debounce
    }
}*/
