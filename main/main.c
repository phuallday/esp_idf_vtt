#include "wifi.h"
#include "vtt_mqtt.h"
void app_main(void)
{
    wifi_start();
    mqtt_app_start();
}
