# Writing a Bare-Metal Device Driver — A Guide for New Hires

You have never written a driver before. That is fine. This guide walks you
through the whole mental model, the layered architecture we use, and — most
importantly — **how to read the hardware documentation**, because 80% of
driver work is reading, not typing.

The running example is a **UART** (a serial port) built from ARM's **PL011**
peripheral, because that is the driver in this repo. But every principle here
applies to *any* memory-mapped peripheral: a timer, a GPIO controller, an SPI
block, an Ethernet MAC. Learn the method once and the specific peripheral
becomes a detail.

> **How to use this guide.** Do not read it like a novel. Wherever you see a
> **🧠 Brainstorm** box, *stop*, cover the text below it, and try to answer it
> yourself first — sketch a diagram, guess, write a sentence. The answer is
> usually right after, but the guessing is where the learning happens. A driver
> engineer's real skill is reasoning about hardware they've never seen from
> documents that never quite spell it out; these boxes are practice for exactly
> that. If your guess was wrong, that gap is the thing worth remembering.

---

## 1. What is a device driver, really?

> **🧠 Brainstorm.** The CPU fundamentally only knows two tricks: load a value
> from an address, and store a value to an address. Given *only* those two
> operations, how could it possibly control a separate UART chip — make it send
> a byte down a wire? Jot an idea before reading on.

On a bare-metal system (no operating system) a peripheral is a piece of
hardware sitting on the chip next to the CPU. You control it by **reading and
writing special memory addresses**. That is the entire trick, and it is called
**memory-mapped I/O (MMIO)**.

- A normal address like `0x40000000` points at RAM. Writing there stores a
  value; reading gives it back. Nothing else happens.
- A **peripheral register** address like `0x09000000` looks like memory to the
  CPU, but the write is wired to real logic. Writing a byte to the UART's data
  register does not "store" it — it **transmits it out of a wire**. Reading a
  status register does not return what you last wrote — it returns **what the
  hardware currently is**.

So a register is a mailbox between software and a hardware state machine. Your
driver's job is to poke the right mailboxes in the right order.

Two consequences you must internalize immediately:

1. **Registers are `volatile`.** The compiler must never assume a register's
   value is unchanged, never cache it in a CPU register, never delete a
   "redundant" read. The hardware changes it underneath you. Forgetting
   `volatile` produces code that works in debug and mysteriously breaks with
   optimization on.
2. **Reads and writes can have side effects.** Reading one register may pop a
   FIFO. Writing a bit may clear an interrupt. The same address may even be a
   *different physical register* on read versus write. You cannot know this by
   looking at the address — only the documentation tells you.

---

## 2. What is an IP — and how do you find where it lives?

Before you can drive the UART you have to answer a question beginners don't even
know to ask: *what exactly is this "PL011" thing, and how did it end up at
address `0x09000000`?* Answering it dissolves most of the confusion about the
documentation.

### 2.1 An IP is a reusable hardware block

**IP** stands for **Intellectual Property** — an **IP block** (or *IP core*) is
a pre-designed, reusable piece of hardware that a chip company **licenses** and
drops into their chip, the same way you pull in a software library instead of
writing it yourself. Your `PL011` UART is an ARM **PrimeCell** IP: ARM designed
the serial-port logic once, and dozens of unrelated chip vendors license that
exact block and integrate it into their own chips.

Two things follow from "it's a reusable block," and they matter enormously:

- **The register block is identical everywhere.** Every chip that licenses the
  PL011 has the *same* registers at the *same* offsets with the *same* bits.
  Learn PL011 once → you can drive it in any of those chips. That is why the
  peripheral manual is worth studying deeply.
- **The block's own manual cannot tell you its address.** ARM has no idea where
  a given vendor will place it — so the PL011 manual never mentions
  `0x09000000`. *Where* the block lives is decided by whoever built the chip,
  not by whoever designed the block. (Hold that thought; it is the whole of
  §4.1.)

> **🧠 Brainstorm.** If ARM ships the same UART design to 50 different chip
> makers, why would it be a *mistake* for ARM to print a fixed address like
> `0x09000000` in the PL011 manual? What breaks?

