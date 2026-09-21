/*
 * journal.c -- the action journal and its string intern table.
 */

#include "journal.h"
#include "string.h"
#include "kprintf.h"
#include "timer.h"

static char     intern_table[INTERN_CAPACITY][INTERN_STRING_MAX];
static uint16_t intern_used;

static struct event ring[JOURNAL_CAPACITY];
static uint32_t     ring_head;      /* next slot to write */
static uint32_t     ring_count;     /* live entries, <= JOURNAL_CAPACITY */
static uint32_t     total_recorded;

void journal_init(void)
{
    intern_used    = 0;
    ring_head      = 0;
    ring_count     = 0;
    total_recorded = 0;

    /* Slot 0 is the reserved "no string" id, so the table starts at 1. */
    intern_table[0][0] = '\0';
    intern_used = 1;
}

uint16_t journal_intern(const char *s)
{
    if (!s || !*s)
        return INTERN_NONE;

    if (strlen(s) >= INTERN_STRING_MAX)
        return INTERN_NONE;

    for (uint16_t i = 1; i < intern_used; i++) {
        if (strcmp(intern_table[i], s) == 0)
            return i;
    }

    if (intern_used >= INTERN_CAPACITY)
        return INTERN_NONE;

    strlcpy(intern_table[intern_used], s, INTERN_STRING_MAX);

    return intern_used++;
}

const char *journal_string(uint16_t id)
{
    if (id == INTERN_NONE || id >= intern_used)
        return "";

    return intern_table[id];
}

uint32_t journal_count(void)
{
    return ring_count;
}

uint32_t journal_total(void)
{
    return total_recorded;
}

const struct event *journal_recent(uint32_t back)
{
    if (back >= ring_count)
        return NULL;

    /* ring_head points one past the newest entry. */
    uint32_t index = (ring_head + JOURNAL_CAPACITY - 1 - back) % JOURNAL_CAPACITY;

    return &ring[index];
}

const struct event *journal_record(const char *verb, int argc, char **argv)
{
    uint16_t verb_id = journal_intern(verb);

    /* An unrecordable verb means we cannot describe this action, so we
     * must not record a partial version of it. */
    if (verb_id == INTERN_NONE)
        return NULL;

    struct event *event = &ring[ring_head];

    memset(event, 0, sizeof(*event));
    event->verb    = verb_id;
    event->time_ms = timer_uptime_ms();
    event->serial  = total_recorded;

    int slots = 0;

    for (int i = 1; i < argc && slots < EVENT_MAX_ARGS; i++) {
        bool last_slot = (slots == EVENT_MAX_ARGS - 1);
        bool more_left = (i + 1 < argc);

        if (last_slot && more_left) {
            /*
             * Out of slots with arguments still to go: join the rest
             * into this one. The shell collapses runs of whitespace when
             * it tokenises, so re-splitting this on single spaces yields
             * the original arguments.
             */
            char joined[INTERN_STRING_MAX];
            size_t pos = 0;

            for (int j = i; j < argc; j++) {
                size_t len = strlen(argv[j]);

                if (j > i && pos + 1 < sizeof(joined))
                    joined[pos++] = ' ';

                for (size_t k = 0; k < len && pos + 1 < sizeof(joined); k++)
                    joined[pos++] = argv[j][k];
            }

            joined[pos] = '\0';
            event->args[slots] = journal_intern(joined);
        } else {
            event->args[slots] = journal_intern(argv[i]);
        }

        /* A string we could not intern would silently change the meaning
         * of the action, so drop the whole event instead. */
        if (event->args[slots] == INTERN_NONE)
            return NULL;

        slots++;
    }

    event->argc = (uint8_t)slots;

    ring_head = (ring_head + 1) % JOURNAL_CAPACITY;
    if (ring_count < JOURNAL_CAPACITY)
        ring_count++;
    total_recorded++;

    /* The entry we just filled in, now one slot behind the head. */
    return journal_recent(0);
}

void journal_format(const struct event *event, char *buf, size_t size)
{
    if (!event || size == 0)
        return;

    size_t pos = strlcpy(buf, journal_string(event->verb), size);

    if (pos >= size)
        pos = size - 1;

    for (uint8_t i = 0; i < event->argc; i++) {
        const char *arg = journal_string(event->args[i]);
        size_t      len = strlen(arg);

        if (pos + 1 + len + 1 > size)
            break;

        buf[pos++] = ' ';
        memcpy(buf + pos, arg, len);
        pos += len;
    }

    buf[pos] = '\0';
}
