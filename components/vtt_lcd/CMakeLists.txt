idf_component_register(SRCS "vtt_lcd.c"
                    INCLUDE_DIRS "."
                    REQUIRES i2cdev hd44780 pcf8574)