### 2.2 IPs hang off buses — the AMBA family

> **🧠 Brainstorm.** A chip has one CPU but a dozen blocks of wildly different
> speeds — fast DRAM, a slow UART, GPIO, timers. They can't each get their own
> dedicated wires to the CPU. Sketch a structure that lets the CPU reach all of
> them without the slow UART holding back fast memory.

A **SoC** (System-on-Chip) is the CPU, memory, and a pile of IP blocks all on
one die. They connect through **on-chip buses**. ARM's bus family is **AMBA**
(Advanced Microcontroller Bus Architecture), and three members come up
constantly:

| Bus | Character | Who lives here |
|---|---|---|
| **AXI** | high-bandwidth, pipelined | main memory, GPUs, fast masters |
| **AHB** | older high-performance bus | on-chip RAM, DMA, higher-speed peripherals |
| **APB** | simple, low-bandwidth, low-power | **most small peripherals: UART, GPIO, timers, I²C** |

The PL011 is an **APB** peripheral. The buses form a hierarchy: the CPU issues
accesses on a fast bus (AXI/AHB), and a **bridge** connects that down to the
slower APB where the little peripherals sit. So a write to your UART actually
travels:

```
CPU  →  AXI/AHB  →  (AXI-to-APB bridge)  →  APB  →  UART (PL011)
```

The bus fabric looks at the address, decodes which slave owns it, and routes the
access there. "The UART is *attached to* the APB, which is bridged off the main
bus" is what people mean by where an IP "lives."

### 2.3 Finding *where* it is attached: the memory map

That address decode the bus performs is described by the SoC's **memory map**
(a.k.a. address map): a table that says, in effect, "addresses
`0x0900_0000`–`0x0900_0FFF` belong to UART0." **That table is the answer to
"where is the IP attached."** You will find it, in roughly the order you'll meet
it in real life:

1. **The SoC / board TRM** — an address-map table listing every peripheral and
   its base.
2. **The device tree** (`.dts` / `.dtb`) — used by Linux and by QEMU. Each
   peripheral is a node carrying a `compatible` string (e.g. `"arm,pl011"`) and
   a `reg = <base size>`. Grep the device tree for the IP name and you have its
   address and size directly.
3. **The emulator's platform / machine model** — for us,
   [renode/renode_platform.repl](renode/renode_platform.repl) literally says
   `uart0: UART.PL011 @ sysbus 0x09000000`. That one line *is* the memory map of
   our virtual board.
4. **QEMU's `virt` board** builds its device tree at runtime — dump it with
   `-machine dumpdtb=virt.dtb` and decompile it, or read `hw/arm/virt.c` in
   QEMU's source, to see the same map.

So two *different* documents answer two *different* questions, and conflating
them is the classic beginner mistake:

- **The peripheral (PL011) manual → *what* the registers are** (offsets, bits).
- **The board's memory map (TRM / device tree / platform file) → *where* the
  register block is attached** (the base address).

---

## 3. Before any code: get the documents

You cannot write a driver from imagination. You need the manuals — and now you
know there is more than one, each answering a different question:

| Document | Answers |
|---|---|
| **Peripheral TRM** (e.g. "PrimeCell UART (PL011) TRM") | What are this IP's registers, bits, and programming sequences? (offsets, not the base) |
| **SoC / board TRM + memory map** | Where is each IP attached in the address space? What bus? What input clock? |
| **Device tree / emulator platform file** | The concrete memory map for *this* board — base address and size per peripheral |
| **Board / schematic docs** | Which physical pins, clocks, voltages? What is the UART's input clock frequency (you need it for baud math)? |
| **CPU architecture manual (ARM ARM)** | Exceptions, interrupts (GIC), the vector table, CPSR — needed once you go interrupt-driven |

> **Rule of thumb:** the *peripheral* manual gives you everything **from the
> base onward** (offsets and bits); the *board's memory map* gives you the
> **base** itself. You always need both, and they are never the same document.

---

## 4. How to read ARM documentation

This is the skill that matters most, so we go slowly. There are four things you
extract, in order.

### 4.1 Finding the base address (it is usually *not* in the peripheral manual)

