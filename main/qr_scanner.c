#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "driver/uart.h"

#include "qr_scanner.h"

static const char *TAG = "QR_SCANNER";

#define UART_NUM UART_NUM_2
#define PATTERN_CHR_NUM    (3)         /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/

#define BUF_SIZE (1024)
#define RD_BUF_SIZE (BUF_SIZE)
static QueueHandle_t uart2_queue;



void qr_scanner_init(void){

}