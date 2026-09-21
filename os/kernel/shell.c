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
#include "journal.h"
#include "automate.h"
#include "io.h"

#define MAX_LINE 256
#define MAX_ARGS 16

/* An automation may call another; this bounds the recursion. */
#define AUTOMATION_MAX_DEPTH 4

static struct fs_node *cwd;

struct command {
    const char *name;
    const char *usage;
    const char *summary;

    /*
     * Where free-form text begins in argv, or 0 for a command whose
     * arguments are all positional.
     *
     * The automation engine compares actions by shape, so `append log a`
     * and `append log a b c` would otherwise look like different kinds
     * of action and never be seen as a repeat. Declaring that `append`
     * takes a filename and then prose lets the engine record the prose
     * as one argument, which is what it means to the user.
     */
    uint8_t tail_index;

    /*
     * Whether doing this counts as work worth watching. Commands that
     * are about the automation system itself are excluded, so asking
     * what the OS has learned does not become something it learns.
     */
    bool observable;

    void (*run)(int argc, char **argv);
};

static const struct command commands[];
static void dispatch(int argc, char **argv, int depth);

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


/* ------------------------------------------------------------------ */
/* The automation system                                               */
/* ------------------------------------------------------------------ */

/* Keep an accepted automation visible in the filesystem, so the software
 * the user never wrote is at least something they can read. */
static void publish_automation(const struct automation *automation)
{
    char path[FS_PATH_MAX];

    if (!fs_lookup(fs_root(), "/auto"))
        fs_create(fs_root(), "/auto", FS_DIR);

    ksnprintf(path, sizeof(path), "/auto/%s", automation->name);

    struct fs_node *file = fs_lookup(fs_root(), path);
    if (!file)
        file = fs_create(fs_root(), path, FS_FILE);
    if (!file)
        return;

    char body[AUTOMATION_LINE_MAX * PATTERN_MAX_LEN + 128];
    size_t pos = 0;

    pos += (size_t)ksnprintf(body + pos, sizeof(body) - pos,
                             "# learned from %u repetitions, %u parameter%s\n",
                             automation->learned_from, automation->param_count,
                             automation->param_count == 1 ? "" : "s");

    for (uint8_t i = 0; i < automation->line_count && pos < sizeof(body); i++)
        pos += (size_t)ksnprintf(body + pos, sizeof(body) - pos, "%s\n",
                                 automation->lines[i]);

    fs_write(file, body, pos);
}

static void unpublish_automation(const char *name)
{
    char path[FS_PATH_MAX];

    ksnprintf(path, sizeof(path), "/auto/%s", name);

    struct fs_node *file = fs_lookup(fs_root(), path);
    if (file)
        fs_unlink(file);
}

/* Tell the user about anything the engine has newly worked out. */
static void announce_proposals(void)
{
    const struct proposal *proposal;

    while ((proposal = automate_next_unannounced()) != NULL) {
        vga_set_color(VGA_YELLOW, VGA_BLACK);
        kprintf("\n[auto] you have done this %u times:\n",
                proposal->occurrences);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

        for (uint8_t i = 0; i < proposal->line_count; i++)
            kprintf("         %s\n", proposal->example[i]);

        vga_set_color(VGA_YELLOW, VGA_BLACK);
        kprintf("       i can make that a command");
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

        if (proposal->param_count)
            kprintf(" taking %u argument%s", proposal->param_count,
                    proposal->param_count == 1 ? "" : "s");
        kprintf(":\n");

        for (uint8_t i = 0; i < proposal->line_count; i++)
            kprintf("         %s\n", proposal->lines[i]);

        kprintf("       `accept %u` to keep it as `%s`, or `dismiss %u`\n\n",
                proposal->id, proposal->name, proposal->id);
    }
}