> **🧠 Brainstorm.** You open the PL011 manual to find the UART's address and…
> it isn't there. `0x09000000` appears nowhere in it. Before reading on: given
> §2, why is that *expected* rather than a documentation bug — and where should
> you look instead? List every place a board could record "the UART lives here."

Here is the beginner trap: you search the peripheral manual for the address and
never find it. **That is correct behaviour.** The PL011 TRM describes every
register as an **offset** — `0x000`, `0x004`, `0x018`, … — from an *unspecified*
base, because (§2.1) ARM cannot know where an integrator will map the block.

The base is fixed by whoever integrated the IP, and you read it from the
**memory map** (§2.3): the SoC TRM's address table, the device tree, or — for
us — the emulator platform file. For this project the base `0x09000000` comes
from QEMU's `virt` memory map, mirrored in
[renode/renode_platform.repl](renode/renode_platform.repl). It did **not** come
from the PL011 manual, and you should not expect it to.

Once you have it, write it as a single named constant — it is the one number
most likely to change when you move to a different chip, so it lives in exactly
one place, down in the driver layer:

```c
#define UART0_BASE 0x09000000UL
```

Every register you care about is then `base + offset`, and the offsets come from
the peripheral manual (next).

### 4.2 The register map → offsets and access types

The peripheral TRM has a **register summary table**. This is your treasure map.
It looks like:

```
Offset  Name             Type   Reset       Description
0x000   UARTDR           RW     0x---       Data register
0x004   UARTRSR/UARTECR  RW     0x0         Receive status / error clear
0x018   UARTFR           RO     0x-9-       Flag register
0x024   UARTIBRD         RW     0x0         Integer baud rate divisor
0x028   UARTFBRD         RW     0x0         Fractional baud rate divisor
0x02C   UARTLCR_H        RW     0x0         Line control
0x030   UARTCR           RW     0x300       Control
0x038   UARTIMSC         RW     0x0         Interrupt mask set/clear
0x03C   UARTRIS          RO     0x-         Raw interrupt status
0x040   UARTMIS          RO     0x-         Masked interrupt status
0x044   UARTICR          WO     -           Interrupt clear
...
```

Read every column deliberately:

- **Offset** — the register's address is `base + offset`. Offsets have gaps
  (e.g. `0x008`–`0x014` here are reserved); you must reproduce those gaps
  exactly in your C layout (see §6), or every register after the gap will be
  at the wrong address.
- **Type / Access** — this is critical and easy to skim past:
  - **RO** (read-only): reading is fine; writing does nothing (or is illegal).
  - **WO** (write-only): reading back is *undefined* — do not read it to
    modify it. `UARTICR` is write-only.
  - **RW** (read/write): normal.
  - **W1C** (write-1-to-clear): writing a `1` clears that bit; writing `0`
    leaves it alone. Common on interrupt/status registers.
  - **Read-clears / read-pops**: reading has a side effect. `UARTDR` pops a
    byte off the receive FIFO when read.
- **Reset value** — what the register holds after power-on. Knowing this lets
  you reason about the starting state and, later, verify your init actually
  changed things.

> **The single most important habit:** before you touch a register, know its
> access type. The RMW pattern "read, flip one bit, write back" is *correct*
> for an RW register and *a bug* for a WO register or one whose read has a side
> effect.

### 4.3 A single register's bit-assignment table

Each register gets its own page with a bit table. Here is how to read one. Take
`UARTLCR_H` (line control), which configures the data format:

```
Bit   Name   Description
7     SPS    Stick parity select
6:5   WLEN   Word length: 00=5, 01=6, 10=7, 11=8 data bits
4     FEN    Enable FIFOs
3     STP2   Two stop bits select
2     EPS    Even parity select (0 = odd, 1 = even)
1     PEN    Parity enable
0     BRK    Send break
31:8  -      Reserved
```

> **🧠 Brainstorm.** You want "8 data bits, parity on, FIFOs on." Using only the
> table above, work out the exact 32-bit value you'd write to `UARTLCR_H`
> *before* you read the next paragraph. (Hint: which bits, and what does the
> 2-bit `WLEN` field have to be?)

What you extract from this page:

