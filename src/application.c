#include "application.h"

UART_status App_UART_Open(UART_INSTANCE uart, uint32_t baud_rate)
{
    return HAL_UART_init(uart, baud_rate);
}

UART_status App_UART_Close(UART_INSTANCE uart)
{
    return HAL_UART_disable(uart);
}

UART_status App_UART_SendChar(UART_INSTANCE uart, char c)
{
    return HAL_UART_write(uart, (uint8_t)c);
}

UART_status App_UART_SendString(UART_INSTANCE uart, const char *str)
{
    UART_status status;

    while (*str)
    {
        if (*str == '\n')
        {
            status = HAL_UART_write(uart, '\r');
            if (status != HAL_UART_STATUS_OK)
                return status;
        }

        status = HAL_UART_write(uart, (uint8_t)*str);
        if (status != HAL_UART_STATUS_OK)
            return status;

        str++;
    }

    return HAL_UART_STATUS_OK;
}

UART_status App_UART_ReceiveChar(UART_INSTANCE uart, char *out)
{
    if (!out)
        return HAL_UART_ERR_INVALID_ARG;

    return HAL_UART_read(uart, (uint8_t *)out);
}

UART_status App_UART_ReceiveLine(UART_INSTANCE uart, char *buf, uint32_t max_len, uint32_t *out_len)
{
    uint32_t i = 0;
    uint8_t byte;
    UART_status status;

    if (!buf || max_len == 0)
        return HAL_UART_ERR_INVALID_ARG;

    while (i < max_len - 1)
    {
        status = HAL_UART_read(uart, &byte);
        if (status != HAL_UART_STATUS_OK)
            return status;

        if (byte == '\r' || byte == '\n')
            break;

        buf[i++] = (char)byte;
    }

    buf[i] = '\0';
    if (out_len)
        *out_len = i;

    return HAL_UART_STATUS_OK;
}

UART_status App_UART_SendBytes(UART_INSTANCE uart, const uint8_t *data, uint32_t len)
{
    UART_status status;

    if (!data)
        return HAL_UART_ERR_INVALID_ARG;

    for (uint32_t i = 0; i < len; i++)
    {
        status = HAL_UART_write(uart, data[i]);
        if (status != HAL_UART_STATUS_OK)
            return status;
    }

    return HAL_UART_STATUS_OK;
}

UART_status App_UART_SendUInt(UART_INSTANCE uart, uint32_t value)
{
    char buf[11];                     /* up to 10 digits for a 32-bit value + NUL */
    uint32_t i = sizeof(buf) - 1;

    /* Build the decimal string back-to-front, then send from the first
     * digit. do-while so that value == 0 still emits a single '0'. */
    buf[i] = '\0';
    do
    {
        buf[--i] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value != 0u && i > 0u);

    return App_UART_SendString(uart, &buf[i]);
}

bool App_UART_DataAvailable(UART_INSTANCE uart)
{
    return HAL_UART_rx_ready(uart);
}

UART_status App_UART_ReceiveCharTimeout(UART_INSTANCE uart, char *out, uint32_t timeout_iters)
{
    if (!out)
        return HAL_UART_ERR_INVALID_ARG;

    return HAL_UART_read_timeout(uart, (uint8_t *)out, timeout_iters);
}