static void cmd_auto(int argc, char **argv)
{
    uint32_t automations = automate_count();
    uint32_t pending     = automate_proposal_count();

    kprintf("automations (%u):\n", automations);

    for (uint32_t i = 0; i < automations; i++) {
        const struct automation *automation = automate_at(i);

        vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
        kprintf("  %-16s", automation->name);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        kprintf(" %u arg%s, run %u time%s, learned from %u repetitions\n",
                automation->param_count,
                automation->param_count == 1 ? " " : "s",
                automation->run_count,
                automation->run_count == 1 ? "" : "s",
                automation->learned_from);

        for (uint8_t line = 0; line < automation->line_count; line++)
            kprintf("      %s\n", automation->lines[line]);
    }

    if (!automations)
        kprintf("  (none yet -- repeat yourself and the OS will offer some)\n");

    kprintf("\nproposals (%u):\n", pending);

    for (uint32_t i = 0; i < pending; i++) {
        const struct proposal *proposal = automate_proposal_at(i);

        vga_set_color(VGA_YELLOW, VGA_BLACK);
        kprintf("  [%u] %-14s", proposal->id, proposal->name);
        vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);
        kprintf(" seen %u times, %u argument%s\n",
                proposal->occurrences, proposal->param_count,
                proposal->param_count == 1 ? "" : "s");

        for (uint8_t line = 0; line < proposal->line_count; line++)
            kprintf("      %s\n", proposal->lines[line]);
    }

    if (!pending)
        kprintf("  (none)\n");

    /* Showing the raw counts keeps the mechanism inspectable: the user
     * can see what the OS is building towards before it says anything. */
    uint32_t watching = automate_watch_count();

    kprintf("\nwatching (%u repeated sequences):\n", watching);

    for (uint32_t i = 0; i < watching && i < 8; i++) {
        char     text[AUTOMATION_LINE_MAX * 2];
        uint16_t occurrences = 0;
        bool     ready = false;

        if (!automate_watch_at(i, text, sizeof(text), &occurrences, &ready))
            break;

        kprintf("  %2ux  %s%s\n", occurrences, text, ready ? "  (ready)" : "");
    }

    if (!watching)
        kprintf("  (nothing repeated yet)\n");
}

static void cmd_accept(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: accept <proposal id> [name]\n");
        return;
    }

    int id = atoi_safe(argv[1]);
    if (id <= 0) {
        kprintf("accept: %s is not a proposal id\n", argv[1]);
        return;
    }

    const struct automation *automation =
        automate_accept((uint16_t)id, (argc > 2) ? argv[2] : NULL);

    if (!automation) {
        kprintf("accept: no proposal %d, or that name is taken\n", id);
        return;
    }

    publish_automation(automation);

    vga_set_color(VGA_LIGHT_GREEN, VGA_BLACK);
    kprintf("learned `%s`", automation->name);
    vga_set_color(VGA_LIGHT_GREY, VGA_BLACK);

    if (automation->param_count)
        kprintf(" -- run it as `%s%s`", automation->name,
                automation->param_count == 1 ? " <arg>" : " <arg> <arg>");

    kprintf(". saved to /auto/%s\n", automation->name);
}

static void cmd_dismiss(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: dismiss <proposal id>\n");
        return;
    }

    int id = atoi_safe(argv[1]);

    if (id <= 0 || !automate_dismiss((uint16_t)id))
        kprintf("dismiss: no proposal %s\n", argv[1]);
    else
        kprintf("dismissed; that pattern will not be suggested again\n");
}

static void cmd_forget(int argc, char **argv)
{
    if (argc < 2) {
        kprintf("usage: forget <automation>\n");
        return;
    }

    if (automate_forget(argv[1])) {
        unpublish_automation(argv[1]);
        kprintf("forgot `%s`\n", argv[1]);
    } else {
        kprintf("forget: no automation called %s\n", argv[1]);
    }
}

