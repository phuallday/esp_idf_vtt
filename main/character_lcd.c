#include <string.h>
#include <stdio.h>

#include "esp_event.h"
#include "character_lcd.h"
#include "event_source.h"

#include "i2cdev.h"
#include "pcf8574.h"
#include "hd44780.h"

#define CONFIG_LCD_I2C_ADDR       0x27
#define CONFIG_LCD_I2C_MASTER_SDA 21
#define CONFIG_LCD_I2C_MASTER_SCL 22

ESP_EVENT_DEFINE_BASE(LCD_EVENT);
static i2c_dev_t pcf8574;

static esp_err_t write_lcd_data(const hd44780_t *lcd, uint8_t data) {
    return pcf8574_port_write(&pcf8574, data);
}

static hd44780_t lcd = {
    .write_cb = write_lcd_data,
    .font     = HD44780_FONT_5X8,
    .lines    = 4,
    .pins     = {
            .rs = 0,
            .e  = 2,
            .d4 = 4,
            .d5 = 5,
            .d6 = 6,
            .d7 = 7,
            .bl = 3}};

void lcd_show_handler(void *event_handler_arg,
                      esp_event_base_t event_base,
                      int32_t event_id,
                      void *event_data) {
    switch (event_id) {
    case LCD_SHOW_QR:
        hd44780_gotoxy(&lcd, 0, 0);
        char *data = malloc(20);
        strncpy(data, (const char *)event_data, 20);
        char buff[20];
        sprintf(buff, "%-19s", data);
        hd44780_puts(&lcd, buff);
        free(data);
        data = NULL;
        break;
    case LCD_SHOW_MESSAGE:
        char *substr1 = malloc(20);
        char *substr2 = malloc(20);
        strncpy(substr1, (const char *)event_data, 20);
        strncpy(substr2, (const char *)event_data + 20, 20);
        hd44780_gotoxy(&lcd, 0, 1);
        hd44780_puts(&lcd, substr1);
        hd44780_gotoxy(&lcd, 0, 2);
        hd44780_puts(&lcd, substr2);
        free(substr1);
        substr1 = NULL;
        free(substr2);
        substr2 = NULL;
        break;
    case LCD_SHOW_NEEDLE:
        hd44780_gotoxy(&lcd, 0, 3);
        hd44780_puts(&lcd, (const char *)event_data);
        break;
    };
}

void lcd_init() {
    memset(&pcf8574, 0, sizeof(i2c_dev_t));
    ESP_ERROR_CHECK(i2cdev_init());
    ESP_ERROR_CHECK(pcf8574_init_desc(&pcf8574, CONFIG_LCD_I2C_ADDR, 0, CONFIG_LCD_I2C_MASTER_SDA, CONFIG_LCD_I2C_MASTER_SCL));
    ESP_ERROR_CHECK(hd44780_init(&lcd));
    hd44780_clear(&lcd);
    hd44780_switch_backlight(&lcd, true);
    esp_event_handler_register(LCD_EVENT, LCD_SHOW_MESSAGE, lcd_show_handler, NULL);
    esp_event_handler_register(LCD_EVENT, LCD_SHOW_NEEDLE, lcd_show_handler, NULL);
    esp_event_handler_register(LCD_EVENT, LCD_SHOW_QR, lcd_show_handler, NULL);
}