/*
 * shell.c -- the interactive command shell.
 *
 * Reads a line, splits it on whitespace, and dispatches on the first
 * token. Commands are table-driven so `help` can describe them without a
 * second list drifting out of date.
 */

#include "shell.h"
#include "input.h"
#include "kprintf.h"
#include "string.h"
#include "vga.h"
#include "fs.h"
#include "heap.h"
#include "pmm.h"
#include "paging.h"
#include "sched.h"
#include "timer.h"
#include "rtc.h"
#include "io.h"

#define MAX_LINE 256
#define MAX_ARGS 16

static struct fs_node *cwd;

struct command {
    const char *name;
    const char *usage;
    const char *summary;
    void (*run)(int argc, char **argv);
};

static const struct command commands[];

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

/* Split `line` in place into argv. Returns the argument count. */
static int tokenize(char *line, char **argv, int max_args)
{
    int argc = 0;

    while (*line && argc < max_args) {
        while (*line == ' ' || *line == '\t')
            *line++ = '\0';

        if (!*line)
            break;

        argv[argc++] = line;

        while (*line && *line != ' ' && *line != '\t')
            line++;
    }

    return argc;
}

/* Join argv[from..argc) with single spaces into buf. */
static size_t join_args(char *buf, size_t size, int argc, char **argv, int from)
{
    size_t pos = 0;

    for (int i = from; i < argc; i++) {
        size_t len = strlen(argv[i]);

        if (i > from && pos + 1 < size)
            buf[pos++] = ' ';

        for (size_t j = 0; j < len && pos + 1 < size; j++)
            buf[pos++] = argv[i][j];
    }

    buf[pos] = '\0';
    return pos;
}

static void print_size(size_t bytes)
{
    if (bytes >= 1024 * 1024)
        kprintf("%u.%u MiB", (uint32_t)(bytes / (1024 * 1024)),
                (uint32_t)((bytes % (1024 * 1024)) * 10 / (1024 * 1024)));
    else if (bytes >= 1024)
        kprintf("%u.%u KiB", (uint32_t)(bytes / 1024),
                (uint32_t)((bytes % 1024) * 10 / 1024));
    else
        kprintf("%u B", (uint32_t)bytes);
}

/* ------------------------------------------------------------------ */
/* Commands                                                            */
/* ------------------------------------------------------------------ */

static void cmd_help(int argc, char **argv)
{
    kprintf("Nebula OS shell. Available commands:\n\n");

    for (int i = 0; commands[i].name; i++)
        kprintf("  %-22s %s\n", commands[i].usage, commands[i].summary);

    kprintf("\n");
}

static void cmd_clear(int argc, char **argv)
{
    vga_clear();
}

static void cmd_echo(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        kprintf("%s%s", argv[i], (i + 1 < argc) ? " " : "");

    kprintf("\n");
}

static void cmd_ls(int argc, char **argv)
{
    struct fs_node *dir = cwd;

    if (argc > 1) {
        dir = fs_lookup(cwd, argv[1]);
        if (!dir) {
            kprintf("ls: %s: no such file or directory\n", argv[1]);
            return;
        }
    }

    if (dir->type == FS_FILE) {
        kprintf("%8u  %s\n", (uint32_t)dir->size, dir->name);
        return;
    }

    uint32_t files = 0, dirs = 0;

    for (struct fs_node *child = dir->children; child; child = child->sibling) {
        if (child->type == FS_DIR) {
            vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
            kprintf("     <dir>  %s/\n", child->name);
            vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
            dirs++;
        } else {
            kprintf("%10u  %s\n", (uint32_t)child->size, child->name);
            files++;
        }
    }

    if (!files && !dirs)
        kprintf("(empty)\n");
}

static void cmd_cd(int argc, char **argv)
{
    if (argc < 2) {
        cwd = fs_root();
        return;
    }

    struct fs_node *target = fs_lookup(cwd, argv[1]);

    if (!target) {
        kprintf("cd: %s: no such file or directory\n", argv[1]);
        return;
    }

    if (target->type != FS_DIR) {
        kprintf("cd: %s: not a directory\n", argv[1]);
        return;
    }

    cwd = target;
}

static void cmd_pwd(int argc, char **argv)
{
    char path[FS_PATH_MAX];

    fs_path(cwd, path, sizeof(path));
    kprintf("%s\n", path);
}

static void cmd_cat(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: cat <file>\n");
        return;
    }

    struct fs_node *file = fs_lookup(cwd, argv[1]);

    if (!file) {
        kprintf("cat: %s: no such file or directory\n", argv[1]);
        return;
    }

    if (file->type != FS_FILE) {
        kprintf("cat: %s: is a directory\n", argv[1]);
        return;
    }

    for (size_t i = 0; i < file->size; i++)
        kputchar((char)file->data[i]);

    /* Do not leave the prompt hanging off the end of an unterminated file. */
    if (file->size && file->data[file->size - 1] != '\n')
        kputchar('\n');
}

