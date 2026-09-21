# Nebula OS

A small operating system for 32-bit x86, written from scratch in C and
assembly, built around one idea: **the computer should be continuously
programming itself for you.**

Every action you take is observable. The system watches what you repeat,
works out which parts of it stay the same and which vary, and after the
third repetition offers the whole thing back as a command you can run.
Accept it and you have a new program -- without having written software.

It boots from its own 512-byte boot sector on bare metal (or QEMU), runs
in protected mode with paging, preemptively schedules kernel threads, and
drops you into a shell that is paying attention.

```
nebula:/$ mkdir gamma
nebula:/$ cd gamma
nebula:/gamma$ cd ..

[auto] you have done this 3 times:
         mkdir gamma
         cd gamma
         cd ..
       i can make that a command taking 1 argument:
         mkdir $1
         cd $1
         cd ..
       `accept 2` to keep it as `mkdir-cd`, or `dismiss 2`

nebula:/$ accept 2 project
learned `project` -- run it as `project <arg>`. saved to /auto/project
nebula:/$ project delta
nebula:/$ ls
     <dir>  delta/
```

```
Nebula OS 0.1.0
a small 32-bit x86 kernel

[boot] loaded from drive 0x00
[gdt] flat descriptors loaded, TSS installed
[idt] 256 vectors installed, PIC remapped to 0x20-0x2f
[pmm] 127 MiB usable, 32736 frames (32480 free), bitmap at 0x0001f000
[paging] identity mapped 0-127 MiB, directory at 0x00100000
[heap] 68 KiB at 0xd0000000
[cpu] interrupts enabled
[sched] round-robin scheduler up, slice 5 ticks
[fs] in-memory filesystem mounted at / (6 nodes)

Welcome to Nebula OS. Type `help` to get started.

nebula:/$
```

There is no third-party code here: no bootloader, no libc, no driver
framework. The BIOS hands over 512 bytes and everything after that is in
this directory.

## Quick start

```sh
make            # build build/nebula.img, a 1.44 MiB bootable floppy image
make run        # boot it in QEMU with a graphical console
make run-text   # boot it headless, with the console on your terminal
make check      # run the full test suite
```

Requires `gcc` with 32-bit support (`gcc-multilib`), GNU `binutils`, and
`qemu-system-i386` to run. The kernel builds with the ordinary system
compiler -- no cross-toolchain needed.

With `make run-text` you can type at the shell directly: the serial port
is wired up as a console input, so the keyboard and the terminal feed the
same input queue.

## How the boot works

The BIOS reads one sector from the floppy to `0x7C00` and jumps to it in
16-bit real mode. From there, `boot/boot.S`:

1. Normalises the segment registers and sets up a stack.
2. Walks the BIOS `E820` memory map and stashes it at `0x5000`, because
   this information is only available before leaving real mode -- the
   kernel would have no way to discover it later.
3. Reads the kernel from disk to `0x10000`, one sector at a time with
   retries and a disk reset between attempts. It advances the segment
   register rather than the offset between sectors, which sidesteps the
   64 KiB wrap that bites naive multi-sector loaders.
4. Enables the A20 gate, so addresses above 1 MiB stop aliasing.
5. Installs a flat GDT, sets `CR0.PE`, and far-jumps into 32-bit code.

The number of sectors to read is not hardcoded: the Makefile builds the
kernel first, measures it, and assembles the boot sector with the real
count. A kernel that outgrows the loader is a build error, not a
mysterious crash.

`kernel/entry.S` then zeroes `.bss` and switches to a kernel-owned stack
before calling `kmain`.

## Layout

```
boot/boot.S       boot sector: real mode -> protected mode
kernel/entry.S    32-bit entry: zero .bss, set up the stack, call kmain
kernel/kmain.c    subsystem bring-up and boot-time self-tests

kernel/gdt.c      segment descriptors, ring-3 segments, TSS
kernel/idt.c      interrupt descriptor table and dispatch
kernel/isr.S      interrupt stubs that normalise the CPU's frames
kernel/pic.c      8259A interrupt controllers
kernel/switch.S   the context switch

kernel/pmm.c      physical frame allocator (bitmap over the E820 map)
kernel/paging.c   two-level paging
kernel/heap.c     kernel heap: first-fit, splitting and coalescing
kernel/sched.c    preemptive round-robin scheduler

kernel/vga.c      80x25 text console
kernel/serial.c   16550 UART on COM1, in and out
kernel/keyboard.c PS/2 keyboard
kernel/timer.c    programmable interval timer at 100 Hz
kernel/rtc.c      CMOS real-time clock
kernel/input.c    shared console input queue and line editor

kernel/journal.c  the action journal: what the user did
kernel/automate.c pattern mining, generalisation, proposals

kernel/fs.c       in-memory filesystem
kernel/shell.c    the interactive shell
kernel/string.c   freestanding string and memory routines
kernel/kprintf.c  formatted output and panic
```

## Memory

