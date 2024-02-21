#include <stdlib.h>
#include "esp_event.h"
#include "event_source.h"
#include "character_lcd.h"

void app_main(void) {
    esp_event_loop_create_default();
    lcd_init();
}
