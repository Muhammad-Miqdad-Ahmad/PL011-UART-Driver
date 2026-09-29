/* Backing store for the fake register banks declared in test_hw.h.
 * (test_hw.h is force-included, so the extern declaration and the base-address
 * macros are already in scope here.) */
#include <stdint.h>

uint32_t fake_uart_mem[4][0x400];