Physical memory comes from the BIOS `E820` map. Everything the allocator
manages is identity mapped, so a physical address is always usable as a
pointer inside the kernel -- which is what makes editing page tables
straightforward, since a table's physical address is also where you write
to it.

```
0x00000000 - 0x000FFFFF   reserved: IVT, BIOS data, boot sector, video memory
0x00010000 - ...          the kernel image
      ... - ...           the physical frame bitmap, right after the kernel
0x000B8000                VGA text buffer
0xD0000000 - 0xD1000000   kernel heap (mapped on demand, 16 MiB ceiling)
```

The heap deliberately lives above the identity-mapped region: it grows by
asking the frame allocator for pages and mapping them at the next free
virtual address, so it exercises the MMU rather than sitting in a
pre-reserved block. One of the boot self-tests asserts exactly this --
that a heap pointer resolves to a *different* physical address.

Frames above 1 GiB are left unmanaged. Mapping the full 4 GiB would cost
4 MiB of page tables to describe memory this kernel has no use for.

## Scheduling

Tasks are kernel threads: they share the kernel address space and each
owns a 16 KiB stack. A circular list is the run queue, and the timer
interrupt preempts the running task when its five-tick slice expires.

Two details are load-bearing, and both cost a debugging session if you
get them wrong:

- **The PIC is acknowledged before the handler runs.** The timer handler
  can context-switch away and not return for several slices. If the EOI
  waited until the handler returned, the controller would hold IRQ0
  in-service forever and no further timer interrupts would arrive --
  preemption would stop after exactly one switch.

- **`schedule()` never enables interrupts.** It runs inside the timer
  interrupt, where an `sti` would let the timer re-enter the handler on
  the same stack. Callers that genuinely need to wait (`sched_sleep`,
  `sched_exit`) do their idling outside it, in task context.

A new task gets a synthetic stack laid out so that the context switch's
final `ret` lands on its entry point, with a return address pointing at
an exit trampoline. That way a task that simply returns is cleaned up
rather than running off into whatever was on the stack.

## Software you never wrote

This is the part of the system that is not like other systems, so it is
worth explaining what it actually does.

### Observing

`kernel/journal.c` is the observation layer, and it is a kernel service
rather than a shell feature on purpose: anything that acts on the user's
behalf can record what it did, and the miner sees one unified stream.
Every action is interned down to a handful of integers, which is what
makes it affordable to re-mine the entire history after every single
action.

The shell is currently the only thing feeding it. Commands declare where
their free-form text begins, so `append log.txt fixed the parser` is
recorded as *append, to log.txt, this prose* rather than as five loose
words. Without that, two runs of the same command with different amounts
of text would look like different kinds of action and never be seen as a
repetition at all.

Commands about the automation system itself are deliberately not
observed. Asking the OS what it has learned must not become something it
learns.

### Generalising

`kernel/automate.c` is the part that does the thinking. After every
action it folds the trailing window of the journal, at every length up to
four, into a table of patterns. Two windows are the same pattern when
their verbs and argument counts line up -- and then the arguments are
*unified* rather than compared:

```
mkdir alpha ; cd alpha      everything is literal so far
mkdir beta  ; cd beta       both positions changed -- and changed together
mkdir gamma ; cd gamma      -> one parameter, used twice

              mkdir $1 ; cd $1
```

Positions that stay the same stay literal. Positions that change become
parameters. Positions that always change *to the same value as each
other* collapse into a single parameter. That last step is the whole
game: it is the difference between replaying a recording and writing a
function with a signature.

### Knowing when to shut up

A sliding window over a stream of actions finds an enormous number of
"patterns" that are really the same observation wearing different hats.
Three rules keep the suggestions to things a person would recognise:

- **Periodic windows are ignored.** Typing `ls` four times makes the
  window `ls ls` look like a repeated pair. Alternating `cd` and `ls`
  makes `cd ls cd` look like a repeated triple. Any pattern that is a
  shorter pattern seen through a sliding window is discarded in favour of
  the shorter one. (The period deliberately does not have to divide the
  length -- requiring that catches `A A` and misses `A B A`.)

- **Rotations are the same discovery.** A stream of `mkdir` and `cd`
  contains `cd ; mkdir` exactly as often as `mkdir ; cd`, because the
  window has no idea where the user thinks the work begins. The tie is
  broken by recency: the unit that *just finished* is the one the user
  thinks they were doing.

- **Suggestions settle before they are spoken.** A shorter pattern
  crosses the threshold one action before the longer pattern containing
  it, so the engine's first idea is routinely superseded a moment later.
  A proposal waits one action before being announced, which lets the
  improvement replace it silently instead of the user being shown a
  suggestion and then a correction. Nothing already said out loud is ever
  retracted.

And one rule about taste: a bare repeated command is never proposed. If
nothing about `ls` is fixed and nothing varies, an automation for it
would just be `ls` with extra steps.

### Accreting

An accepted automation becomes a command, indistinguishable from a
built-in at the prompt, and is written to `/auto/<name>` so the software
you never wrote is at least something you can read:

