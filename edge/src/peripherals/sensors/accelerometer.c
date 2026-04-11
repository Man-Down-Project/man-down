#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_task_wdt.h"

#include "driver/i2c.h"
#include "driver/gpio.h"

#include "bma400.h"
#include "bma400_defs.h"

#include <string.h>

#include "config/edge_config.h"
#include "system/event_task.h"
#include "system/system_events.h"

//-------- Hardware Config ---------//


#define I2C_MASTER_NUM      I2C_NUM_0
#define I2C_MASTER_FREQ_HZ  100000



/*------------ Globals -------------*/

static struct bma400_dev bma;
static TaskHandle_t bma400_task_handle = NULL;

uint8_t dev_addr = BMA400_I2C_ADDRESS_SDO_LOW;
static bool tilt_active = false;

static const char *TAG = "[ACC.BMA400]";
static volatile int last_int_level = -1;
/*----------------- ISR ------------------*/
static void IRAM_ATTR bma400_isr(void *arg)
{
    BaseType_t higher_woken = pdFALSE;

    last_int_level = gpio_get_level(BMA400_INT_PIN);
    vTaskNotifyGiveFromISR(bma400_task_handle, &higher_woken);
    
    if (higher_woken)
    {
        portYIELD_FROM_ISR();
    }  
}
/*---------------- I2C Driver---------------*/
void i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}
/*---------------- GPIO Interrupt ---------------*/
static void bma400_gpio_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BMA400_INT_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(BMA400_INT_PIN, bma400_isr, NULL);
}
/* ---------------- BMA400 BUS Interface -------------------*/
static int8_t bma400_i2c_read(uint8_t reg_addr,
                              uint8_t *data,
                              uint32_t len,
                              void *intf_ptr)
{
    uint8_t addr = *(uint8_t*)intf_ptr;

    esp_err_t err = i2c_master_write_read_device(
        I2C_MASTER_NUM,
        addr,
        &reg_addr,
        1,
        data,
        len,
        pdMS_TO_TICKS(100)
    );
    return (err == ESP_OK) ? BMA400_OK : BMA400_E_COM_FAIL;
}
static int8_t bma400_i2c_write(uint8_t reg_addr,
                               const uint8_t *data,
                               uint32_t len,
                               void *intf_ptr)
{
    uint8_t addr = *(uint8_t*)intf_ptr;

    uint8_t buffer[16];
    buffer[0] = reg_addr;
    memcpy(&buffer[1], data, len);

    esp_err_t err = i2c_master_write_to_device(
        I2C_MASTER_NUM,
        addr,
        buffer,
        len + 1,
        pdMS_TO_TICKS(100)
    );
    return (err == ESP_OK) ? BMA400_OK : BMA400_E_COM_FAIL;
}

static void bma400_delay_us(uint32_t period, void *intf_ptr)
{
    esp_rom_delay_us(period);
}


/* ------------------ Sensor Configuration -----------------*/
static void bma400_configure_sensor(void)
{
    struct bma400_sensor_conf conf = {0};

    conf.type = BMA400_ACCEL;
    conf.param.accel.odr = BMA400_ODR_100HZ;
    conf.param.accel.range = BMA400_RANGE_2G;
    conf.param.accel.data_src = BMA400_DATA_SRC_ACCEL_FILT_1;

    bma400_set_sensor_conf(&conf, 1, &bma);
}
/* -------------------- Interrupt Configuration ------------------*/
static void bma400_configure_interrupt(void)
{
    struct bma400_sensor_conf conf = {0};
    struct bma400_int_enable int_en = {0};
    struct bma400_device_conf pin_conf = {0};
    
    /* Generic interrupt 1 config*/
    conf.type = BMA400_GEN1_INT;

    conf.param.gen_int.gen_int_thres = 4;
    conf.param.gen_int.gen_int_dur = 2;
    
    conf.param.gen_int.axes_sel = BMA400_AXIS_Z_EN;

    conf.param.gen_int.data_src = BMA400_DATA_SRC_ACC_FILT1;

    conf.param.gen_int.criterion_sel = BMA400_ACTIVITY_INT;

    conf.param.gen_int.evaluate_axes = BMA400_ANY_AXES_INT;

    conf.param.gen_int.ref_update = BMA400_UPDATE_EVERY_TIME;

    conf.param.gen_int.hysteresis = BMA400_HYST_48_MG;

    conf.param.gen_int.int_chan = BMA400_INT_CHANNEL_1;

    bma400_set_sensor_conf(&conf, 1, &bma);

    

    int_en.type = BMA400_GEN1_INT_EN;
    int_en.conf = BMA400_ENABLE;

    bma400_enable_interrupt(&int_en, 1, &bma);

    pin_conf.type = BMA400_INT_PIN_CONF;
    pin_conf.param.int_conf.int_chan = BMA400_INT_CHANNEL_1;
    pin_conf.param.int_conf.pin_conf = BMA400_INT_PUSH_PULL_ACTIVE_0;
    bma400_set_device_conf(&pin_conf, 1, &bma);
    
}
/* ------------------- Sensor Init ---------------------- */
static void bma400_sensor_init(void)
{
    bma.intf = BMA400_I2C_INTF;
    bma.intf_ptr = &dev_addr;

    bma.read = bma400_i2c_read;
    bma.write = bma400_i2c_write;
    bma.delay_us = bma400_delay_us;
    
    vTaskDelay(pdMS_TO_TICKS(100));

    int8_t result = bma400_init(&bma);
    ESP_LOGI(TAG, "Init result: %d", result);

    ESP_LOGI(TAG, "Chip ID: 0x%02X", bma.chip_id);

    bma400_configure_sensor();
    
    bma400_set_power_mode(BMA400_MODE_NORMAL, &bma);

    bma400_configure_interrupt();
}
/* ------------------ Acceleration Reading -------------------- */
static void bma400_read_accel(void)
{
    struct bma400_sensor_data accel;

    if (bma400_get_accel_data(BMA400_DATA_ONLY, &accel, &bma) != BMA400_OK)
        return;
    
    //ESP_LOGI(TAG, "X=%d Y=%d Z=%d", accel.x, accel.y, accel.z);

    if (accel.z < -900 && !tilt_active)
    {
        tilt_active = true;
        ESP_LOGI(TAG, "Tilt detected -> alarm");
        system_event_post(EVENT_FALL_ALARM, 0);
    }
    if (accel.z > -200 && tilt_active)
    {
        tilt_active = false;

        ESP_LOGI(TAG, "Orientation restored");
        system_event_post(EVENT_BUTTON_RESET, 0);
        
    }
}
/* -------------------- Sensor task ----------------------*/

