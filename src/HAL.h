#ifndef HAL_H
#define HAL_H

#include "DRIVER.h"
#include <stdbool.h>

typedef enum {
    HAL_UART_STATUS_OK = 0,
    HAL_UART_ERR_FRAMING,
    HAL_UART_ERR_PARITY,
    HAL_UART_ERR_BREAK,
    HAL_UART_ERR_OVERRUN,
    HAL_UART_ERR_INVALID_ARG,
    HAL_UART_ERR_NOT_INITIALIZED,
    HAL_UART_ERR_TIMEOUT,
} UART_status;

UART_status HAL_UART_init(UART_INSTANCE,uint32_t);
UART_status HAL_UART_disable(UART_INSTANCE);
UART_status HAL_UART_clear_errors(UART_INSTANCE);

bool HAL_UART_tx_ready(UART_INSTANCE);
bool HAL_UART_rx_ready(UART_INSTANCE);
bool HAL_UART_is_busy(UART_INSTANCE);

UART_status HAL_UART_write(UART_INSTANCE, uint8_t data);
UART_status HAL_UART_read(UART_INSTANCE, uint8_t *out_byte);
UART_status HAL_UART_read_timeout(UART_INSTANCE, uint8_t *out_byte, uint32_t timeout_iters);

UART_status HAL_UART_enable_TX(UART_INSTANCE uart);
UART_status HAL_UART_disable_TX(UART_INSTANCE uart);
UART_status HAL_UART_enable_RX(UART_INSTANCE uart);
UART_status HAL_UART_disable_RX(UART_INSTANCE uart);

UART_status HAL_UART_set_baud_rate(UART_INSTANCE, uint32_t baud_rate);

UART_status HAL_UART_enable_loop_back(UART_INSTANCE uart);

UART_status HAL_UART_enable_parity(UART_INSTANCE uart);
UART_status HAL_UART_disable_parity(UART_INSTANCE uart);

UART_status HAL_UART_set_data_bits(UART_INSTANCE uart, int numOfDataBits);

UART_status HAL_UART_enable_interrupts(UART_INSTANCE uart, uart_int_t sources);
UART_status HAL_UART_disable_interrupts(UART_INSTANCE uart, uart_int_t sources);
UART_status HAL_UART_clear_interrupts(UART_INSTANCE uart, uart_int_t sources);
UART_status HAL_UART_get_pending(UART_INSTANCE uart, uart_int_t *out);
UART_status HAL_UART_set_fifo_levels(UART_INSTANCE uart, uart_ifls_t levels);

#endif