#ifndef EVENT_SOURCE_H
#define EVENT_SOURCE_H

#include "esp_event.h"

ESP_EVENT_DECLARE_BASE(LCD_EVENT);

enum{
    LCD_SHOW_MESSAGE,
    LCD_SHOW_QR,
    LCD_SHOW_NEEDLE,
};

#endif 