static void bma400_dump_interrupts(void); //<----- Only here for debug func

static void bma400_task(void *arg)
{
    uint16_t int_status;
    //ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    while(1)
    {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        //ESP_LOGI(TAG, "INT triggered, pin level=%d", last_int_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        if(bma400_get_interrupt_status(&int_status, &bma) != BMA400_OK)
        {
            ESP_LOGE(TAG, "INT status read failed");
            continue;
        }
        if (int_status == 0)
        {
            continue;
        }
        // bma400_dump_interrupts();
        // ESP_LOGI(TAG, "INT STATUS = 0x%04X", int_status);
        
        if (int_status & BMA400_ASSERTED_GEN1_INT)
        {
            bma400_read_accel();
            vTaskDelay(pdMS_TO_TICKS(10));
            bma400_get_interrupt_status(&int_status, &bma);
        }
        //esp_task_wdt_reset();
    }
}

void accelerometer_init(void)
{
    i2c_master_init();
    
    
    for (int i = 0; i < 10; i++)
    {
        ESP_LOGI(TAG, "INT idle level = %d", gpio_get_level(BMA400_INT_PIN));
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    xTaskCreate(bma400_task,
                "bma_400_task",
                4096,
                NULL,
                5,
                &bma400_task_handle);
    bma400_gpio_init();
    bma400_sensor_init();
}

static void bma400_dump_interrupts(void)
{
    uint8_t int_regs[2];

    if (bma400_get_regs(BMA400_REG_INT_STAT0, int_regs, 2, &bma) != BMA400_OK)
    {
        ESP_LOGE(TAG, "Failed to read interrupt registers");
        return;
    }

    uint16_t status = (int_regs[1] << 8) | int_regs[0];

    ESP_LOGI(TAG, "INT_RAW = 0x%04X", status);

    if (status & BMA400_ASSERTED_WAKEUP_INT)
        ESP_LOGI(TAG, "INT: WAKEUP");

    if (status & BMA400_ASSERTED_ORIENT_CH)
        ESP_LOGI(TAG, "INT: ORIENTATION CHANGE");

    if (status & BMA400_ASSERTED_GEN1_INT)
        ESP_LOGI(TAG, "INT: GENERIC 1");

    if (status & BMA400_ASSERTED_GEN2_INT)
        ESP_LOGI(TAG, "INT: GENERIC 2");

    if (status & BMA400_ASSERTED_ACT_CH_Z)
        ESP_LOGI(TAG, "INT: ACTIVITY Z");
    if (status & BMA400_ASSERTED_ACT_CH_Y)
        ESP_LOGI(TAG, "INT: ACTIVITY Y");
    if (status & BMA400_ASSERTED_ACT_CH_X)
        ESP_LOGI(TAG, "INT: ACTIVITY X");

    if (status & BMA400_ASSERTED_STEP_INT)
        ESP_LOGI(TAG, "INT: STEP");
}