- **Which bits do what.** `PEN` at bit 1 enables parity; `WLEN` at bits 6:5 is
  a *2-bit field*, not a flag — its four values mean four word lengths.
- **Multi-bit fields have an encoding table.** `WLEN = 0b11` means 8 data bits.
  You must look this up; you cannot guess.
- **Reserved bits.** Bits 31:8 are reserved. The rule for reserved bits is
  almost always **"write 0, ignore on read,"** and the safest way to honor that
  is to always start from a zeroed value and only set the fields you mean —
  which is exactly why our init does `uart_lcrh_t format = {0};` first.

You did this reading yourself for `UARTIMSC`, and you noticed the docs say it
"sets the mask," not "enables the interrupt." That is *exactly* the kind of
close reading this job rewards — see §4.4.

### 4.4 The operational sections: relationships and sequences

Bit tables tell you *what* each bit is. The prose sections tell you *how they
interact* and *in what order to touch them*. These are the sections beginners
skip and then spend two days debugging. Two examples from the UART:

**Relationships between registers.** The interrupt logic is defined by prose,
not by any single bit table:

```
MIS = RIS & IMSC
```

- `RIS` (raw status) tracks the interrupt condition **always**.
- `IMSC` is the mask/gate.
- `MIS` (masked status) is what actually drives the interrupt line to the CPU.

This is why setting an `IMSC` bit to 1 — which the docs pedantically call
"setting the mask" — *enables* the interrupt: with the gate open, `MIS` follows
`RIS` out to the interrupt controller. The bit table alone would never tell you
that; the AND relationship does.

**Programming sequences.** The TRM usually spells out the required order for
bring-up, e.g. *"disable the UART, wait for the current character to finish,
flush the FIFO, program the control registers, then enable."* Follow that order
literally. Line-control and baud-rate registers in particular often must be
written **while the UART is disabled**; changing them mid-transmission is
undefined. When the docs give you a numbered sequence, your init function
should read like that numbered sequence.

---

## 5. The three-layer architecture

> **🧠 Brainstorm.** Imagine you'll later port this driver to a new chip where
> the same PL011 sits at a *different* address, and then reuse the same UART to
> drive a GPS module instead of a debug console. Which parts of your code should
> be forced to change in each case, and which should stay untouched? Draw the
> boundaries you'd want *before* you read ours below — then compare.

Now that you can read the hardware, structure the software. We split every
driver into three layers. The point of layering is **separation of concerns**:
each layer knows one thing and hides it from the layers above.

```
+-------------------------------------------------------------+
|  Application layer     "send this line of text"             |
|    - business logic, no idea registers exist                |
+-------------------------------------------------------------+
|  HAL (Hardware Abstraction Layer)  "write one byte, 8N1"    |
|    - operations in terms of the DEVICE                      |
|    - init sequences, error decoding, config policy          |
+-------------------------------------------------------------+
|  Driver layer          "store this value at UARTDR"         |
|    - register access ONLY: addresses + bit layout           |
|    - no policy, no sequences, as thin as possible           |
+-------------------------------------------------------------+
|  Hardware              memory-mapped registers @ base       |
+-------------------------------------------------------------+
```

Data flows **down** through calls and **up** through return values. A layer may
only call the layer directly beneath it. (Notice how the brainstorm maps onto
this: a new *address* only disturbs the driver layer; a new *use* only disturbs
the application layer. That is the whole payoff of drawing the lines here.)

### 5.1 Driver layer — the register plumbing

**Knows:** the base addresses, the register map, the bit layout of each
register. **Does:** read and write registers. **Nothing else.**

- One or two functions per register (`read_UARTFR`, `write_UARTDR`).
- No decisions, no loops waiting for hardware, no "if error then...". It is the
  thinnest possible skin over MMIO.
- It is the *only* layer that knows a hardware address exists. If the peripheral
  moves to a new address on a new chip, this is the only layer that changes.
- Returns raw values (or a trivial success/failure) up to the HAL.

In this repo the driver layer is `DRIVER.c` / `DRIVER.h`, with functions like
`drv_uart_write_UARTDR(instance, byte)` that do exactly one store and return
`0`/`-1` depending only on whether the instance was registered.