static void cmd_history(int argc, char **argv)
{
    int limit = (argc > 1) ? atoi_safe(argv[1]) : 20;

    if (limit <= 0)
        limit = 20;

    uint32_t held = journal_count();
    if ((uint32_t)limit > held)
        limit = (int)held;

    kprintf("%u actions recorded, %u kept\n", journal_total(), held);

    /* Oldest of the requested window first, so it reads as a sequence. */
    for (int i = limit - 1; i >= 0; i--) {
        const struct event *event = journal_recent((uint32_t)i);
        char                text[AUTOMATION_LINE_MAX];

        if (!event)
            continue;

        journal_format(event, text, sizeof(text));
        kprintf("  %5u  %s\n", event->serial + 1, text);
    }
}

static const struct command commands[] = {
    /*                                                            tail  observable */
    { "help",    "help",              "list these commands",         0, false, cmd_help },
    { "clear",   "clear",             "clear the screen",            0, false, cmd_clear },
    { "echo",    "echo <text>",       "print its arguments",         1, true,  cmd_echo },
    { "ls",      "ls [path]",         "list a directory",            0, true,  cmd_ls },
    { "cd",      "cd [path]",         "change directory",            0, true,  cmd_cd },
    { "pwd",     "pwd",               "print the working directory", 0, true,  cmd_pwd },
    { "cat",     "cat <file>",        "print a file",                0, true,  cmd_cat },
    { "mkdir",   "mkdir <name>",      "create a directory",          0, true,  cmd_mkdir },
    { "write",   "write <f> <text>",  "write text to a file",        2, true,  cmd_write },
    { "append",  "append <f> <text>", "append text to a file",       2, true,  cmd_append },
    { "rm",      "rm <name>",         "remove a file or directory",  0, true,  cmd_rm },
    { "mem",     "mem",               "show memory and heap usage",  0, true,  cmd_mem },
    { "memmap",  "memmap",            "show the BIOS memory map",    0, true,  cmd_memmap },
    { "ps",      "ps",                "list tasks",                  0, true,  cmd_ps },
    { "spawn",   "spawn [rounds]",    "start a background worker",   0, true,  cmd_spawn },
    { "sleep",   "sleep <ms>",        "block the shell for a while", 0, true,  cmd_sleep },
    { "uptime",  "uptime",            "time since boot",             0, true,  cmd_uptime },
    { "date",    "date",              "read the real-time clock",    0, true,  cmd_date },
    { "uname",   "uname",             "kernel version",              0, true,  cmd_uname },

    /* The automation system. None of these are observable: asking what
     * the OS has learned must not itself become something it learns. */
    { "auto",    "auto",              "automations, proposals, watchlist", 0, false, cmd_auto },
    { "accept",  "accept <id> [name]","turn a proposal into a command",    0, false, cmd_accept },
    { "dismiss", "dismiss <id>",      "reject a proposal for good",        0, false, cmd_dismiss },
    { "forget",  "forget <name>",     "delete an automation",              0, false, cmd_forget },
    { "history", "history [n]",       "show the action journal",           0, false, cmd_history },

    { "fault",   "fault",             "trigger a page fault on purpose", 0, false, cmd_fault },
    { "reboot",  "reboot",            "restart the machine",             0, false, cmd_reboot },
    { NULL, NULL, NULL, 0, false, NULL }
};


/* ------------------------------------------------------------------ */
/* Dispatch                                                            */
/* ------------------------------------------------------------------ */

static const struct command *find_command(const char *name)
{
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(name, commands[i].name) == 0)
            return &commands[i];
    }

    return NULL;
}

/*
 * Record what the user just did.
 *
 * `tail_index` is where the command's free-form text starts. Everything
 * from there is joined into a single argument, because that is what it
 * is: one piece of prose, not N words. Without this, two runs of the
 * same command with different amounts of text would look like different
 * shapes of action and never be recognised as a repetition.
 */
