#include <stdlib.h>
#include "esp_event.h"
#include "event_source.h"
#include "character_lcd.h"
int min = 0, now = 0, max = 0;
void app_main(void) {
    esp_event_loop_create_default();
    lcd_init();
}