### 5.2 HAL — the device abstraction

**Knows:** how to combine registers into meaningful operations. **Does:** the
programming sequences, the polling, the error decoding, the configuration
policy. **Speaks the language of the device, not of registers.**

- `HAL_UART_init(instance, baud)` runs the whole bring-up sequence from §4.4:
  disable, wait-not-busy, clear errors, program format, set baud, enable.
- `HAL_UART_write(instance, byte)` polls the "TX full" flag until there is
  room, then hands the byte to the driver.
- `HAL_UART_read(...)` pulls a byte and **decodes** the framing/parity/break/
  overrun error bits into a clean status enum, so callers never see raw bits.
- It is **portable**: any board using this same peripheral can reuse the HAL
  unchanged, because the board-specific addresses live one layer down.

A good test for "does this belong in the HAL?": *does it involve more than one
register, a sequence, a wait, or a decision?* If yes, HAL. If it is a single
naked register access, driver.

### 5.3 Application layer — the business logic

**Knows:** what the product needs to do. **Does:** calls the HAL. **Never**
mentions a register, a bit, or an address.

- `App_UART_SendString(instance, "hello\n")` loops over characters calling
  `HAL_UART_write`, and adds product-level policy like translating `\n` into
  `\r\n` for a terminal.
- `App_UART_ReceiveLine(...)` reads until a newline into a buffer.

If you find yourself writing `0x30` or `UARTCR` in the application layer, stop —
that logic belongs lower down.

### 5.4 Where does X go? A quick decision table

Try to place each row yourself before checking the right column.

| You are writing... | Layer |
|---|---|
| The address `0x09000000` | Driver (and nowhere else) |
| "Set bit 1 of UARTLCR_H" | Driver |
| "Enable parity" (which happens to be bit 1) | HAL |
| "Wait until the TX FIFO has room, then send" | HAL |
| "Framing error → return an error code" | HAL |
| "Send this whole string, `\n` becomes `\r\n`" | Application |
| "Prompt the user and read a line" | Application |

---

## 6. Representing registers in C

The idiom is to describe the register block as a `struct` and overlay it on the
base address. Because the fields are laid out at fixed offsets, the compiler
computes `base + offset` for you when you write `uart->UARTLCR_H`.

```c
typedef struct {
    volatile uint32_t UARTDR;          /* 0x000 */
    volatile uint32_t UARTRSR_UARTECR; /* 0x004 */
    uint32_t RESERVED_1[4];            /* 0x008 - 0x014  <-- the gap! */
    volatile const uint32_t UARTFR;    /* 0x018  RO -> const */
    ...
} REGISTERS;

REGISTERS *uart = (REGISTERS *)UART0_BASE;
```

> **🧠 Brainstorm.** The register map jumps from `UARTRSR_UARTECR` at `0x004`
> straight to `UARTFR` at `0x018`. If you *forgot* the reserved gap and wrote
> the two fields back-to-back, what address would your code compute for
> `UARTFR`, and what would happen at run time?

Points that trip up beginners:

- **The reserved gap is not optional.** If the map skips `0x008`–`0x014`, you
  must reserve those 16 bytes (`uint32_t RESERVED_1[4]`), or `UARTFR` lands at
  the wrong address and nothing works.
- **`volatile` on every register** (see §1).
- **`const` on read-only registers** lets the compiler catch an accidental
  write at compile time.

### 6.1 Named bit-fields with unions

Rather than `reg |= (1 << 1)` with magic bit numbers everywhere, describe each
register's bits once as a union of a bit-field struct and a raw word:

```c
typedef union {
    struct {
        uint32_t brk  : 1;   /* bit 0 */
        uint32_t pen  : 1;   /* bit 1 */
        uint32_t eps  : 1;   /* bit 2 */
        uint32_t stp2 : 1;   /* bit 3 */
        uint32_t fen  : 1;   /* bit 4 */
        uint32_t wlen : 2;   /* bits 6:5 */
        uint32_t sps  : 1;   /* bit 7 */
        uint32_t reserved : 24;
    };
    uint32_t raw;
} uart_lcrh_t;
```

