#ifndef APPLICATION_H
#define APPLICATION_H

#include "HAL.h"

/* Brings a UART instance up ready for use (8N1, FIFOs enabled - see
 * HAL_UART_init) at the given baud rate. HAL_UART_init itself always
 * configures 9600 first; this overrides it afterward via
 * HAL_UART_set_baud_rate if a different rate was asked for. */
UART_status App_UART_Open(UART_INSTANCE uart, uint32_t baud_rate);

/* Cleanly shuts a UART instance down. */
UART_status App_UART_Close(UART_INSTANCE uart);

UART_status App_UART_SendChar(UART_INSTANCE uart, char c);

/* Sends the whole string, translating '\n' to "\r\n" for terminals.
 * Stops and returns the first error encountered, if any. */
UART_status App_UART_SendString(UART_INSTANCE uart, const char *str);

/* Blocking single-character receive. */
UART_status App_UART_ReceiveChar(UART_INSTANCE uart, char *out);

/* Blocking read until '\r', '\n', or max_len-1 bytes; buf is always
 * NUL-terminated. out_len (may be NULL) receives the number of
 * characters stored, excluding the terminator. */
UART_status App_UART_ReceiveLine(UART_INSTANCE uart, char *buf, uint32_t max_len, uint32_t *out_len);

/* Sends `len` raw bytes with no newline translation (binary-safe).
 * Stops and returns the first error encountered, if any. */
UART_status App_UART_SendBytes(UART_INSTANCE uart, const uint8_t *data, uint32_t len);

/* Sends an unsigned integer as decimal ASCII, no leading zeros - the
 * bare-metal substitute for printf("%u"). */
UART_status App_UART_SendUInt(UART_INSTANCE uart, uint32_t value);

/* Non-blocking: true if at least one character is waiting to be read. */
bool App_UART_DataAvailable(UART_INSTANCE uart);

/* Bounded single-character receive: gives up with HAL_UART_ERR_TIMEOUT
 * after timeout_iters empty polls instead of blocking forever. */
UART_status App_UART_ReceiveCharTimeout(UART_INSTANCE uart, char *out, uint32_t timeout_iters);

#endif
