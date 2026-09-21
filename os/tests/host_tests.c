/*
 * host_tests.c -- unit tests for the kernel's portable logic.
 *
 * The string routines, the printf formatter and the filesystem are all
 * ordinary C with no hardware dependencies, so they can be compiled for
 * the host and tested in a fraction of a second. That makes it cheap to
 * cover the edge cases that are awkward to reach from a booted kernel:
 * truncation, overlapping copies, allocation failure, malformed paths.
 *
 * The hardware-facing pieces -- paging, interrupts, context switching --
 * are covered by the boot tests in scripts/boot-test.py instead, because
 * there is nothing meaningful to test about them off the metal.
 *
 * Built and run by `make unit-test`.
 */

#include "types.h"
#include "string.h"
#include "kprintf.h"
#include "fs.h"
#include "heap.h"
#include "vga.h"
#include "serial.h"
#include "timer.h"
#include "journal.h"
#include "automate.h"

/* Declared by hand rather than including the host's headers, which would
 * collide with the kernel's freestanding types. */
extern void *malloc(size_t size);
extern void *realloc(void *ptr, size_t size);
extern void  free(void *ptr);
extern int   printf(const char *fmt, ...);
extern int   putchar(int c);

static int checks_run;
static int checks_failed;

static void check(const char *name, int ok)
{
    checks_run++;

    if (ok) {
        printf("  PASS  %s\n", name);
    } else {
        checks_failed++;
        printf("  FAIL  %s\n", name);
    }
}

static void check_str(const char *name, const char *got, const char *want)
{
    int ok = strcmp(got, want) == 0;

    checks_run++;

    if (ok) {
        printf("  PASS  %s\n", name);
    } else {
        checks_failed++;
        printf("  FAIL  %s\n          got  %s\n          want %s\n",
               name, got, want);
    }
}

/* ------------------------------------------------------------------ */
/* Kernel dependencies, satisfied by the host                          */
/* ------------------------------------------------------------------ */

void vga_putchar(char c)        { (void)c; }
void serial_putchar(char c)     { (void)c; }
void vga_set_color(enum vga_color fg, enum vga_color bg) { (void)fg; (void)bg; }
uint32_t timer_uptime_ms(void)  { return 0; }

/* A failure switch, so allocation failure paths can be tested. */
static int allocations_fail;

void *kmalloc(size_t size)  { return allocations_fail ? NULL : malloc(size); }
void  kfree(void *ptr)      { free(ptr); }

void *kcalloc(size_t count, size_t size)
{
    if (count && size > (size_t)0xFFFFFFFFu / count)
        return NULL;

    void *ptr = kmalloc(count * size);
    if (ptr)
        memset(ptr, 0, count * size);

    return ptr;
}

void *krealloc(void *ptr, size_t size)
{
    return allocations_fail ? NULL : realloc(ptr, size);
}

/* ------------------------------------------------------------------ */
/* Tests                                                               */
/* ------------------------------------------------------------------ */

static void test_string(void)
{
    printf("\nstring routines:\n");

    check("strlen of an empty string", strlen("") == 0);
    check("strlen counts characters", strlen("nebula") == 6);

    check("strcmp orders by character value", strcmp("a", "b") < 0);
    check("strcmp is symmetric", strcmp("b", "a") > 0);
    check("strcmp reports equality", strcmp("same", "same") == 0);
    check("strcmp treats bytes as unsigned",
          strcmp("\x80", "\x01") > 0);

    check("strncmp stops at n", strncmp("abcdef", "abcxyz", 3) == 0);
    check("strncmp finds a difference inside n",
          strncmp("abcdef", "abcxyz", 4) != 0);
    check("strncmp with n = 0 is always equal", strncmp("a", "b", 0) == 0);

    char buf[16];

    memset(buf, 'x', sizeof(buf));
    strlcpy(buf, "hi", sizeof(buf));
    check_str("strlcpy copies and terminates", buf, "hi");

    size_t needed = strlcpy(buf, "much too long for this", 8);
    check_str("strlcpy truncates to fit", buf, "much to");
    check("strlcpy returns the length it needed", needed == 22);

    strlcpy(buf, "abc", 1);
    check_str("strlcpy with size 1 writes only the terminator", buf, "");

    check("strchr finds a character", strchr("hello", 'l') != NULL);
    check("strchr returns the first match",
          strchr("hello", 'l') == strchr("hello", 'l'));
    check("strchr reports a missing character",
          strchr("hello", 'z') == NULL);
    check("strchr finds the terminator",
          strchr("hello", '\0') != NULL);

    /* memmove must cope with overlap in both directions; memcpy need not. */
    char overlap[] = "abcdefgh";
    memmove(overlap + 2, overlap, 6);
    check_str("memmove handles forward overlap", overlap, "ababcdef");

    char overlap2[] = "abcdefgh";
    memmove(overlap2, overlap2 + 2, 6);
    check_str("memmove handles backward overlap", overlap2, "cdefghgh");

    check("memcmp reports equality", memcmp("abc", "abc", 3) == 0);
    check("memcmp reports a difference", memcmp("abc", "abd", 3) != 0);
    check("memcmp with count 0 is equal", memcmp("a", "b", 0) == 0);

    check("atoi_safe parses digits", atoi_safe("1234") == 1234);
    check("atoi_safe parses zero", atoi_safe("0") == 0);
    check("atoi_safe rejects a non-digit", atoi_safe("12a") == -1);
    check("atoi_safe rejects an empty string", atoi_safe("") == -1);
    check("atoi_safe rejects a negative sign", atoi_safe("-5") == -1);
    check("atoi_safe accepts the largest int", atoi_safe("2147483647") == 2147483647);
    check("atoi_safe rejects one past the largest int",
          atoi_safe("2147483648") == -1);
    check("atoi_safe rejects a long overflowing string",
          atoi_safe("99999999999") == -1);
    check("atoi_safe rejects a very long digit string",
          atoi_safe("123456789012345678901234567890") == -1);
}