Now callers write `format.pen = 1; format.wlen = 3;` — self-documenting, and
the magic numbers live in exactly one place (this definition). The `raw` member
lets you still get at the whole 32-bit value when you need it.

### 6.2 When a union is safe — and when it is a trap

This is the subtle lesson. A union laid directly over a live register is only
safe when **reading the register has no side effect AND read semantics match
write semantics.** Check the access type from §4.2 first:

- **Safe** (`UARTLCR_H`, `UARTCR`, `UARTFR`, `IMSC`, ...): plain RW or RO
  registers. Read gives you what is there; write puts it back. Use the union
  directly.
- **Trap — read has a side effect** (`UARTDR`): reading pops the RX FIFO. You
  cannot do "read-modify-write" on it. Keep it accessed explicitly, one read or
  one write at a time.
- **Trap — write-only** (`UARTICR`): reading back is undefined, so never RMW it.
  Build the value in a **local scratch union**, then do a single whole-value
  write.
- **Trap — same address is two registers** (`UARTRSR/UARTECR`): read and write
  hit different physical registers. A union pretending it is one register would
  be lying.

### 6.3 Whole-value writes vs. field-by-field writes

Writing fields one at a time to a live register is not just slower — it is a
correctness hazard:

```c
uart->UARTLCR_H.pen  = 1;   /* load, set bit, store */
uart->UARTLCR_H.wlen = 3;   /* load, set bits, store  <-- a SECOND store */
```

Each field assignment to a `volatile` register compiles to its own
read-modify-write **store**, so the register briefly holds half-configured
intermediate states, and the hardware sees several transient writes. Instead,
build the value in a local, then store once:

```c
uart_lcrh_t format = {0};
format.pen = 1;
format.wlen = 3;
uart->UARTLCR_H = format;   /* exactly ONE store of the final value */
```

Disassemble both versions once (`arm-none-eabi-objdump -d`) and you will see
the difference in stores with your own eyes. It is worth doing.

---

## 7. The method, end to end

Putting it together, here is the sequence a new hire follows to bring up any
peripheral from scratch:

1. **Identify the IP and its bus** (§2): what block is this, what does its
   peripheral manual cover, what bus is it on?
2. **Find the base address** from the board memory map — *not* the peripheral
   manual (§4.1) → `#define XXX_BASE`.
3. **Transcribe the register map** into a `volatile` struct, reserved gaps and
   all (§6). Mark RO registers `const`.
4. **Transcribe each register's bits** into a union (§6.1), checking access
   type as you go to decide which registers are union-safe (§6.2).
5. **Write the driver layer**: one thin read/write per register, no policy.
6. **Write the HAL init** by translating the TRM's programming sequence
   (§4.4) step for step: disable → wait → clear → format → baud → enable.
7. **Write the HAL operations**: `write`, `read` (with error decoding), status
   queries, config setters.
8. **Decide an error strategy** and apply it consistently: the driver returns a
   trivial ok/fail; the HAL returns a rich status enum; the application checks
   that enum.
9. **Write the application API** in the product's language, calling only the HAL.

Do the reading (steps 1–4) thoroughly and the coding (steps 5–9) is almost
mechanical.

---

## 8. Testing without hardware

You usually do not have the chip on your desk on day one. Emulators
(**QEMU**, **Renode**) model the peripheral so you can run your ELF and inspect
registers.

- Build a small `main` that exercises init and a write, then **verify the real
  register values against numbers you computed by hand** — e.g. for 9600 baud
  at a 24 MHz clock, the integer divisor is `24_000_000 / (16 * 9600) = 156`,
  so `UARTIBRD` must read `156` after init. If it does, your baud math and your
  register layout are both right.
- A useful trick when you have no `printf`: an empty `checkpoint(addr)` function
  plus a debugger/emulator symbol hook that prints the value at `addr` whenever
  execution reaches it. You can point it at a hardware register *or* at a global
  variable holding a status code, and read both the same way.
