#include "DRIVER.h"

static REGISTERS *uart_instances[UART_INSTANCE_COUNT] = {NULL, NULL, NULL, NULL};

/* NULL if instance is out of range or hasn't been registered yet. */
static REGISTERS *uart_get(UART_INSTANCE instance)
{
    if ((unsigned)instance >= (unsigned)UART_INSTANCE_COUNT)
        return NULL;
    return uart_instances[instance];
}

void drv_uart_reg_init(UART_INSTANCE instance)
{
    uintptr_t address;

    switch (instance)
    {
    case UART0:
        address = UART0_BASE;
        break;
    case UART1:
        address = UART1_BASE;
        break;
    case UART2:
        address = UART2_BASE;
        break;
    case UART3:
        address = UART3_BASE;
        break;
    default:
        return; /* invalid instance - nothing to register */
    }

    uart_instances[instance] = (REGISTERS *)address;
}

int drv_uart_write_UARTDR(UART_INSTANCE instance, uint8_t data)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;

    uart->UARTDR.raw = data; /* always a whole-value write - never assign .data alone */
    
    return 0;
}

int drv_uart_read_UARTDR(UART_INSTANCE instance, uart_dr_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTDR;
    return 0;
}

int drv_uart_write_UARTECR(UART_INSTANCE instance)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    uart->UARTRSR_UARTECR = 0xFF;
    return 0;
}

int drv_uart_read_UARTRSR(UART_INSTANCE instance, uint32_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTRSR_UARTECR;
    return 0;
}

int drv_uart_read_UARTFR(UART_INSTANCE instance, uart_fr_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTFR;
    return 0;
}

int drv_uart_write_UARTIBRD(UART_INSTANCE instance, uint16_t divisor)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    uart->UARTIBRD = divisor;
    return 0;
}

int drv_uart_read_UARTIBRD(UART_INSTANCE instance, uint16_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = (uint16_t)uart->UARTIBRD;
    return 0;
}

int drv_uart_write_UARTFBRD(UART_INSTANCE instance, uint8_t fraction)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    uart->UARTFBRD = fraction & 0x3Fu; /* register is only 6 bits wide, clamp before writing */
    return 0;
}

int drv_uart_read_UARTFBRD(UART_INSTANCE instance, uint8_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = (uint8_t)uart->UARTFBRD;
    return 0;
}

int drv_uart_write_UARTLCR_H(UART_INSTANCE instance, uart_lcrh_t value)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    uart->UARTLCR_H = value;
    return 0;
}

int drv_uart_read_UARTLCR_H(UART_INSTANCE instance, uart_lcrh_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTLCR_H;
    return 0;
}

int drv_uart_write_UARTCR(UART_INSTANCE instance, uart_cr_t value)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    uart->UARTCR = value;
    return 0;
}

int drv_uart_read_UARTCR(UART_INSTANCE instance, uart_cr_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTCR;
    return 0;
}

int drv_uart_write_UARTIFLS(UART_INSTANCE instance, uart_ifls_t value)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    uart->UARTIFLS = value;
    return 0;
}

int drv_uart_read_UARTIFLS(UART_INSTANCE instance, uart_ifls_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTIFLS;
    return 0;
}

int drv_uart_write_UARTIMSC(UART_INSTANCE instance, uart_int_t value)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    uart->UARTIMSC = value;
    return 0;
}

int drv_uart_read_UARTIMSC(UART_INSTANCE instance, uart_int_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTIMSC;
    return 0;
}

int drv_uart_read_UARTRIS(UART_INSTANCE instance, uart_int_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTRIS;
    return 0;
}

int drv_uart_read_UARTMIS(UART_INSTANCE instance, uart_int_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTMIS;
    return 0;
}

int drv_uart_write_UARTICR(UART_INSTANCE instance, uart_int_t value)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    uart->UARTICR = value.raw; /* UARTICR itself stays a plain uint32_t - value is
                                 * only ever a local, freshly-zeroed scratch value */
    return 0;
}

int drv_uart_write_UARTDMACR(UART_INSTANCE instance, uart_dmacr_t value)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    uart->UARTDMACR = value;
    return 0;
}

int drv_uart_read_UARTDMACR(UART_INSTANCE instance, uart_dmacr_t *out)
{
    REGISTERS *uart = uart_get(instance);
    if (!uart) return -1;
    *out = uart->UARTDMACR;
    return 0;
}