static void cmd_mkdir(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: mkdir <name>\n");
        return;
    }

    if (!fs_create(cwd, argv[1], FS_DIR))
        kprintf("mkdir: %s: cannot create directory\n", argv[1]);
}

static void cmd_write(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: write <file> [text...]\n");
        return;
    }

    struct fs_node *file = fs_lookup(cwd, argv[1]);

    if (!file) {
        file = fs_create(cwd, argv[1], FS_FILE);
        if (!file) {
            kprintf("write: %s: cannot create file\n", argv[1]);
            return;
        }
    }

    if (file->type != FS_FILE) {
        kprintf("write: %s: is a directory\n", argv[1]);
        return;
    }

    char text[MAX_LINE];
    size_t len = join_args(text, sizeof(text) - 1, argc, argv, 2);

    text[len++] = '\n';

    if (!fs_write(file, text, len))
        kprintf("write: out of memory\n");
}

static void cmd_append(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: append <file> [text...]\n");
        return;
    }

    struct fs_node *file = fs_lookup(cwd, argv[1]);

    if (!file) {
        file = fs_create(cwd, argv[1], FS_FILE);
        if (!file) {
            kprintf("append: %s: cannot create file\n", argv[1]);
            return;
        }
    }

    if (file->type != FS_FILE) {
        kprintf("append: %s: is a directory\n", argv[1]);
        return;
    }

    char text[MAX_LINE];
    size_t len = join_args(text, sizeof(text) - 1, argc, argv, 2);

    text[len++] = '\n';

    if (!fs_append(file, text, len))
        kprintf("append: out of memory\n");
}

static void cmd_rm(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: rm <name>\n");
        return;
    }

    struct fs_node *node = fs_lookup(cwd, argv[1]);

    if (!node) {
        kprintf("rm: %s: no such file or directory\n", argv[1]);
        return;
    }

    if (node == cwd) {
        kprintf("rm: refusing to remove the current directory\n");
        return;
    }

    if (!fs_unlink(node))
        kprintf("rm: %s: not empty, or not removable\n", argv[1]);
}

static void cmd_mem(int argc, char **argv)
{
    size_t heap_total, heap_used, heap_blocks;
    heap_stats(&heap_total, &heap_used, &heap_blocks);

    uint32_t frames = pmm_total_frames();

    kprintf("physical memory\n");
    kprintf("  total      %u frames (", frames);
    print_size((size_t)frames * PAGE_SIZE);
    kprintf(")\n");
    kprintf("  used       %u frames (", pmm_used_frames());
    print_size((size_t)pmm_used_frames() * PAGE_SIZE);
    kprintf(")\n");
    kprintf("  free       %u frames (", pmm_free_frames());
    print_size((size_t)pmm_free_frames() * PAGE_SIZE);
    kprintf(")\n");

    kprintf("kernel heap at %p\n", (void *)KHEAP_START);
    kprintf("  mapped     ");
    print_size(heap_total);
    kprintf("\n  allocated  ");
    print_size(heap_used);
    kprintf("\n  blocks     %u\n", (uint32_t)heap_blocks);

    kprintf("filesystem\n");
    kprintf("  nodes      %u\n", fs_node_count());
    kprintf("  content    ");
    print_size(fs_total_bytes());
    kprintf("\n");
}

static void cmd_memmap(int argc, char **argv)
{
    pmm_dump_map();
}

static void cmd_ps(int argc, char **argv)
{
    sched_dump();
}

static void cmd_uptime(int argc, char **argv)
{
    uint32_t ms = timer_uptime_ms();
    uint32_t seconds = ms / 1000;

    kprintf("up %u:%02u:%02u.%03u (%u timer ticks at %u Hz)\n",
            seconds / 3600, (seconds / 60) % 60, seconds % 60, ms % 1000,
            timer_ticks(), TIMER_HZ);
}

static void cmd_date(int argc, char **argv)
{
    struct rtc_time now;

    rtc_read(&now);
    kprintf("%04u-%02u-%02u %02u:%02u:%02u UTC\n",
            now.year, now.month, now.day, now.hour, now.minute, now.second);
}

static void cmd_uname(int argc, char **argv)
{
    kprintf("Nebula 0.1.0 i386 (32-bit protected mode, paging enabled)\n");
}

/* A demo task, so `spawn` has something to run. */
static void worker_task(void *arg)
{
    uint32_t rounds = (uint32_t)arg;

    for (uint32_t i = 1; i <= rounds; i++) {
        kprintf("[worker %u] tick %u/%u\n", sched_current()->id, i, rounds);
        sched_sleep(500);
    }

    kprintf("[worker %u] finished\n", sched_current()->id);
}

