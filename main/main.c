#include "wifi.h"
#include "vtt_mqtt.h"
#include "qr_reader.h"
void app_main(void)
{
    wifi_start();
    mqtt_app_start();
    qr_reader_app_start();
}