static void test_printf(void)
{
    printf("\nformatted output:\n");

    char buf[128];

    ksnprintf(buf, sizeof(buf), "%d", 0);
    check_str("%d prints zero", buf, "0");

    ksnprintf(buf, sizeof(buf), "%d", -1234);
    check_str("%d prints a negative number", buf, "-1234");

    ksnprintf(buf, sizeof(buf), "%d", (int32_t)0x80000000);
    check_str("%d handles the most negative integer", buf, "-2147483648");

    ksnprintf(buf, sizeof(buf), "%u", 4294967295u);
    check_str("%u prints the full unsigned range", buf, "4294967295");

    ksnprintf(buf, sizeof(buf), "%x|%X", 0xdeadbeef, 0xdeadbeef);
    check_str("%x and %X differ only in case", buf, "deadbeef|DEADBEEF");

    ksnprintf(buf, sizeof(buf), "%p", (void *)0x1234);
    check_str("%p is zero-padded and prefixed", buf, "0x00001234");

    ksnprintf(buf, sizeof(buf), "[%5d][%-5d][%05d]", 42, 42, 42);
    check_str("width, left-align and zero-pad", buf, "[   42][42   ][00042]");

    ksnprintf(buf, sizeof(buf), "[%6s][%-6s]", "ab", "ab");
    check_str("string padding on both sides", buf, "[    ab][ab    ]");

    ksnprintf(buf, sizeof(buf), "%s", (char *)NULL);
    check_str("a null string prints as (null)", buf, "(null)");

    ksnprintf(buf, sizeof(buf), "100%%");
    check_str("%% is a literal percent", buf, "100%");

    ksnprintf(buf, sizeof(buf), "%c%c", 'o', 'k');
    check_str("%c prints characters", buf, "ok");

    ksnprintf(buf, sizeof(buf), "%q", 1);
    check_str("an unknown conversion is shown verbatim", buf, "%q");

    ksnprintf(buf, sizeof(buf), "trailing %");
    check_str("a trailing percent is printed", buf, "trailing %");

    /* The return value is the length that would have been written, so
     * callers can detect truncation. */
    int n = ksnprintf(buf, 5, "%s", "0123456789");
    check_str("ksnprintf truncates to the buffer", buf, "0123");
    check("ksnprintf returns the untruncated length", n == 10);

    char tiny[1];
    ksnprintf(tiny, sizeof(tiny), "abc");
    check("ksnprintf with a 1-byte buffer writes only the terminator",
          tiny[0] == '\0');
}

