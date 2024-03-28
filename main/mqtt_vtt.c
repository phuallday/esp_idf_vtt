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
#include "cJSON.h"

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

esp_mqtt_client_handle_t client;

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
        char *topic = calloc(event->topic_len, sizeof(char));
        char *data  = calloc(event->data_len, sizeof(char));
        sprintf(topic, "%.*s", event->topic_len, event->topic);
        sprintf(data, "%.*s", event->data_len, event->data);
        printf("%s\n", topic);
        printf("%s\n", data);
        if (strcmp(topic, MAC_SUB_LINK) == 0) {
            printf("Change Product Type:\n");
            cJSON *jsonData = cJSON_Parse(data);
            if (jsonData == NULL) {
                const char *error_ptr = cJSON_GetErrorPtr();
                if (error_ptr != NULL) {
                    printf("Error: %s\n", error_ptr);
                }
            }
            else {
                cJSON *ErrCode   = cJSON_GetObjectItemCaseSensitive(jsonData, "ErrCode");
                cJSON *ErrMsgStr = cJSON_GetObjectItemCaseSensitive(jsonData, "ErrMsg");
                cJSON *ErrMsg    = cJSON_Parse(ErrMsgStr->valuestring);
                if (cJSON_IsString(ErrCode) && (ErrCode->valuestring != NULL)) {
                    printf("ErrCode: %s\n", ErrCode->valuestring);
                    if (strcmp(ErrCode->valuestring, "31") == 0) {
                        if (strlen(DEVICE_CODE_IN) > 0) {
                            int msg_id = esp_mqtt_client_unsubscribe(client, IN_SUB_LINK);
                            ESP_LOGI(TAG, "sent unsubscribe IN_SUB_LINK successful, msg_id=%d", msg_id);
                        }
                        if (strlen(DEVICE_CODE_OUT) > 0) {
                            int msg_id = esp_mqtt_client_unsubscribe(client, OUT_SUB_LINK);
                            ESP_LOGI(TAG, "sent unsubscribe OUT_SUB_LINK successful, msg_id=%d", msg_id);
                        }
                        memset(DEVICE_CODE_IN, 0, sizeof DEVICE_CODE_IN);
                        memset(DEVICE_CODE_OUT, 0, sizeof DEVICE_CODE_OUT);
                        cJSON *Device = NULL;
                        cJSON_ArrayForEach(Device, ErrMsg) {
                            cJSON *STEP_TYPE   = cJSON_GetObjectItemCaseSensitive(Device, "STEP_TYPE");
                            cJSON *DEVICE_CODE = cJSON_GetObjectItemCaseSensitive(Device, "DEVICE_CODE");
                            if (cJSON_IsString(STEP_TYPE) && (STEP_TYPE->valuestring != NULL)) {
                                if (cJSON_IsString(DEVICE_CODE) && (DEVICE_CODE->valuestring != NULL)) {
                                    if (strcmp(STEP_TYPE->valuestring, "IN") == 0) {
                                        sprintf(DEVICE_CODE_IN, "%s", DEVICE_CODE->valuestring);
                                        printf("DEVICE_CODE_IN:%s\n", DEVICE_CODE_IN);
                                    }
                                    else if (strcmp(STEP_TYPE->valuestring, "OUT") == 0) {
                                        sprintf(DEVICE_CODE_OUT, "%s", DEVICE_CODE->valuestring);
                                        printf("DEVICE_CODE_OUT:%s\n", DEVICE_CODE_OUT);
                                    }
                                }
                            }
                            else {
                                printf("STEP_TYPE is not string type or null value.\n");
                            }
                        }

                        if (strlen(DEVICE_CODE_IN) > 0) {
                            sprintf(IN_SUB_LINK, "%s/%s/2", SERVER, DEVICE_CODE_IN);
                            int msg_id = esp_mqtt_client_subscribe(client, IN_SUB_LINK, 1);
                            ESP_LOGI(TAG, "sent subscribe IN_SUB_LINK successful, msg_id=%d", msg_id);
                            printf("IN_SUB_LINK:%s\n", IN_SUB_LINK);
                        }
                        if (strlen(DEVICE_CODE_OUT) > 0) {
                            sprintf(OUT_SUB_LINK, "%s/%s/2", SERVER, DEVICE_CODE_OUT);
                            int msg_id = esp_mqtt_client_subscribe(client, OUT_SUB_LINK, 1);
                            ESP_LOGI(TAG, "sent subscribe OUT_SUB_LINK successful, msg_id=%d", msg_id);
                            printf("IN_SUB_LINK:%s\n", OUT_SUB_LINK);
                        }
                    }
                    else if (strcmp(ErrCode->valuestring, "32") == 0) {
                        printf("SHOW_LCD:%s\n", ErrMsg->valuestring);
                        esp_event_post(LCD_EVENT, LCD_SHOW_MESSAGE, ErrMsg->valuestring, strlen(ErrMsg->valuestring) + 1, portMAX_DELAY);
                    }
                }
                else {
                    printf("ErrCode: %s\n", "err");
                }
                cJSON_Delete(ErrMsg);
            }
            cJSON_Delete(jsonData);
        }
        else {
        }
        free(topic);
        free(data);
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
        const char *qrStr = (const char *)event_data;
        printf("QR_RECEIVED:%s\n", qrStr);
        if (strncmp(qrStr, "PARTN_", strlen("PARTN_")) == 0) {
            printf("QR is change product type.\n");
            cJSON *changTypeJson = cJSON_CreateObject();
            cJSON_AddNumberToObject(changTypeJson, "ID", rand());
            cJSON_AddStringToObject(changTypeJson, "MAC_ADDRESS", MAC_STR);
            cJSON_AddStringToObject(changTypeJson, "QR_CODE", qrStr);
            char *json_ChangeTypeStr = cJSON_PrintUnformatted(changTypeJson);
            printf("%s\n", json_ChangeTypeStr);
            int msg_id = esp_mqtt_client_publish(client, MAC_PUB_LINK, json_ChangeTypeStr, strlen(json_ChangeTypeStr), 1, false);
            ESP_LOGI(TAG, "sent publish MAC_PUB_LINK successful, msg_id=%d", msg_id);
            cJSON_free(json_ChangeTypeStr);
            cJSON_Delete(changTypeJson);
        }
        else if (strncmp(qrStr, "em_", strlen("em_")) == 0 ||
                 strncmp(qrStr, "bc_", strlen("bc_")) == 0) {
            printf("QR is bactchcard or employee type.\n");
        }
        break;
    }
}

void mqtt_vtt_init(void) {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);

    esp_event_handler_register(QR_SCANNER_EVENT, QR_RECEIVED, qr_scanner_data_handler, NULL);

    sprintf(PUB_LINK, "%s/production_process/1", SERVER);
    sprintf(MAC_PUB_LINK, "%s/production_change/1", SERVER);
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
        size_t device_code_in_buf = sizeof DEVICE_CODE_IN;
        err                       = nvs_get_str(nvs_handle, "DEVI_CODE_IN", DEVICE_CODE_IN, &device_code_in_buf);
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
        size_t device_code_out_buf = sizeof DEVICE_CODE_OUT;
        err                        = nvs_get_str(nvs_handle, "DEVI_CODE_OUT", DEVICE_CODE_OUT, &device_code_out_buf);
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
        size_t curr_model_buf = sizeof CURRENT_MODEL;
        err                   = nvs_get_str(nvs_handle, "CURRENT_MODEL", CURRENT_MODEL, &curr_model_buf);
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
