/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"
#include "esp_mac.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_vtt.h"
#include "event_source.h"

static const char *TAG = "MQTT";

static char *SERVER = "uat";
static char MAC_STR[18];
static char PUB_LINK[30];     // uat/Production_Process/1
static char MAC_PUB_LINK[30]; // uat/Production_Change/1
static char MAC_SUB_LINK[30]; // uat/00:11:22:33:44:55/2
static char DEVICE_CODE_IN[35];
static char DEVICE_CODE_OUT[35];
static char IN_SUB_LINK[50];
static char OUT_SUB_LINK[50];
static char CURRENT_MODEL[13];

static void log_error_if_nonzero(const char *message, int error_code) {
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event   = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, MAC_SUB_LINK, 1);
        ESP_LOGI(TAG, "sent subscribe MAC_SUB_LINK successful, msg_id=%d", msg_id);
        if (strlen(DEVICE_CODE_IN) != 0) {
            msg_id = esp_mqtt_client_subscribe(client, IN_SUB_LINK, 1);
            ESP_LOGI(TAG, "sent subscribe IN_SUB_LINK successful, msg_id=%d", msg_id);
        }
        if (strlen(DEVICE_CODE_OUT) != 0) {
            msg_id = esp_mqtt_client_subscribe(client, OUT_SUB_LINK, 1);
            ESP_LOGI(TAG, "sent subscribe OUT_SUB_LINK successful, msg_id=%d", msg_id);
        }
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void qr_scanner_data_handler(void *event_handler_arg,
                             esp_event_base_t event_base,
                             int32_t event_id,
                             void *event_data) {
    switch (event_id) {
    case QR_RECEIVED:
        printf("QR_RECEIVED:%s\n", (const char *)event_data);
        break;
    }
}

void mqtt_vtt_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            }
            else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    }
    else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    esp_event_handler_register(QR_SCANNER_EVENT, QR_RECEIVED, qr_scanner_data_handler, NULL);

    sprintf(PUB_LINK, "%s/Production_Process/1", SERVER);
    sprintf(MAC_PUB_LINK, "%s/Production_Change/1", SERVER);
    uint8_t *MAC_ADDR = calloc(6, sizeof(uint8_t));
    esp_read_mac(MAC_ADDR, ESP_MAC_BASE);
    sprintf(MAC_STR, "%02x:%02x:%02x:%02x:%02x:%02x", MAC_ADDR[0], MAC_ADDR[1], MAC_ADDR[2], MAC_ADDR[3], MAC_ADDR[4], MAC_ADDR[5]);
    sprintf(MAC_SUB_LINK, "%s/%s/2", SERVER, MAC_STR);
    free(MAC_ADDR);
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    nvs_handle_t nvs_handle;
    err = nvs_open("stogare", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    }
    else {
        printf("Reading DEVICE_CODE_IN from NVS ... ");
        err = nvs_get_str(nvs_handle, "DEVI_CODE_IN", DEVICE_CODE_IN, sizeof(DEVICE_CODE_IN));
        switch (err) {
        case ESP_OK:
            printf("DEVICE_CODE_IN = %s\n", DEVICE_CODE_IN);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
        printf("Reading DEVICE_CODE_OUT from NVS ... ");
        err = nvs_get_str(nvs_handle, "DEVI_CODE_OUT", DEVICE_CODE_OUT, sizeof(DEVICE_CODE_OUT));
        switch (err) {
        case ESP_OK:
            printf("DEVICE_CODE_OUT = %s\n", DEVICE_CODE_OUT);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
        printf("Reading CURRENT_MODEL from NVS ... ");
        err = nvs_get_str(nvs_handle, "CURRENT_MODEL", CURRENT_MODEL, sizeof(CURRENT_MODEL));
        switch (err) {
        case ESP_OK:
            printf("CURRENT_MODEL = %s\n", CURRENT_MODEL);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            printf("The value is not initialized yet!\n");
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }
        nvs_close(nvs_handle);
    }
    if (strlen(DEVICE_CODE_IN) != 0) {
        sprintf(IN_SUB_LINK, "%s/%s/2", SERVER, DEVICE_CODE_IN);
        printf("IN_SUB_LINK:%s", IN_SUB_LINK);
    }
    if (strlen(DEVICE_CODE_OUT) != 0) {
        sprintf(OUT_SUB_LINK, "%s/%s/2", SERVER, DEVICE_CODE_OUT);
        printf("OUT_SUB_LINK:%s", OUT_SUB_LINK);
    }
}
