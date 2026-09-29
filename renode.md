# Testing a Bare-Metal Driver with Renode — From Scratch

For someone who has never used Renode, has a driver's `.c`/`.h` files
already written, and has **no linker script, no startup code, and no
Makefile yet**. This walks through everything needed to get from "I have
driver source" to "I can watch it run and inspect real register values,"
using generic placeholders — swap in your own peripheral, addresses, and
file names throughout.

## 0. What you need before starting

- A driver that reads/writes some memory-mapped peripheral — plain `.c`/`.h`
  files, nothing fancy required.
- Know your target: which ARM core (this guide assumes Cortex-M3, adjust
  `-mcpu=` if yours differs), and the base address(es) of the peripheral(s)
  your driver touches.

## 1. Install the tools

```bash
sudo apt-get install -y gcc-arm-none-eabi          # cross-compiler
wget https://builds.renode.io/renode-latest.deb
sudo apt-get install -y ./renode-latest.deb         # simulator
```

Check both:
```bash
arm-none-eabi-gcc --version
renode --version
```

One hard rule: **keep your project path free of spaces.** Renode's command
parser breaks on spaces in file paths (quoting doesn't fix it), so
`my project/` will cause confusing errors later — use `my-project/` instead.

## 2. Understand what "running on real hardware" actually requires

Your driver's `.c` files alone can't run on a Cortex-M chip (or its
simulator) the way they'd run on your Linux machine. Three things have to
exist first, none of which your driver provides:

1. **A vector table.** On reset, the CPU reads two fixed 32-bit values from
   address `0x00000000` and `0x00000004`: the initial stack pointer, and the
   address of the first function to run (conventionally `Reset_Handler`).
   This has to be a real array placed at a known address — nothing runs
   before the CPU reads it.
2. **A linker script.** Something has to tell the linker where your code
   goes (flash/ROM), where your variables go (RAM), and where the stack is
   — the CPU and linker have no idea otherwise.
3. **Startup code** (`Reset_Handler`). Before your `main()` can safely run,
   something has to copy initialized globals from flash into RAM and zero
   out uninitialized ones — bare metal gives you no guarantee this already
   happened, unlike a hosted OS.

None of this is specific to your driver's logic — it's the same boilerplate
for any bare-metal Cortex-M program. Write it once.

## 3. Write a linker script

Save as `linker.ld`. Adjust the memory addresses/sizes to match your real
target (these two — flash at `0`, RAM at `0x20000000` — are the standard
Cortex-M defaults and correct for many chips, but check your target's
datasheet):

```
ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00000000, LENGTH = 4M
    RAM   (rwx) : ORIGIN = 0x20000000, LENGTH = 4M
}

SECTIONS
{
    .isr_vector :
    {
        KEEP(*(.isr_vector))
    } > FLASH

    .text :
    {
        *(.text*)
        *(.rodata*)
    } > FLASH

    .data :
    {
        _sdata = .;
        *(.data*)
        _edata = .;
    } > RAM AT > FLASH

    _sidata = LOADADDR(.data);

    .bss :
    {
        _sbss = .;
        *(.bss*)
        *(COMMON)
        _ebss = .;
    } > RAM

    . = ALIGN(8);
    _estack = ORIGIN(RAM) + LENGTH(RAM);
}
```

## 4. Write startup code

Save as `startup.c`. This is generic boilerplate — the vector table, and a
`Reset_Handler` that does the copy/zero step then calls your `main()`:

```c
#include <stdint.h>

extern uint32_t _estack;
extern uint32_t _sdata, _edata, _sidata, _sbss, _ebss;

int main(void);
void Reset_Handler(void);
void Default_Handler(void);

typedef void (*isr_handler_t)(void);

__attribute__((section(".isr_vector"), used))
const isr_handler_t vector_table[16] = {
    (isr_handler_t)&_estack, // 0: initial stack pointer
    Reset_Handler,           // 1: Reset
    Default_Handler,         // 2: NMI
    Default_Handler,         // 3: HardFault
    Default_Handler,         // 4: MemManage
    Default_Handler,         // 5: BusFault
    Default_Handler,         // 6: UsageFault
    0, 0, 0, 0,              // 7-10: Reserved
    Default_Handler,         // 11: SVCall
    Default_Handler,         // 12: DebugMon
    0,                       // 13: Reserved
    Default_Handler,         // 14: PendSV
    Default_Handler          // 15: SysTick
};

void Reset_Handler(void)
{
    uint32_t *src = &_sidata;
    uint32_t *dst = &_sdata;
    while (dst < &_edata) *dst++ = *src++;

    dst = &_sbss;
    while (dst < &_ebss) *dst++ = 0;

    main();
    while (1) { }   // main() has nowhere to return to on bare metal
}

void Default_Handler(void)
{
    while (1) { }
}
```

## 5. Compile it — no Makefile needed yet

Compile every source file, then link them together with your linker script:

```bash
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Wall -Wextra -ffreestanding -nostartfiles -g -c startup.c -o startup.o
arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Wall -Wextra -ffreestanding -nostartfiles -g -c your_driver.c -o your_driver.o
# ... repeat -c for every other .c file in your driver ...

arm-none-eabi-gcc -mcpu=cortex-m3 -mthumb -Wall -Wextra -ffreestanding -nostartfiles -g \
    -T linker.ld -o firmware.elf startup.o your_driver.o
```

Flag meanings, briefly: `-mcpu=cortex-m3 -mthumb` targets the right
instruction set; `-ffreestanding` tells the compiler there's no OS beneath
this code; `-nostartfiles` skips the compiler's normal C-runtime startup
(you're providing your own via `Reset_Handler`); `-T linker.ld` uses the
script from step 3.

If your driver calls `printf` and you want that to work, add
`-specs=rdimon.specs` to the final link command, and call
`initialise_monitor_handles()` (declared `extern void
initialise_monitor_handles(void);`) at the very start of `main()` before any
I/O — this is a real, easy-to-miss requirement: without it, `printf` calls
compile and run but produce no visible output. That said, semihosting
`printf` doesn't currently work under Renode the way it does under QEMU
(you'll see `Semihosting handler is not registered` warnings) — for testing
under Renode specifically, you don't need this at all; read register values
directly instead (step 8).

You should now have `firmware.elf`.

## 6. Describe your target hardware to Renode

Renode needs to know what CPU and what peripherals exist, at what
addresses — this is a **platform description**, a small text file with the
extension `.repl`. Save as `platform.repl`:

```
cpu: CPU.CortexM @ sysbus
    cpuType: "cortex-m3"
    nvic: nvic

nvic: IRQControllers.NVIC @ sysbus 0xE000E000
    IRQ -> cpu@0

flash: Memory.MappedMemory @ sysbus 0x00000000
    size: 0x00400000

ram: Memory.MappedMemory @ sysbus 0x20000000
    size: 0x00400000
```

That much is boilerplate for any Cortex-M3 target. Now add your actual
peripheral. Renode ships with real (not stub) models for a large number of
chips — check what's available before assuming you need to write one:

```bash
grep -rl "YourChipName" /opt/renode/platforms/          # search bundled examples
ls /opt/renode/platforms/                                 # or browse by vendor/board
```

If you find one matching your peripheral, instantiate it at its real base
address, e.g. (this example is Arm's CMSDK AHB GPIO — substitute your own):

```
myPeripheral: GPIOPort.ARM_AHB_GPIO @ sysbus 0x40010000
```

The syntax is always `name: Namespace.TypeName @ sysbus <base_address>`. If
nothing in Renode matches your exact peripheral, the closest official
starting point is
[Renode's peripheral-modeling guide](https://renode.readthedocs.io/en/latest/advanced/writing-peripherals.html)
— out of scope for this guide, but worth knowing it's a documented,
supported path, not a dead end.

## 7. Write a script to load and run it

Save as `run.resc`:

```
mach create "test"
machine LoadPlatformDescription @platform.repl
sysbus LoadELF @firmware.elf
start
```

Run it:

```bash
renode --disable-gui --console -e "s @run.resc"
```

(`--console` matters — without it, Renode's interactive monitor opens on a
telnet port instead of your terminal, and it'll look like it's hung with no
prompt.)

You'll see a wall of setup log lines, then a `(test)` prompt — you're now
inside the simulator, and your firmware is running.

## 8. Actually test your driver

Two ways to check register behavior, depending on what you're trying to
prove:

**A. Poke the peripheral directly** (tests the hardware model itself,
bypasses your driver entirely):

```
sysbus ReadDoubleWord 0x40010010
sysbus WriteDoubleWord 0x40010010 0x7
```

**B. Watch your driver's own code run** (tests whether *your code* does the
right thing) — add an empty marker function to your driver source, call it
with whichever register address you want checked at that point, and hook it
by name from Renode:

```c
void checkpoint(uint32_t address) { (void)address; }
```

```c
your_driver_function_that_sets_a_register();
checkpoint(0x40010010);   // or better: a getter function that returns the address, if your driver has one
```

```
sysbus.cpu AddSymbolHook "checkpoint" "addr = self.GetRegister(0).RawValue; print('checkpoint(0x%x) = 0x%x' % (addr, machine.SystemBus.ReadDoubleWord(addr)))"
```

`AddSymbolHook` fires every time execution reaches that function, by name
— it survives rebuilds even though addresses shift, because it looks the
symbol up fresh each time. `self.GetRegister(0).RawValue` reads `R0`, which
is where a function's first argument lives per the ARM calling convention
— that's how the hook knows which address you asked to check.

## 9. Once this all works: wrap it in a Makefile (optional)

Everything above is plain shell commands specifically so you understand
what's actually happening. Once it does, a minimal Makefile is just those
same commands with dependency tracking:

```makefile
CC = arm-none-eabi-gcc
CFLAGS = -mcpu=cortex-m3 -mthumb -Wall -Wextra -ffreestanding -nostartfiles -g
SRCS = startup.c your_driver.c
OBJS = $(SRCS:.c=.o)

firmware.elf: $(OBJS)
	$(CC) $(CFLAGS) -T linker.ld -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

renode: firmware.elf
	renode --disable-gui --console -e "s @run.resc"

clean:
	rm -f $(OBJS) firmware.elf
```

`make renode` now rebuilds automatically (only) when a source file changed,
then runs it — but nothing about *how* it works changes from steps 1–8.

## Common mistakes

- **"I changed my code but the output didn't change"** — you ran `renode`
  directly instead of rebuilding first. Renode just loads whatever `.elf`
  already exists; it doesn't know your source changed.
- **"It just hangs, no prompt"** — missing `--console`.
- **Spaces in your project path** — Renode's `@path` syntax will fail with
  `Could not tokenize here`. Rename the directory.
- **A register always reads back `0` no matter what you write** — either
  you're looking at the wrong address, or the peripheral model you picked
  doesn't implement that specific register's behavior (some Renode models
  implement certain registers as inert placeholders — check the model's
  source if a register that should clearly be "live" never changes).