static void record_action(uint8_t tail_index, int argc, char **argv)
{
    char  joined[INTERN_STRING_MAX];
    char *record_argv[EVENT_MAX_ARGS + 1];
    int   record_argc = 0;

    const struct event *event;

    if (tail_index > 0 && argc > tail_index + 1) {
        for (int i = 0; i < tail_index && record_argc < EVENT_MAX_ARGS; i++)
            record_argv[record_argc++] = argv[i];

        size_t pos = 0;
        for (int i = tail_index; i < argc; i++) {
            size_t len = strlen(argv[i]);

            if (i > tail_index && pos + 1 < sizeof(joined))
                joined[pos++] = ' ';

            for (size_t j = 0; j < len && pos + 1 < sizeof(joined); j++)
                joined[pos++] = argv[i][j];
        }
        joined[pos] = '\0';

        record_argv[record_argc++] = joined;

        event = journal_record(record_argv[0], record_argc, record_argv);
    } else {
        /*
         * Hand the whole argument list over untouched. The journal joins
         * anything past its own limit into the final slot, which is
         * lossless; copying into a bounded array here would instead drop
         * the tail and quietly record a different action than happened.
         */
        event = journal_record(argv[0], argc, argv);
    }

    if (event)
        automate_observe(event);
}

static void run_automation(const struct automation *automation,
                           int argc, char **argv, int depth)
{
    if (depth >= AUTOMATION_MAX_DEPTH) {
        kprintf("%s: automations nested too deeply\n", argv[0]);
        return;
    }

    if (argc <= automation->param_count) {
        kprintf("%s: needs %u argument%s\n", automation->name,
                automation->param_count,
                automation->param_count == 1 ? "" : "s");
        return;
    }

    automate_note_run(automation);

    for (uint8_t line = 0; line < automation->line_count; line++) {
        char  expanded[AUTOMATION_LINE_MAX];
        char *sub_argv[MAX_ARGS];

        if (!automate_expand(automation, line, argc, argv,
                             expanded, sizeof(expanded))) {
            kprintf("%s: could not expand step %u\n", automation->name, line + 1);
            return;
        }

        int sub_argc = tokenize(expanded, sub_argv, MAX_ARGS);
        if (sub_argc == 0)
            continue;

        dispatch(sub_argc, sub_argv, depth + 1);
    }
}

/*
 * Run one command line.
 *
 * Only the outermost invocation is journalled. The steps an automation
 * performs are not recorded individually -- if they were, running an
 * automation would re-teach the OS the very pattern it was built from.
 * Recording the invocation instead is also what lets automations compose:
 * a repeated sequence of automation calls is itself a pattern, so the
 * software the user never wrote can be built out of more of the same.
 */
static void dispatch(int argc, char **argv, int depth)
{
    if (argc == 0)
        return;

    const struct command *cmd = find_command(argv[0]);

    if (cmd) {
        cmd->run(argc, argv);

        if (depth == 0 && cmd->observable)
            record_action(cmd->tail_index, argc, argv);

        return;
    }

    const struct automation *automation = automate_find(argv[0]);

    if (automation) {
        run_automation(automation, argc, argv, depth);

        if (depth == 0)
            record_action(0, argc, argv);

        return;
    }

    kprintf("%s: command not found. Try `help`.\n", argv[0]);
}

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

    kprintf("\nWelcome to Nebula OS. Type `help` to get started.\n");
    kprintf("This system watches what you repeat and offers to automate it;\n");
    kprintf("`auto` shows what it has noticed so far.\n\n");

    for (;;) {
        prompt();

        size_t len = input_readline(line, sizeof(line));
        if (len == 0)
            continue;

        int argc = tokenize(line, argv, MAX_ARGS);
        if (argc == 0)
            continue;

        dispatch(argc, argv, 0);

        /* Say anything the engine worked out, then let this action's
         * discoveries settle so they can be announced next time. */
        announce_proposals();
        automate_settle();
    }
}