- **Emulators are not the hardware.** They implement a *subset* of the
  peripheral. For instance, the PL011 models in both QEMU and Renode do **not**
  implement the loopback (`LBE`) bit — a self-test that relies on it will hang
  forever, and that is an emulator gap, not a bug in your driver. When
  something that "must" work does not, ask whether the model even implements it
  before you doubt your code. Design paths that could hang (like a blocking
  read) with a **timeout** so a missing feature fails cleanly instead of
  locking up.

> **🧠 Brainstorm.** Loopback (`LBE`) doesn't work in the emulator, so you can't
> test the receive path by wiring TX back to RX. But *something* else could put
> a byte into the RX FIFO for you to read. What, and how would you trigger it
> from outside the guest? (We solved this by injecting a byte from the emulator
> monitor — but try to invent it before peeking at the commit.)

---

## 9. Common pitfalls (the greatest hits)

Every one of these has bitten a real person on this exact driver:

- **Missing `volatile`** → code works unoptimized, breaks with `-O2` as the
  compiler caches or elides register accesses.
- **RMW on a side-effect register** → reading `UARTDR` to "modify" it silently
  eats a received byte.
- **Compound literal only sets the first field.** `(uart_int_t){1}` sets only
  the first bit-field member to 1, *not* every field. To set all 11 interrupt
  bits, write `x.raw = 0x7FF;`, not `(uart_int_t){1}`.
- **Field-by-field writes to a live register** → multiple transient stores
  (§6.3).
- **Inverted busy-wait.** `while (!is_busy());` on a device that starts *not*
  busy loops forever. Get the polarity right and think about the initial state.
- **Building a config value but never writing it.** Filling in a local
  `uart_cr_t` and forgetting the final store to the register is a silent no-op.
- **Reconfiguring while enabled.** Writing baud/line-control while the UART is
  running is undefined; disable first (§4.4).
- **Confusing the two manuals.** Hunting for the base address in the peripheral
  manual, or expecting register bits in the board memory map (§2.3).
- **Magic numbers leaking upward.** An address or bit number in the HAL or app
  layer means something is in the wrong layer.

---

## 10. Glossary

- **Bare-metal** — running with no operating system; your code is everything.
- **IP / IP core** — a reusable, licensed hardware block (e.g. the PL011)
  integrated into many different chips.
- **PrimeCell** — ARM's brand for its AMBA-compliant peripheral IP blocks.
- **SoC** — System-on-Chip; CPU, memory, and many IP blocks on one die.
- **AMBA** — ARM's on-chip bus family. Members: **AXI** / **AHB** (high
  performance) and **APB** (simple low-power peripheral bus, where UARTs live).
- **Bus bridge** — glue that connects one bus to another (e.g. AXI→APB).
- **Memory map / address map** — the table saying which address range belongs to
  which peripheral; *the* source of a base address.
- **Device tree** (`.dts`/`.dtb`) — a data description of a board's peripherals
  (each with a `compatible` string and `reg = <base size>`); a memory map you
  can grep.
- **MMIO** — memory-mapped I/O; controlling hardware by reading/writing
  addresses.
- **Register** — a hardware-backed address; the software/hardware mailbox.
- **TRM** — Technical Reference Manual, the peripheral's/chip's datasheet.
- **Base address** — where a peripheral's register block starts in the map;
  comes from the board, not the peripheral manual.
- **Offset** — a register's distance from the base; address = base + offset.
- **Access type** — RO / WO / RW / W1C; how a register may be touched.
- **W1C** — write-1-to-clear; writing 1 clears a bit, writing 0 leaves it.
- **FIFO** — a small hardware queue buffering bytes in/out.
- **UART / baud** — a serial port; baud is its bits-per-second line rate.
- **HAL** — Hardware Abstraction Layer; device operations, board-independent.
- **GIC** — ARM's Generic Interrupt Controller; routes peripheral interrupts to
  the CPU (needed for interrupt-driven, as opposed to polled, drivers).
- **`volatile`** — tells the compiler a value can change outside the program's
  control; mandatory on registers.

---

### The one-sentence version

**An IP block's manual tells you *what* its registers are; the board's memory
map tells you *where* they are — read both until you can predict every value,
mirror the register map into a `volatile` struct, keep addresses in the driver
layer, device logic in the HAL, product logic in the application, and verify
against hand-computed numbers before you trust it.**