static void cmd_spawn(int argc, char **argv)
{
    int rounds = (argc > 1) ? atoi_safe(argv[1]) : 3;

    if (rounds <= 0 || rounds > 100) {
        kprintf("usage: spawn [rounds 1-100]\n");
        return;
    }

    uint32_t id = sched_spawn("worker", worker_task, (void *)(uint32_t)rounds);

    if (id)
        kprintf("spawned task %u for %d rounds\n", id, rounds);
    else
        kprintf("spawn: could not create the task\n");
}

static void cmd_sleep(int argc, char **argv)
{
    int ms = (argc > 1) ? atoi_safe(argv[1]) : -1;

    if (ms < 0) {
        kprintf("usage: sleep <milliseconds>\n");
        return;
    }

    sched_sleep((uint32_t)ms);
}

static void cmd_reboot(int argc, char **argv)
{
    kprintf("rebooting...\n");

    /* Pulse the keyboard controller's reset line, the traditional way to
     * reset a PC from software. */
    uint8_t status;
    do {
        status = inb(0x64);
        if (status & 1)
            (void)inb(0x60);    /* drain, or the controller ignores us */
    } while (status & 2);

    outb(0x64, 0xFE);

    /* If that did nothing, force a triple fault by loading a null IDT
     * and raising an interrupt. */
    struct {
        uint16_t limit;
        uint32_t base;
    } __attribute__((packed)) null_idt = { 0, 0 };

    __asm__ volatile("lidt %0; int $3" : : "m"(null_idt));

    for (;;)
        hlt();
}

static void cmd_fault(int argc, char **argv)
{
    kprintf("dereferencing an unmapped address to demonstrate the fault handler\n");

    /* 0x40000000 is above the identity map and nothing maps it. */
    volatile uint32_t *bad = (volatile uint32_t *)0x40000000;
    *bad = 0x1234;

    kprintf("unexpectedly survived\n");
}

static const struct command commands[] = {
    { "help",    "help",              "list these commands",                 cmd_help },
    { "clear",   "clear",             "clear the screen",                    cmd_clear },
    { "echo",    "echo <text>",       "print its arguments",                 cmd_echo },
    { "ls",      "ls [path]",         "list a directory",                    cmd_ls },
    { "cd",      "cd [path]",         "change directory",                    cmd_cd },
    { "pwd",     "pwd",               "print the working directory",         cmd_pwd },
    { "cat",     "cat <file>",        "print a file",                        cmd_cat },
    { "mkdir",   "mkdir <name>",      "create a directory",                  cmd_mkdir },
    { "write",   "write <f> <text>",  "write text to a file",                cmd_write },
    { "append",  "append <f> <text>", "append text to a file",               cmd_append },
    { "rm",      "rm <name>",         "remove a file or empty directory",    cmd_rm },
    { "mem",     "mem",               "show memory and heap usage",          cmd_mem },
    { "memmap",  "memmap",            "show the BIOS physical memory map",   cmd_memmap },
    { "ps",      "ps",                "list tasks",                          cmd_ps },
    { "spawn",   "spawn [rounds]",    "start a background worker task",      cmd_spawn },
    { "sleep",   "sleep <ms>",        "block the shell for a while",         cmd_sleep },
    { "uptime",  "uptime",            "time since boot",                     cmd_uptime },
    { "date",    "date",              "read the CMOS real-time clock",       cmd_date },
    { "uname",   "uname",             "kernel version",                      cmd_uname },
    { "fault",   "fault",             "trigger a page fault on purpose",     cmd_fault },
    { "reboot",  "reboot",            "restart the machine",                 cmd_reboot },
    { NULL, NULL, NULL, NULL }
};

/* ------------------------------------------------------------------ */
/* Main loop                                                           */
/* ------------------------------------------------------------------ */

static void prompt(void)
{
    char path[FS_PATH_MAX];

    fs_path(cwd, path, sizeof(path));

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("nebula");
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf(":");
    vga_set_color(VGA_LIGHT_BLUE, VGA_BLACK);
    kprintf("%s", path);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
    kprintf("$ ");
}

void shell_task(void *arg)
{
    char  line[MAX_LINE];
    char *argv[MAX_ARGS];

    cwd = fs_root();

    kprintf("\nWelcome to Nebula OS. Type `help` to get started.\n\n");

    for (;;) {
        prompt();

        size_t len = input_readline(line, sizeof(line));
        if (len == 0)
            continue;

        int argc = tokenize(line, argv, MAX_ARGS);
        if (argc == 0)
            continue;

        const struct command *cmd = NULL;
        for (int i = 0; commands[i].name; i++) {
            if (strcmp(argv[0], commands[i].name) == 0) {
                cmd = &commands[i];
                break;
            }
        }

        if (cmd)
            cmd->run(argc, argv);
        else
            kprintf("%s: command not found. Try `help`.\n", argv[0]);
    }
}
