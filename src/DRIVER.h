#ifndef PL011_DRIVER_H
#define PL011_DRIVER_H

#include <stdint.h>
#include "PL011_config.h"

/* Every function below returns 0 on success, -1 if the instance hasn't
 * been registered with uart_reg_init yet (or is out of range). */

void drv_uart_reg_init(UART_INSTANCE instance);

int drv_uart_write_UARTDR(UART_INSTANCE, uint8_t data);
int drv_uart_read_UARTDR(UART_INSTANCE, uart_dr_t *out);

int drv_uart_read_UARTRSR(UART_INSTANCE, uint32_t *out);
int drv_uart_write_UARTECR(UART_INSTANCE);

int drv_uart_read_UARTFR(UART_INSTANCE, uart_fr_t *out);

int drv_uart_write_UARTIBRD(UART_INSTANCE, uint16_t divisor);
int drv_uart_read_UARTIBRD(UART_INSTANCE, uint16_t *out);

int drv_uart_write_UARTFBRD(UART_INSTANCE, uint8_t fraction);
int drv_uart_read_UARTFBRD(UART_INSTANCE, uint8_t *out);

int drv_uart_write_UARTLCR_H(UART_INSTANCE, uart_lcrh_t value);
int drv_uart_read_UARTLCR_H(UART_INSTANCE, uart_lcrh_t *out);

int drv_uart_write_UARTCR(UART_INSTANCE, uart_cr_t value);
int drv_uart_read_UARTCR(UART_INSTANCE, uart_cr_t *out);

int drv_uart_write_UARTIFLS(UART_INSTANCE, uart_ifls_t value);
int drv_uart_read_UARTIFLS(UART_INSTANCE, uart_ifls_t *out);

int drv_uart_write_UARTIMSC(UART_INSTANCE, uart_int_t value);
int drv_uart_read_UARTIMSC(UART_INSTANCE, uart_int_t *out);

int drv_uart_read_UARTRIS(UART_INSTANCE, uart_int_t *out);
int drv_uart_read_UARTMIS(UART_INSTANCE, uart_int_t *out);

int drv_uart_write_UARTICR(UART_INSTANCE, uart_int_t value);

int drv_uart_write_UARTDMACR(UART_INSTANCE, uart_dmacr_t value);
int drv_uart_read_UARTDMACR(UART_INSTANCE, uart_dmacr_t *out);

#endif /* PL011_DRIVER_H */
