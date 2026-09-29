# PL011 UART Driver (bare-metal ARM)

A bare-metal driver for ARM's PrimeCell **PL011 UART**, written from the
datasheet in C with no vendor HAL and no libc. It runs on QEMU's `virt`
machine or Renode (Cortex-A15), and the driver/HAL logic is unit-tested on the
host with GoogleTest.

## Layers

```
application.c   App_UART_Open / SendString / SendUInt / ReceiveChar
      │
HAL.c           init, baud-rate divisors, framing, FIFOs, interrupts,
      │         blocking + timeout reads, error decode
DRIVER.c        one typed read/write per register, instance lookup, NULL guards
      │
PL011_config.h  register map as bitfield unions, base addresses, clock
```

Each layer only calls the one below it. Nothing above `DRIVER.c` touches a
register address.

## Build & run

Needs `arm-none-eabi-gcc`, and `qemu-system-arm` or `renode`. The unit tests
also need `cmake` and a host C++ compiler.

```bash
make              # build/pl011.elf
make run          # run in QEMU
make renode       # run in Renode; checkpoint() hooks print register values
make interactive  # Renode with UART0 on TCP 3456 -> `telnet 127.0.0.1 3456` to echo
make test         # host unit tests (GoogleTest, fetched on first run)
```

`make renode` walks through bring-up, TX, number output, interrupt enables,
an injected RX byte, and a negative test on an unopened instance. It prints
each register or status value next to its hand-computed expectation (see
`src/main.c`).

## Host tests

`test/test_hw.h` is force-included ahead of `PL011_config.h` and points the
UART base addresses at a RAM buffer. That lets the real `HAL_UART_init` run on
the host, and the tests read back the programmed registers (LCR_H, IBRD/FBRD,
CR).

## Docs

- [`WRITING_A_DRIVER.md`](WRITING_A_DRIVER.md) — how to read an IP datasheet and structure a driver
- [`regs.md`](regs.md) — PL011 register notes
- [`renode.md`](renode.md) — getting a bare-metal project running in Renode from scratch
