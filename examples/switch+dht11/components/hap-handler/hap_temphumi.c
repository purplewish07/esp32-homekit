#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"


#include "hap.h"
#include "dht11.h"


static const char *TAG = "homekit DHT";


/*Homekit macros*/
#define ACCESSORY_NAME  "TEMP/HUMI/SW"
#define MANUFACTURER_NAME   "YOUNGHYUN"
#define MODEL_NAME  "ESP32_ACC"
#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

/* Light Switch */
#define LED_R    0
#define LED_G    2
#define LED_B    4


void light_io_init(void)
{
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO15/16
    //io_conf.pin_bit_mask = GPIO_LED_PIN_SEL;
    io_conf.pin_bit_mask = (1ULL<<LED_R) | (1ULL<<LED_G) | (1ULL<<LED_B);
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
}


static void* acc_ins;
static void* _ev_handle;
static int led = false;

void* led_read(void* arg)
{
    ESP_LOGI(TAG,"LED READ\n");
    return (void*)led;
}


void led_write(void* arg, void* value, int len)
{
   ESP_LOGI(TAG,"LED WRITE. %d\n", (int)value);

    led = (int)value;
    if (value) {
        led = true;
        gpio_set_level(LED_G, 1);
    }
    else {
        led = false;
        gpio_set_level(LED_G, 0);
    }

    if (_ev_handle)
        hap_event_response(acc_ins, _ev_handle, (void*)led);

    return;
}


void led_notify(void* arg, void* ev_handle, bool enable)
{
    if (enable) {
        _ev_handle = ev_handle;
    }
    else {
        _ev_handle = NULL;
    }
}


/* DHT11 FIXME */
static gpio_num_t DHT22_GPIO = GPIO_NUM_22;

static SemaphoreHandle_t ev_mutex;
static int temperature = 0;
static int humidity = 0;
static void* _temperature_ev_handle = NULL;
static void* _humidity_ev_handle =  NULL;

void temperature_humidity_monitoring_task(void* arm)
{
    DHT_Sensor sensor;
    sensor.pin = GPIO_NUM_22;
    sensor.type = DHT11;

    DHTInit(&sensor);
	sleepms(1000);
    while(1){
        if(DHTRead(&sensor, &dht_data)){
            temperature = dht_data.temperature *100;
            humidity = dht_data.humidity*100;
            
            ESP_LOGI(TAG,"T: %d, H: %d", temperature,humidity);

        } else{
            ESP_LOGW(TAG,"Read sensor failed, retrying");
        }

        //xSemaphoreTake(ev_mutex, 0);

#if 1
        if (_humidity_ev_handle)
            hap_event_response(acc_ins, _humidity_ev_handle, (void*)humidity);

        if (_temperature_ev_handle)
            hap_event_response(acc_ins, _temperature_ev_handle, (void*)temperature);

#endif
        //xSemaphoreGive(ev_mutex);

        printf("%d %d\n", temperature, humidity);
        vTaskDelay( 3000 / portTICK_RATE_MS );
    }
}


static void* _temperature_read(void* arg)
{
    ESP_LOGI("MAIN", "_temperature_read");
    return (void*)temperature;
}


void _temperature_notify(void* arg, void* ev_handle, bool enable)
{
    ESP_LOGI("MAIN", "_temperature_notify");
    //xSemaphoreTake(ev_mutex, 0);

    if (enable) 
        _temperature_ev_handle = ev_handle;
    else 
        _temperature_ev_handle = NULL;

    //xSemaphoreGive(ev_mutex);
}


static void* _humidity_read(void* arg)
{
    ESP_LOGI("MAIN", "_humidity_read");
    return (void*)humidity;
}


void _humidity_notify(void* arg, void* ev_handle, bool enable)
{
    ESP_LOGI("MAIN", "_humidity_notify");
    //xSemaphoreTake(ev_mutex, 0);

    if (enable) 
        _humidity_ev_handle = ev_handle;
    else 
        _humidity_ev_handle = NULL;

    //xSemaphoreGive(ev_mutex);
}


static bool _identifed = false;
void* identify_read(void* arg)
{
    return (void*)true;
}


void hap_object_init(void* arg)
{
    void* accessory_object = hap_accessory_add(acc_ins);
    struct hap_characteristic cs[] = {
        {HAP_CHARACTER_IDENTIFY, (void*)true, NULL, identify_read, NULL, NULL},
        {HAP_CHARACTER_MANUFACTURER, (void*)MANUFACTURER_NAME, NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_MODEL, (void*)MODEL_NAME, NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_NAME, (void*)ACCESSORY_NAME, NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_SERIAL_NUMBER, (void*)"0123456789", NULL, NULL, NULL, NULL},
        {HAP_CHARACTER_FIRMWARE_REVISION, (void*)"1.0", NULL, NULL, NULL, NULL},
    };
    hap_service_and_characteristics_add(acc_ins, accessory_object, HAP_SERVICE_ACCESSORY_INFORMATION, cs, ARRAY_SIZE(cs));

    struct hap_characteristic humidity_sensor[] = {
        {HAP_CHARACTER_CURRENT_RELATIVE_HUMIDITY, (void*)humidity, NULL, _humidity_read, NULL, _humidity_notify},
        {HAP_CHARACTER_NAME, (void*)"HYGROMETER" , NULL, NULL, NULL, NULL},
    };
    hap_service_and_characteristics_add(acc_ins, accessory_object, HAP_SERVICE_HUMIDITY_SENSOR, humidity_sensor, ARRAY_SIZE(humidity_sensor));

    struct hap_characteristic temperature_sensor[] = {
        {HAP_CHARACTER_CURRENT_TEMPERATURE, (void*)temperature, NULL, _temperature_read, NULL, _temperature_notify},
        {HAP_CHARACTER_NAME, (void*)"THERMOMETER" , NULL, NULL, NULL, NULL},
    };
    hap_service_and_characteristics_add(acc_ins, accessory_object, HAP_SERVICE_TEMPERATURE_SENSOR, temperature_sensor, ARRAY_SIZE(temperature_sensor));

    struct hap_characteristic cc[] = {
        {HAP_CHARACTER_ON, (void*)led, NULL, led_read, led_write, led_notify},

    };
    hap_service_and_characteristics_add(acc_ins, accessory_object, HAP_SERVICE_SWITCHS, cc, ARRAY_SIZE(cc));

}


void hap_register_device_handler(char *acc_id)
{
    hap_init();

    hap_accessory_callback_t callback;
    callback.hap_object_init = hap_object_init;
    acc_ins = hap_accessory_register((char*)ACCESSORY_NAME, acc_id, (char*)"111-11-111", (char*)MANUFACTURER_NAME, HAP_ACCESSORY_CATEGORY_OTHER, 811, 1, NULL, &callback);

}


