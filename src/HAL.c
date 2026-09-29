// EASY LOGGER LIBRARY
#include "HAL.h"

UART_status HAL_UART_init(UART_INSTANCE uart_instance, uint32_t baud)
{
    // Initialize the UART instace in the driver layer.
    drv_uart_reg_init(uart_instance);

    /* disable before reconfiguring anything - don't touch a UART that
     * might currently be mid-transmission */
    while (HAL_UART_is_busy(uart_instance))
        ;

    /* Zeroing out the control register to start anew */
    if (drv_uart_write_UARTCR(uart_instance, (uart_cr_t){0}) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;
    /* Zeroing out the format register to start anew */
    if (drv_uart_write_UARTLCR_H(uart_instance, (uart_lcrh_t){0}) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;
    /* Clearing the Stick Errors */
    UART_status status = HAL_UART_clear_errors(uart_instance);
    if (status != HAL_UART_STATUS_OK)
        return status;

    /* Clearing the interrupts */
    uart_int_t intr_clr = {0};
    intr_clr.raw = 0x7FFu; /* all 11 real interrupt bits */

    if (drv_uart_write_UARTICR(uart_instance, intr_clr) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    uart_cr_t initialize = (uart_cr_t){0};
    initialize.raw = 0x0;
    initialize.uarten = 0x1;
    initialize.txe = 0x1;
    initialize.rxe = 0x1;

    uart_lcrh_t format = {0}; /* zero first so reserved bits aren't written garbage */
    format.brk = 0x0;  // break condition
    format.pen = 0x1;  // parity enable
    format.eps = 0x0;  // even/odd parity
    format.stp2 = 0x0; // number of stop bits
    format.fen = 0x1;  // enable 16-byte TX/RX FIFO
    format.wlen = 0x3; // data length
    format.sps = 0x0;  // stick parity

    if (drv_uart_write_UARTLCR_H(uart_instance, format) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    status = HAL_UART_set_baud_rate(uart_instance, baud);
    if (status != HAL_UART_STATUS_OK)
        return status;

    if (drv_uart_write_UARTCR(uart_instance, initialize) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_disable(UART_INSTANCE uart_instance)
{
    while (HAL_UART_is_busy(uart_instance))
        ;

    uart_cr_t disable = (uart_cr_t){0};

    if (drv_uart_read_UARTCR(uart_instance, &disable) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    disable.uarten = 0x0;
    if (drv_uart_write_UARTCR(uart_instance, disable) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_clear_errors(UART_INSTANCE uart_instance)
{
    if (drv_uart_write_UARTECR(uart_instance) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

bool HAL_UART_tx_ready(UART_INSTANCE uart)
{
    uart_fr_t fr;
    if (drv_uart_read_UARTFR(uart, &fr) != 0)
        return false;

    return !fr.txff;
}

bool HAL_UART_rx_ready(UART_INSTANCE uart)
{
    uart_fr_t fr;
    if (drv_uart_read_UARTFR(uart, &fr) != 0)
        return false;

    return !fr.rxfe;
}

bool HAL_UART_is_busy(UART_INSTANCE uart)
{
    uart_fr_t fr;
    if (drv_uart_read_UARTFR(uart, &fr) != 0)
        return false;

    return fr.busy;
}

UART_status HAL_UART_write(UART_INSTANCE uart, uint8_t data)
{
    uart_fr_t fr;

    do
    {
        if (drv_uart_read_UARTFR(uart, &fr) != 0)
            return HAL_UART_ERR_NOT_INITIALIZED;
    } while (fr.txff);

    if (drv_uart_write_UARTDR(uart, data) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_read(UART_INSTANCE uart, uint8_t *out_byte)
{
    return HAL_UART_read_timeout(uart, out_byte, UINT32_MAX);
}

/* Same as HAL_UART_read, but gives up with HAL_UART_ERR_TIMEOUT after
 * timeout_iters empty-FIFO polls instead of waiting forever - use this
 * for anything that must not be able to hang the system (e.g. a
 * loopback self-test on hardware/emulation that may not deliver data). */
UART_status HAL_UART_read_timeout(UART_INSTANCE uart, uint8_t *out_byte, uint32_t timeout_iters)
{
    uart_fr_t fr;
    uart_dr_t dr;

    if (!out_byte)
        return HAL_UART_ERR_INVALID_ARG;

    for (;;)
    {
        if (drv_uart_read_UARTFR(uart, &fr) != 0)
            return HAL_UART_ERR_NOT_INITIALIZED;

        if (!fr.rxfe)
            break;

        if (timeout_iters == 0)
            return HAL_UART_ERR_TIMEOUT;
        timeout_iters--;
    }

    if (drv_uart_read_UARTDR(uart, &dr) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    *out_byte = dr.data;

    if (dr.fe)
        return HAL_UART_ERR_FRAMING;
    if (dr.pe)
        return HAL_UART_ERR_PARITY;
    if (dr.be)
        return HAL_UART_ERR_BREAK;
    if (dr.oe)
        return HAL_UART_ERR_OVERRUN;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_set_baud_rate(UART_INSTANCE uart, uint32_t baud_rate)
{
    if (baud_rate == 0)
        return HAL_UART_ERR_INVALID_ARG;

    uint32_t divisor_x64 = (4 * UARTCLK_HZ + baud_rate / 2) / baud_rate;
    uint16_t ibrd = (uint16_t)(divisor_x64 >> 6);
    uint8_t fbrd = (uint8_t)(divisor_x64 & 0x3Fu);
    uart_lcrh_t lcrh;

    if (drv_uart_write_UARTIBRD(uart, ibrd) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    if (drv_uart_write_UARTFBRD(uart, fbrd) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    /* re-write LCR_H unchanged - the write itself latches the new divisor */
    if (drv_uart_read_UARTLCR_H(uart, &lcrh) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    if (drv_uart_write_UARTLCR_H(uart, lcrh) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

#define CR_BIT_TXE (1u << 8)
#define CR_BIT_RXE (1u << 9)

static UART_status set_cr_bit(UART_INSTANCE uart, uint32_t bit, bool enable)
{
    uart_cr_t control;
    if (drv_uart_read_UARTCR(uart, &control) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    if (enable)
        control.raw |= bit;
    else
        control.raw &= ~bit;

    if (drv_uart_write_UARTCR(uart, control) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_enable_TX(UART_INSTANCE uart)
{
    return set_cr_bit(uart, CR_BIT_TXE, true);
}

UART_status HAL_UART_disable_TX(UART_INSTANCE uart)
{
    return set_cr_bit(uart, CR_BIT_TXE, false);
}

UART_status HAL_UART_enable_RX(UART_INSTANCE uart)
{
    return set_cr_bit(uart, CR_BIT_RXE, true);
}

UART_status HAL_UART_disable_RX(UART_INSTANCE uart)
{
    return set_cr_bit(uart, CR_BIT_RXE, false);
}

UART_status HAL_UART_enable_loop_back(UART_INSTANCE uart)
{
    uart_cr_t loop_back;
    if (drv_uart_read_UARTCR(uart, &loop_back) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    loop_back.lbe = 0x1;
    if (drv_uart_write_UARTCR(uart, loop_back) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

static UART_status parity_toggle(UART_INSTANCE uart, bool toggle)
{
    uart_lcrh_t format;

    if (drv_uart_read_UARTLCR_H(uart, &format) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    format.pen = toggle;

    if (drv_uart_write_UARTLCR_H(uart, format) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_enable_parity(UART_INSTANCE uart)
{
    return parity_toggle(uart, 1);
}

UART_status HAL_UART_disable_parity(UART_INSTANCE uart)
{
    return parity_toggle(uart, 0);
}

/* Maps a human word-length (5..8 data bits) to the 2-bit UARTLCR_H.wlen
 * encoding. Kept here (not in the header) so it has one definition. */
static const uint8_t bitMapping[9] = {
    [5] = 0x0,
    [6] = 0x1,
    [7] = 0x2,
    [8] = 0x3,
};

UART_status HAL_UART_set_data_bits(UART_INSTANCE uart, int numOfDataBits)
{
    uart_lcrh_t dataBits;

    if (numOfDataBits < 5 || numOfDataBits > 8)
        return HAL_UART_ERR_INVALID_ARG;

    if (drv_uart_read_UARTLCR_H(uart, &dataBits) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    dataBits.wlen = bitMapping[numOfDataBits];

    if (drv_uart_write_UARTLCR_H(uart, dataBits) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_enable_interrupts(UART_INSTANCE uart, uart_int_t sources)
{
    uart_int_t mask = {0};
    if (drv_uart_read_UARTIMSC(uart, &mask) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    mask.raw |= sources.raw;

    if (drv_uart_write_UARTIMSC(uart, mask) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_disable_interrupts(UART_INSTANCE uart, uart_int_t sources)
{
    uart_int_t mask = {0};
    if (drv_uart_read_UARTIMSC(uart, &mask) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    mask.raw &= ~sources.raw;

    if (drv_uart_write_UARTIMSC(uart, mask) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_clear_interrupts(UART_INSTANCE uart, uart_int_t sources)
{
    if (drv_uart_write_UARTICR(uart, sources) != 0) // Because writing a zero has no affect.
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_get_pending(UART_INSTANCE uart, uart_int_t *out)
{
    if (!out)
        return HAL_UART_ERR_INVALID_ARG;

    if (drv_uart_read_UARTMIS(uart, out) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}

UART_status HAL_UART_set_fifo_levels(UART_INSTANCE uart, uart_ifls_t levels)
{
    if (levels.txiflsel > 4 || levels.rxiflsel > 4)
        return HAL_UART_ERR_INVALID_ARG;

    if (drv_uart_write_UARTIFLS(uart, levels) != 0)
        return HAL_UART_ERR_NOT_INITIALIZED;

    return HAL_UART_STATUS_OK;
}
