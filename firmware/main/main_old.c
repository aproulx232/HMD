#include "freertos/FreeRTOS.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"

#include "features.h"
#include <stdio.h>
#include <string.h>
#include "freertos/task.h"
#include "esp_bt.h"
#include "esp_log.h"
static const char *tag = "BLE_ADV";


esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}


void app_main(void)
{
    // BLE inits
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();

    ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret) {
        ESP_LOGI(tag, "Bluetooth controller release classic bt memory failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGI(tag, "Bluetooth controller initialize failed: %s", esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_BLE)) != ESP_OK) {
        ESP_LOGI(tag, "Bluetooth controller enable failed: %s", esp_err_to_name(ret));
        return;
    }


    // Main tasks
    xTaskCreatePinnedToCore(&bleAdvt_task, "bleAdvt_task", 2048, NULL, 5, NULL, 0);
    //xTaskCreate(&getBPM_task, "getBPM_task", 4096, NULL, 5, NULL);
    //xTaskCreate(&blink_task, "blink_task", configMINIMAL_STACK_SIZE, NULL, 5, NULL);
    //xTaskCreate(&e_paper_task, "epaper_task", 4 * 1024, NULL, 5, NULL);

    //xTaskCreatePinnedToCore(&bleServer_task, "bleServer_task", 4096, NULL, 5, NULL, 0);

}
