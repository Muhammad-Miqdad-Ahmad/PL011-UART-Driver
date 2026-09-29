#pragma once
/*
 * Test-only "fake hardware".
 *
 * A block of ordinary host RAM that stands in for the PL011 register banks.
 * This header is FORCE-INCLUDED (see CMakeLists.txt: -include test_hw.h) ahead
 * of PL011_config.h, so these base-address macros are already defined when the
 * config's `#ifndef UARTx_BASE` guards run - and therefore win. The driver then
 * points at memory we own instead of the unmapped address 0x09000000, so
 * register reads/writes just hit this buffer rather than segfaulting.
 *
 * Caveat: this is dumb memory, not a device model. It faithfully records what
 * the driver writes (great for testing init/configuration), but it does not
 * update status flags on its own - so it cannot model dynamic behaviour like
 * "wait until RXFE clears". Use Renode for that.
 */
#include <stdint.h>

/* 4 instances x 4 KB. uint32_t (not uint8_t) so each bank is 4-byte aligned,
 * which is required to cast it to a REGISTERS*. */
extern uint32_t fake_uart_mem[4][0x400];

#define UART0_BASE ((uintptr_t)fake_uart_mem[0])
#define UART1_BASE ((uintptr_t)fake_uart_mem[1])
#define UART2_BASE ((uintptr_t)fake_uart_mem[2])
#define UART3_BASE ((uintptr_t)fake_uart_mem[3])