static void test_fs(void)
{
    printf("\nfilesystem:\n");

    fs_init();

    check("the root exists", fs_root() != NULL);
    check("the seeded /home is present", fs_lookup(fs_root(), "/home") != NULL);
    check("the seeded readme is present",
          fs_lookup(fs_root(), "/home/readme.txt") != NULL);

    /* --- creation --- */
    struct fs_node *dir = fs_create(fs_root(), "/a", FS_DIR);
    check("a directory can be created", dir != NULL);
    check("creating the same name twice fails",
          fs_create(fs_root(), "/a", FS_DIR) == NULL);
    check("creating under a missing parent fails",
          fs_create(fs_root(), "/nope/x", FS_FILE) == NULL);
    check("creating with an empty name fails",
          fs_create(fs_root(), "/", FS_DIR) == NULL);
    check("'.' cannot be used as a name",
          fs_create(fs_root(), "/a/.", FS_DIR) == NULL);

    char long_name[64];
    memset(long_name, 'x', sizeof(long_name));
    long_name[0] = '/';
    long_name[sizeof(long_name) - 1] = '\0';
    check("an over-long name is rejected",
          fs_create(fs_root(), long_name, FS_FILE) == NULL);

    /* --- contents --- */
    struct fs_node *file = fs_create(fs_root(), "/a/f.txt", FS_FILE);
    check("a file can be created in a subdirectory", file != NULL);

    check("fs_write stores the bytes", fs_write(file, "hello", 5));
    check("the size reflects the write", file->size == 5);

    check("fs_append extends the file", fs_append(file, " world", 6));
    check("append concatenates",
          file->size == 11 && memcmp(file->data, "hello world", 11) == 0);

    /* Many small appends must keep the contents intact as the buffer
     * is reallocated underneath them. */
    fs_write(file, "", 0);
    for (int i = 0; i < 500; i++)
        fs_append(file, "ab", 2);
    check("repeated appends grow correctly", file->size == 1000);
    check("repeated appends preserve the contents",
          file->data[0] == 'a' && file->data[999] == 'b');

    check("writing to a directory fails", !fs_write(dir, "x", 1));

    /* --- lookup --- */
    check("lookup by absolute path", fs_lookup(fs_root(), "/a/f.txt") == file);
    check("lookup relative to a directory", fs_lookup(dir, "f.txt") == file);
    check("'.' resolves to the same node",
          fs_lookup(fs_root(), "/a/./f.txt") == file);
    check("'..' walks back up",
          fs_lookup(fs_root(), "/a/../a/f.txt") == file);
    check("'..' at the root stays at the root",
          fs_lookup(fs_root(), "/../..") == fs_root());
    check("repeated separators are ignored",
          fs_lookup(fs_root(), "//a///f.txt") == file);
    check("a trailing separator is ignored",
          fs_lookup(fs_root(), "/a/") == dir);
    check("a missing component is not found",
          fs_lookup(fs_root(), "/a/missing") == NULL);
    check("a path cannot continue through a file",
          fs_lookup(fs_root(), "/a/f.txt/x") == NULL);
    check("an empty path is not found", fs_lookup(fs_root(), "") == NULL);

    /* --- paths --- */
    char path[FS_PATH_MAX];
    fs_path(file, path, sizeof(path));
    check_str("fs_path rebuilds an absolute path", path, "/a/f.txt");

    fs_path(fs_root(), path, sizeof(path));
    check_str("fs_path of the root is /", path, "/");

    /* --- removal --- */
    check("a non-empty directory cannot be removed", !fs_unlink(dir));
    check("the root cannot be removed", !fs_unlink(fs_root()));
    check("a file can be removed", fs_unlink(file));
    check("the removed file is gone", fs_lookup(fs_root(), "/a/f.txt") == NULL);
    check("the emptied directory can now be removed", fs_unlink(dir));
    check("the removed directory is gone", fs_lookup(fs_root(), "/a") == NULL);

    /* --- allocation failure --- */
    allocations_fail = 1;
    check("creation fails gracefully when out of memory",
          fs_create(fs_root(), "/oom", FS_DIR) == NULL);

    struct fs_node *victim = fs_lookup(fs_root(), "/home/motd");
    check("a write that cannot grow the file fails",
          !fs_append(victim, "padding padding padding padding padding", 39));
    allocations_fail = 0;
}

/* ------------------------------------------------------------------ */
/* The automation engine                                               */
/* ------------------------------------------------------------------ */

/*
 * Perform one action the way the shell would: record it, then mine.
 *
 * tail_index mirrors the shell's per-command declaration of where
 * free-form text begins; everything from there is joined into a single
 * argument. 0 means the command takes only positional arguments.
 */
