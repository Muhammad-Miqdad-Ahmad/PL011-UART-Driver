#ifndef PL011_CONFIG_H
#define PL011_CONFIG_H

#include <stdint.h>
#include <stdio.h>

typedef enum
{
    UART0 = 0,
    UART1,
    UART2,
    UART3,
    UART_INSTANCE_COUNT
} UART_INSTANCE;

typedef union
{
    struct
    {
        uint32_t data : 8; /* bits 7:0  - the TX/RX byte itself */
        uint32_t fe : 1;   /* bit 8     - framing error on the byte just read (read-only) */
        uint32_t pe : 1;   /* bit 9     - parity error on the byte just read (read-only) */
        uint32_t be : 1;   /* bit 10    - break condition on the byte just read (read-only) */
        uint32_t oe : 1;   /* bit 11    - RX FIFO overrun, a byte was lost (read-only) */
        uint32_t reserved : 20;
    };
    uint32_t raw;
} uart_dr_t;

typedef union
{
    struct
    {
        uint32_t cts : 1;  /* bit 0 - modem CTS input, ignore unless doing flow control */
        uint32_t dsr : 1;  /* bit 1 - modem DSR input, ignore unless doing flow control */
        uint32_t dcd : 1;  /* bit 2 - modem DCD input, ignore unless doing flow control */
        uint32_t busy : 1; /* bit 3 - a bit is still shifting out on the wire */
        uint32_t rxfe : 1; /* bit 4 - RX FIFO empty: don't read UARTDR while this is set */
        uint32_t txff : 1; /* bit 5 - TX FIFO full: don't write UARTDR while this is set */
        uint32_t rxff : 1; /* bit 6 - RX FIFO completely full */
        uint32_t txfe : 1; /* bit 7 - TX FIFO completely empty */
        uint32_t ri : 1;   /* bit 8 - modem RI input, ignore unless doing flow control */
        uint32_t reserved : 23;
    };
    uint32_t raw;
} uart_fr_t;

typedef union
{
    struct
    {
        uint32_t brk : 1;  /* bit 0    - force a break condition; not for normal use */
        uint32_t pen : 1;  /* bit 1    - parity enable */
        uint32_t eps : 1;  /* bit 2    - even parity select (only matters if pen=1; 0=odd, 1=even) */
        uint32_t stp2 : 1; /* bit 3    - use 2 stop bits instead of 1 */
        uint32_t fen : 1;  /* bit 4    - enable the 16-byte TX/RX FIFOs */
        uint32_t wlen : 2; /* bits 6:5 - word length: 00=5,01=6,10=7,11=8 bits */
        uint32_t sps : 1;  /* bit 7    - stick parity, rare, unused */
        uint32_t reserved : 24;
    };
    uint32_t raw;
} uart_lcrh_t;

typedef union
{
    struct
    {
        uint32_t uarten : 1;    /* bit 0  - master enable; nothing works until this is 1 */
        uint32_t siren : 1;     /* bit 1  - IrDA SIR enable, unused */
        uint32_t sirlp : 1;     /* bit 2  - IrDA low-power mode, unused */
        uint32_t reserved1 : 4; /* bits 6:3 - reserved */
        uint32_t lbe : 1;       /* bit 7  - loopback enable: wires TX internally to RX */
        uint32_t txe : 1;       /* bit 8  - transmit enable */
        uint32_t rxe : 1;       /* bit 9  - receive enable */
        uint32_t dtr : 1;       /* bit 10 - modem DTR output, unused */
        uint32_t rts : 1;       /* bit 11 - modem RTS output, unused unless doing flow control */
        uint32_t out1 : 1;      /* bit 12 - modem Out1 output, unused */
        uint32_t out2 : 1;      /* bit 13 - modem Out2 output, unused */
        uint32_t rtsen : 1;     /* bit 14 - RTS hardware flow control enable, unused */
        uint32_t ctsen : 1;     /* bit 15 - CTS hardware flow control enable, unused */
        uint32_t reserved2 : 16;
    };
    uint32_t raw;
} uart_cr_t;

typedef union
{
    struct
    {
        uint32_t txiflsel : 3; /* bits 2:0 - TX FIFO interrupt watermark (0=1/8 ... 4=7/8) */
        uint32_t rxiflsel : 3; /* bits 5:3 - RX FIFO interrupt watermark (0=1/8 ... 4=7/8) */
        uint32_t reserved : 26;
    };
    uint32_t raw;
} uart_ifls_t;

typedef union
{
    struct
    {
        uint32_t ri : 1;  /* bit 0  - modem RI, unused */
        uint32_t cts : 1; /* bit 1  - modem CTS, unused unless doing flow control */
        uint32_t dcd : 1; /* bit 2  - modem DCD, unused */
        uint32_t dsr : 1; /* bit 3  - modem DSR, unused */
        uint32_t rx : 1;  /* bit 4  - RX FIFO reached its watermark / has data */
        uint32_t tx : 1;  /* bit 5  - TX FIFO reached its watermark / has room */
        uint32_t rt : 1;  /* bit 6  - receive timeout */
        uint32_t fe : 1;  /* bit 7  - framing error */
        uint32_t pe : 1;  /* bit 8  - parity error */
        uint32_t be : 1;  /* bit 9  - break error */
        uint32_t oe : 1;  /* bit 10 - overrun error */
        uint32_t reserved : 21;
    };
    uint32_t raw;
} uart_int_t; // The Union for the three regs UARTIMSC, RIS, MIS

typedef union
{
    struct
    {
        uint32_t rxdmae : 1;   /* bit 0 - use DMA requests instead of interrupts for RX */
        uint32_t txdmae : 1;   /* bit 1 - use DMA requests instead of interrupts for TX */
        uint32_t dmaonerr : 1; /* bit 2 - stop DMA if a receive error occurs */
        uint32_t reserved : 29;
    };
    uint32_t raw;
} uart_dmacr_t; // UARTDMACR

typedef struct
{
    volatile uart_dr_t UARTDR;         /* 0x000 */
    volatile uint32_t UARTRSR_UARTECR; /* 0x004 - UARTRSR on read, UARTECR on write */
    uint32_t RESERVED_1[4];            /* 0x008 - 0x014 */
    volatile const uart_fr_t UARTFR;   /* 0x018 */
    uint32_t RESERVED_2;               /* 0x01C */
    volatile uint32_t UARTILPR;        /* 0x020 - IrDA only, not used by this driver */
    volatile uint32_t UARTIBRD;        /* 0x024 */
    volatile uint32_t UARTFBRD;        /* 0x028 */
    volatile uart_lcrh_t UARTLCR_H;    /* 0x02C */
    volatile uart_cr_t UARTCR;         /* 0x030 */
    volatile uart_ifls_t UARTIFLS;     /* 0x034 */
    volatile uart_int_t UARTIMSC;      /* 0x038 */
    volatile const uart_int_t UARTRIS; /* 0x03C */
    volatile const uart_int_t UARTMIS; /* 0x040 */
    volatile uint32_t UARTICR;         /* 0x044 */
    volatile uart_dmacr_t UARTDMACR;   /* 0x048 */
} REGISTERS;

#ifndef UART0_BASE
#define UART0_BASE 0x09000000UL
#endif

#ifndef UART1_BASE
#define UART1_BASE 0x09000000UL
#endif

#ifndef UART2_BASE
#define UART2_BASE 0x09000000UL
#endif

#ifndef UART3_BASE
#define UART3_BASE 0x09000000UL
#endif

#ifndef UARTCLK_HZ
#define UARTCLK_HZ 24000000UL
#endif

#endif /* PL011_CONFIG_H */
