#include "application.h"

/* Empty marker for the Renode symbol hook (see renode/renode.resc). Call it
 * with a label and an address; the hook reads the label string out of R0,
 * the address out of R1, and prints "<label> = 0x<value at address>". This
 * is how we inspect both hardware registers and status globals on a system
 * with no printf/semihosting. */
void checkpoint(const char *name, uint32_t address)
{
    (void)name;
    (void)address;
}

/* Another empty marker. The .resc hooks this symbol and, when the guest
 * reaches it, injects a byte into the UART's RX FIFO from the host side
 * (Renode's PL011 has no LBE loopback, so we feed RX directly). The read
 * right after this call then pops that byte back out. */
void inject_point(void) {}

/* Results parked in globals so checkpoint() can surface them. Each expected
 * value is noted at its checkpoint below; HAL_UART_STATUS_OK is 0 and
 * HAL_UART_ERR_NOT_INITIALIZED is 6. */
volatile UART_status g_open_status;
volatile UART_status g_send_status;
volatile UART_status g_num_status;
volatile UART_status g_int_status;
volatile UART_status g_pending_status;
volatile UART_status g_unopened_status;
volatile UART_status g_read_status;
volatile uint32_t g_read_byte; /* uint32_t so the checkpoint reads it cleanly */
volatile uart_int_t g_pending; /* masked interrupt status (MIS) read back */

int main(void)
{
    /* --- 1. bring-up through the application layer --- */
    g_open_status = App_UART_Open(UART0, 9600);
    checkpoint("open_status", (uint32_t)&g_open_status); /* expect 0 (OK) */

    /* Verify what actually landed in hardware, hand-computed:
     *   UARTCR    0x09000030 -> 0x301  (UARTEN|TXE|RXE = bit0|bit8|bit9)
     *   UARTLCR_H 0x0900002C -> 0x72   (PEN|FEN|WLEN=3 = bit1|bit4|(3<<5))
     *   UARTIBRD  0x09000024 -> 156    (24MHz / (16*9600) = 156.25 -> int part)
     *   UARTFBRD  0x09000028 -> 16     (0.25 * 64, rounded) */
    checkpoint("UARTCR", 0x09000030);
    checkpoint("UARTLCR_H", 0x0900002C);
    checkpoint("UARTIBRD", 0x09000024);
    checkpoint("UARTFBRD", 0x09000028);

    /* --- 2. send a line: exercises the SendString loop AND the \n->\r\n
     * translation (nothing has tested that path before). Watch the Renode
     * UART console for "Hello, PL011!" followed by CR+LF. --- */
    g_send_status = App_UART_SendString(UART0, "AY yooooooooooooo wsss uppppppp!\n");
    checkpoint("send_status", (uint32_t)&g_send_status); /* expect 0 (OK) */

    /* --- 3. decimal number out with no printf --- */
    g_num_status = App_UART_SendUInt(UART0, 12345);
    (void)App_UART_SendString(UART0, "\n");
    checkpoint("num_status", (uint32_t)&g_num_status); /* expect 0; "12345" on console */

    /* --- 4. first live use of the interrupt register layer --- */
    uart_int_t sources = {0};
    sources.rx = 1; /* RX FIFO reached watermark        (bit 4, 0x10) */
    sources.tx = 1; /* TX FIFO has room                 (bit 5, 0x20) */
    sources.rt = 1; /* receive timeout                  (bit 6, 0x40) */
    g_int_status = HAL_UART_enable_interrupts(UART0, sources);
    checkpoint("int_status", (uint32_t)&g_int_status); /* expect 0 (OK) */
    checkpoint("UARTIMSC", 0x09000038);                /* -> 0x70 (rx|tx|rt) */

    /* Read back which *enabled* interrupts are actually asserted (MIS).
     * Nothing has been received, so RX/RT read 0; TX may read set because
     * the TX FIFO is empty. Copy through a local so we don't cast away the
     * global's volatile. */
    uart_int_t pending = {0};
    g_pending_status = HAL_UART_get_pending(UART0, &pending);
    g_pending = pending;
    checkpoint("pending_status", (uint32_t)&g_pending_status); /* expect 0 (OK) */
    checkpoint("UARTMIS", (uint32_t)&g_pending);               /* masked status snapshot */
    checkpoint("UARTRIS", 0x0900003C);                         /* raw status, for comparison */

    /* --- 5. receive path: inject_point() is hooked in the .resc to push a
     * byte into the RX FIFO. The bounded read then pops it straight back
     * out - this is the first time HAL_UART_read_timeout and the DR
     * error-decode path actually run. Bounded (not blocking) so that if the
     * injection ever fails, we see a TIMEOUT instead of a hang. --- */
    inject_point();
    uint8_t rx = 0;
    g_read_status = HAL_UART_read_timeout(UART0, &rx, 1000000u);
    g_read_byte = rx;
    checkpoint("read_status", (uint32_t)&g_read_status); /* expect 0 (OK) */
    checkpoint("read_byte", (uint32_t)&g_read_byte);     /* expect 0x41 ('A') */

    /* --- 6. negative test: UART1 was never opened, so the driver never
     * registered a base address for it. Must come back NOT_INITIALIZED,
     * not crash. --- */
    g_unopened_status = App_UART_SendString(UART1, "nope");
    checkpoint("unopened_status", (uint32_t)&g_unopened_status); /* expect 6 */

    /* --- 7. interactive echo. From here the tests are done; whatever arrives
     * on UART0 is read and echoed straight back. Attach a terminal to the
     * UART's socket (see renode/interactive.resc) and type - each character
     * should come back. This is the live check of the receive path. --- */
    App_UART_SendString(UART0, "\r\nEcho ready - type something:\r\n");
    while (1)
    {
        uint8_t c;
        if (HAL_UART_read(UART0, &c) == HAL_UART_STATUS_OK)
        {
            (void)HAL_UART_write(UART0, c);
            (void)HAL_UART_write(UART0, '\n');
        }
    }

    return 0; /* unreachable */
}