static void act_tail(const char *line, int tail_index)
{
    static char  scratch[256];
    static char  joined[INTERN_STRING_MAX];
    char        *argv[16];
    char        *record[EVENT_MAX_ARGS + 1];
    int          argc = 0;
    int          count = 0;

    strlcpy(scratch, line, sizeof(scratch));

    char *p = scratch;
    while (*p && argc < 16) {
        while (*p == ' ')
            *p++ = '\0';
        if (!*p)
            break;
        argv[argc++] = p;
        while (*p && *p != ' ')
            p++;
    }

    if (argc == 0)
        return;

    if (tail_index > 0 && argc > tail_index + 1) {
        for (int i = 0; i < tail_index && count < EVENT_MAX_ARGS; i++)
            record[count++] = argv[i];

        size_t pos = 0;
        for (int i = tail_index; i < argc; i++) {
            size_t len = strlen(argv[i]);
            if (i > tail_index && pos + 1 < sizeof(joined))
                joined[pos++] = ' ';
            for (size_t j = 0; j < len && pos + 1 < sizeof(joined); j++)
                joined[pos++] = argv[i][j];
        }
        joined[pos] = '\0';
        record[count++] = joined;
    } else {
        /* No declared tail: hand the full list to the journal, which
         * joins any overflow losslessly. */
        record[0] = NULL;
    }

    const struct event *event = record[0]
        ? journal_record(record[0], count, record)
        : journal_record(argv[0], argc, argv);

    if (event)
        automate_observe(event);
}

static void act(const char *line)
{
    act_tail(line, 0);
}

static void reset_engine(void)
{
    journal_init();
    automate_init();
}

static const struct proposal *only_proposal(void)
{
    return automate_proposal_count() == 1 ? automate_proposal_at(0) : NULL;
}

