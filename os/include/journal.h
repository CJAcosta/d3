#ifndef NEBULA_JOURNAL_H
#define NEBULA_JOURNAL_H

#include "types.h"

/*
 * The action journal: a system-wide record of what the user did.
 *
 * This is the observation layer the automation system is built on. It is
 * a kernel service rather than a shell feature on purpose -- anything
 * that performs work on the user's behalf can record it here, and the
 * pattern miner sees one unified stream regardless of where an action
 * came from.
 *
 * Strings are interned, so an event is a handful of integers: comparing
 * two actions is comparing small integers, which is what makes mining
 * the whole history after every action affordable.
 */

#define JOURNAL_CAPACITY   256
#define EVENT_MAX_ARGS     4
#define INTERN_CAPACITY    512
#define INTERN_STRING_MAX  48

/* Interned id 0 is reserved to mean "no string". */
#define INTERN_NONE 0

struct event {
    uint16_t verb;                      /* interned */
    uint16_t args[EVENT_MAX_ARGS];      /* interned, INTERN_NONE-padded */
    uint8_t  argc;
    uint32_t time_ms;
    uint32_t serial;                    /* monotonic, survives ring wrap */
};

void journal_init(void);

/*
 * Intern a string, returning its id. Returns INTERN_NONE if the string
 * is too long or the table is full, which callers must treat as "this
 * action is not recordable" rather than as a valid id.
 */
uint16_t journal_intern(const char *s);
const char *journal_string(uint16_t id);

/*
 * Record one action. Arguments beyond EVENT_MAX_ARGS are joined into the
 * final slot with single spaces, which is lossless here because the
 * shell splits on runs of whitespace -- so replaying the joined form
 * reproduces the original argument list exactly.
 *
 * Returns the recorded event, or NULL if it could not be recorded.
 */
const struct event *journal_record(const char *verb, int argc, char **argv);

/* Number of events currently held (at most JOURNAL_CAPACITY). */
uint32_t journal_count(void);

/* Total events ever recorded, including ones aged out of the ring. */
uint32_t journal_total(void);

/*
 * Events indexed back from the present: 0 is the most recent, 1 the one
 * before it, and so on. Returns NULL once the index runs off the end.
 */
const struct event *journal_recent(uint32_t back);

/* Render an event as the command line that produced it. */
void journal_format(const struct event *event, char *buf, size_t size);

#endif /* NEBULA_JOURNAL_H */