```
nebula:/$ cat /auto/project
# learned from 3 repetitions, 1 parameter
mkdir $1
cd $1
cd ..
```

When an automation runs, its steps are **not** journalled individually --
only the invocation is. If the steps were recorded, running an automation
would re-teach the OS the very pattern it was built from. Recording the
invocation instead is also what makes the idea compound: the engine does
not care whether a verb is a built-in or something it taught you last
week, so a repeated run of learned commands is itself a pattern. Small
programs accrete into larger ones the same way the first ones appeared.

### Staying in charge

The engine proposes; it never adopts. `auto` shows what it has learned,
what it is offering, and -- importantly -- the raw watchlist of what it
has noticed repeating and how often, so the mechanism is inspectable
rather than magic. `dismiss` rejects a suggestion permanently, `forget`
deletes an automation, and `history` shows the journal the whole thing is
built on.

## The shell

26 commands, all built in, plus however many the OS has written for you:

| | |
|---|---|
| `help` `clear` `echo` `uname` | basics |
| `ls` `cd` `pwd` `cat` `mkdir` `write` `append` `rm` | filesystem |
| `mem` `memmap` | physical memory, heap and BIOS map |
| `ps` `spawn` `sleep` `uptime` `date` | tasks and time |
| `auto` `accept` `dismiss` `forget` `history` | the automation system |
| `fault` `reboot` | deliberately page-fault; restart |

`spawn` starts a background worker that prints and sleeps, which is the
quickest way to watch preemption actually happening: run `spawn 5` and
keep typing.

`fault` writes to an unmapped address on purpose. The trap handler
reports the faulting address, decodes the error code, and halts:

```
*** KERNEL PANIC ***
page fault writing 0x40000000 from kernel mode
  cause: page not present
  eip=0x00013fd3  eflags=0x00000286
System halted.
```

## Testing

```sh
make unit-test   # host-side unit tests, no emulator needed  (138 checks)
make test        # boot in QEMU and drive the shell           (69 checks)
make check       # both
```

The two suites cover different things on purpose.

`tests/host_tests.c` compiles the hardware-independent code -- the string
routines, the printf formatter, the filesystem, the journal and the
automation engine -- for the host and tests it directly, with `kmalloc`
redirected to `malloc` and a switch to make allocation fail on demand.
This is where the awkward edge cases live: truncation, overlapping
`memmove`, integer overflow, malformed paths, out-of-memory paths, and
every inference rule in the miner -- that parameters unify, that periodic
windows are rejected, that a dismissal sticks. It runs in milliseconds,
which matters, because the miner is the part most likely to be wrong in a
way that only shows up on the fourth repetition.

`scripts/boot-test.py` boots the real image under QEMU and drives the
real shell over the serial console, asserting on what comes back. It
covers what only a running machine can show: that the boot sector loads
the kernel, that paging translates, that interrupts fire, that a spawned
task is actually scheduled while the shell sleeps, and that a bad memory
access produces a diagnosed panic rather than a reboot loop.

`kmain` also runs a set of self-tests at boot, printed to the console, so
a machine that boots wrong says so on screen.

If you need to see inside a boot that is failing before the console is
up, `scripts/qemu-probe.py` dumps the VGA text buffer and CPU state from
the QEMU monitor.

## What is not here

Worth being explicit, since these are the obvious next steps rather than
oversights:

- **No user mode.** The GDT has ring-3 segments and a TSS, and there is a
  DPL-3 syscall gate at `int $0x80`, but nothing runs in ring 3 yet.
  Every task is a kernel thread.
- **No disk driver.** The boot sector's BIOS reads are the only disk I/O.
  The filesystem is in RAM and does not survive a reboot.
- **No ELF loader**, so there are no separate programs: the shell and its
  commands are compiled into the kernel.
- **Single processor.** No APIC, no SMP.
- **The scheduler is round-robin with a fixed slice.** No priorities, no
  blocking primitives beyond sleep.

And about the automation engine specifically, since overclaiming here
would be the easiest thing in the world:

- **The shell is the only sensor.** The journal is a kernel service and
  anything could feed it, but today nothing else does. A real version of
  this idea would observe file operations, window focus, and the things
  people actually repeat that are not typed commands.
- **Only contiguous runs are mined.** If you do A, then something
  unrelated, then B, the pair A-B is not noticed. Real repetition is
  often interrupted, and handling that needs gap-tolerant matching.
- **Sequences are capped at four actions** and automations are
  straight-line: no conditionals, no loops, no error handling. They do
  the same steps in the same order every time.
- **Parameters are positional and untyped.** The engine knows that a
  position varies, not what kind of thing belongs there, so it cannot
  warn you that you passed a filename where it expected a number.
- **Nothing persists.** The journal, the patterns and the automations are
  all in RAM, so a reboot forgets everything the machine learned about
  you. That is a property of having no disk driver, not a design choice.

## License

Same license as the containing repository.