static void test_automation(void)
{
    printf("\nautomation engine:\n");

    /* --- a two-step sequence generalises into one parameter --- */
    reset_engine();
    act("mkdir alpha");
    act("cd alpha");
    check("nothing is proposed after one occurrence",
          automate_proposal_count() == 0);

    act("mkdir beta");
    act("cd beta");
    check("nothing is proposed after two occurrences",
          automate_proposal_count() == 0);

    act("mkdir gamma");
    act("cd gamma");

    const struct proposal *p = only_proposal();
    check("a proposal appears on the third repetition", p != NULL);

    if (p) {
        check("the proposal has both steps", p->line_count == 2);
        check_str("the first step is generalised", p->lines[0], "mkdir $1");
        check_str("the second step reuses the same parameter",
                  p->lines[1], "cd $1");
        check("positions that change together share one parameter",
              p->param_count == 1);
        check("the proposal counts the occurrences", p->occurrences == 3);
        check_str("the suggested name is derived from the verbs",
                  p->name, "mkdir-cd");
        check_str("the proposal shows a real example", p->example[0],
                  "mkdir gamma");
    }

    /* --- accepting turns it into a callable command --- */
    const struct automation *a = automate_accept(p ? p->id : 0, NULL);
    check("accepting creates an automation", a != NULL);
    check("the proposal is consumed", automate_proposal_count() == 0);
    check("the automation is findable by name",
          automate_find("mkdir-cd") == a);

    if (a) {
        char  line[AUTOMATION_LINE_MAX];
        char *argv[] = { (char *)"mkdir-cd", (char *)"delta" };

        check("expanding the first line substitutes the argument",
              automate_expand(a, 0, 2, argv, line, sizeof(line)));
        check_str("the substitution is correct", line, "mkdir delta");

        automate_expand(a, 1, 2, argv, line, sizeof(line));
        check_str("the shared parameter expands the same way",
                  line, "cd delta");

        char *no_args[] = { (char *)"mkdir-cd" };
        check("expansion fails when an argument is missing",
              !automate_expand(a, 0, 1, no_args, line, sizeof(line)));
    }

    check("an adopted pattern is not proposed again",
          automate_proposal_count() == 0);

    act("mkdir epsilon");
    act("cd epsilon");
    check("repeating an adopted pattern raises no new proposal",
          automate_proposal_count() == 0);

    /* --- a repeated command keeps what stays fixed --- */
    reset_engine();
    act_tail("append log.txt started the build", 2);
    act_tail("append log.txt tests are green", 2);
    act_tail("append log.txt shipped it", 2);

    p = only_proposal();
    check("a single repeated command can be proposed", p != NULL);

    if (p) {
        check("it is a one-line automation", p->line_count == 1);
        check_str("the unchanging argument stays literal",
                  p->lines[0], "append log.txt $1");
        check("it takes one parameter", p->param_count == 1);
        check_str("the name comes from what it operates on", p->name, "append-log");
        check_str("free text of differing lengths still unifies",
                  p->example[0], "append log.txt shipped it");
    }

    /* --- independently varying positions get separate parameters --- */
    reset_engine();
    act("write a.txt one");
    act("rm a.txt");
    act("write b.txt two");
    act("rm b.txt");
    act("write c.txt three");
    act("rm c.txt");

    p = only_proposal();
    check("a mixed sequence is proposed", p != NULL);

    if (p) {
        check_str("the filename and the text are separate parameters",
                  p->lines[0], "write $1 $2");
        check_str("the filename is reused in the second step",
                  p->lines[1], "rm $1");
        check("two parameters are needed", p->param_count == 2);
    }

    /* --- a bare repeated command is not worth automating --- */
    reset_engine();
    act("ls");
    act("ls");
    act("ls");
    act("ls");
    check("repeating a bare command proposes nothing",
          automate_proposal_count() == 0);

    /* --- periodic windows do not masquerade as longer patterns --- */
    reset_engine();
    act("pwd");
    act("pwd");
    act("pwd");
    act("pwd");
    act("pwd");
    act("pwd");
    check("a command repeated many times proposes nothing",
          automate_proposal_count() == 0);

    /* --- alternating pair: the pair is found, not the quadruple --- */
    reset_engine();
    for (int i = 0; i < 3; i++) {
        act("cd /tmp");
        act("ls");
    }

    p = only_proposal();
    check("an alternating pair is discovered", p != NULL);
    if (p) {
        check("the period is two steps, not four", p->line_count == 2);
        check_str("the fixed path is kept", p->lines[0], "cd /tmp");
    }

    /* --- dismissal is permanent --- */
    reset_engine();
    act("cd /home");
    act("cat readme.txt");
    act("cd /home");
    act("cat readme.txt");
    act("cd /home");
    act("cat readme.txt");

    p = only_proposal();
    check("a proposal is raised before dismissal", p != NULL);
    check("dismissing it succeeds", p && automate_dismiss(p->id));
    check("the proposal is gone", automate_proposal_count() == 0);

    act("cd /home");
    act("cat readme.txt");
    act("cd /home");
    act("cat readme.txt");
    check("a dismissed pattern is not proposed again",
          automate_proposal_count() == 0);

    /* --- announcement happens once --- */
    reset_engine();
    act("mkdir x");
    act("cd x");
    act("mkdir y");
    act("cd y");
    act("mkdir z");
    act("cd z");
    check("a fresh proposal is not announced immediately",
          automate_next_unannounced() == NULL);
    automate_settle();
    check("it is announced once it has settled",
          automate_next_unannounced() != NULL);
    check("it is only announced once",
          automate_next_unannounced() == NULL);

    /* --- forgetting an automation --- */
    reset_engine();
    act("append notes.txt a");
    act("append notes.txt b");
    act("append notes.txt c");
    p = only_proposal();
    if (p)
        automate_accept(p->id, "note");
    check("an automation can be given a chosen name",
          automate_find("note") != NULL);
    check("automations are counted", automate_count() == 1);
    check("forgetting removes it", automate_forget("note"));
    check("it is gone afterwards", automate_find("note") == NULL);
    check("forgetting something unknown fails", !automate_forget("nope"));

    /*
     * --- automations compose ---
     *
     * The engine does not care whether a verb is a built-in or something
     * it taught the user's machine earlier: a repeated run of learned
     * commands is just another pattern. That is the mechanism by which
     * small programs accrete into larger ones without anyone writing
     * software.
     */
    reset_engine();
    for (int i = 0; i < 3; i++) {
        char build[64], ship[64];

        ksnprintf(build, sizeof(build), "project r%d", i);
        ksnprintf(ship, sizeof(ship), "note r%d", i);

        act(build);
        act(ship);
    }

    p = only_proposal();
    check("a sequence of learned commands is itself discovered", p != NULL);

    if (p) {
        check("the composed automation has both steps", p->line_count == 2);
        check_str("the first learned command is generalised",
                  p->lines[0], "project $1");
        check_str("the second reuses the same parameter",
                  p->lines[1], "note $1");
        check("the composition takes one argument", p->param_count == 1);
    }

    /* --- the journal is inspectable --- */
    reset_engine();
    act("echo hello world and some more text here");
    const struct event *last = journal_recent(0);
    check("the action was journalled", last != NULL);

    if (last) {
        char rendered[128];
        journal_format(last, rendered, sizeof(rendered));
        check_str("arguments beyond the limit are joined losslessly",
                  rendered, "echo hello world and some more text here");
    }

    check("the journal counts what it holds", journal_count() == 1);
}

int main(void)
{
    printf("Nebula OS unit tests\n");

    test_string();
    test_printf();
    test_fs();
    test_automation();

    printf("\n%d checks, %d failed\n", checks_run, checks_failed);

    return checks_failed ? 1 : 0;
}
