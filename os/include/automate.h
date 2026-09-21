#ifndef NEBULA_AUTOMATE_H
#define NEBULA_AUTOMATE_H

#include "types.h"
#include "journal.h"

/*
 * The automation engine: the part of the system that watches what you do
 * and writes software for you.
 *
 * It mines the action journal for repeated sequences. A sequence that
 * recurs is not merely replayed -- it is generalised. Argument positions
 * that stay the same across occurrences are kept as literals; positions
 * that change become parameters, and positions that always change
 * *together* become the same parameter. That generalisation step is the
 * difference between a macro recorder and a system that programs itself:
 * the result is a small program with a signature, not a keystroke dump.
 *
 * Once a pattern has recurred enough times the engine raises a proposal.
 * Accepting it turns the pattern into a named command that is invoked
 * exactly like a built-in. Automations are themselves recorded when they
 * run, so automations built out of automations get discovered too.
 */

#define PATTERN_MAX_LEN      4      /* longest sequence we will mine */
#define PATTERN_TABLE_SIZE   64
#define AUTOMATION_THRESHOLD 3      /* proposals appear on the third repeat */
#define AUTOMATION_MAX_PARAMS 6
#define AUTOMATION_NAME_MAX  24
#define AUTOMATION_LINE_MAX  128
#define AUTOMATION_MAX       32
#define PROPOSAL_MAX         8

struct proposal {
    uint16_t id;
    uint16_t pattern;                    /* index into the pattern table */
    char     name[AUTOMATION_NAME_MAX];  /* suggested, user may override */
    char     lines[PATTERN_MAX_LEN][AUTOMATION_LINE_MAX];
    char     example[PATTERN_MAX_LEN][AUTOMATION_LINE_MAX];
    uint8_t  line_count;
    uint8_t  param_count;
    uint16_t occurrences;
    bool     settled;       /* has survived one further action */
    bool     announced;
    bool     live;
};

struct automation {
    bool     used;
    char     name[AUTOMATION_NAME_MAX];
    char     lines[PATTERN_MAX_LEN][AUTOMATION_LINE_MAX];
    uint8_t  line_count;
    uint8_t  param_count;
    uint32_t run_count;
    uint16_t learned_from;               /* occurrences that triggered it */
};

void automate_init(void);

/* Feed one observed action to the miner. */
void automate_observe(const struct event *event);

/* Proposals awaiting a decision. */
uint32_t               automate_proposal_count(void);
const struct proposal *automate_proposal_at(uint32_t index);
const struct proposal *automate_proposal_by_id(uint16_t id);

/*
 * The next settled proposal the user has not been told about, marking it
 * announced. Returns NULL when there is nothing new to say -- each
 * discovery is mentioned exactly once.
 */
const struct proposal *automate_next_unannounced(void);

/*
 * Mark the current proposals as settled, meaning they may now be
 * announced. Call it once at the end of each action.
 *
 * A shorter pattern crosses the threshold one action before the longer
 * pattern that contains it, so the engine's first idea is routinely
 * superseded by a better one on the very next action. Letting a proposal
 * settle for one action means the improvement replaces it silently,
 * instead of the user being shown a suggestion and then a correction.
 */
void automate_settle(void);

/*
 * Turn a proposal into a command. `name` may be NULL to take the
 * suggested one. Returns NULL if the id is unknown, the name is already
 * taken, or there is no room left.
 */
const struct automation *automate_accept(uint16_t id, const char *name);
bool automate_dismiss(uint16_t id);

/* The automations that exist now. */
uint32_t                 automate_count(void);
const struct automation *automate_at(uint32_t index);
const struct automation *automate_find(const char *name);
bool                     automate_forget(const char *name);

/*
 * Expand one line of an automation, substituting $1..$9 from argv (where
 * argv[0] is the automation's own name). Returns false if the line
 * references a parameter the caller did not supply.
 */
bool automate_expand(const struct automation *automation, uint8_t line,
                     int argc, char **argv, char *buf, size_t size);

void automate_note_run(const struct automation *automation);

/*
 * Introspection: what the engine is currently watching, most-repeated
 * first. Lets the user see the mechanism rather than be surprised by it.
 */
uint32_t automate_watch_count(void);
bool     automate_watch_at(uint32_t rank, char *buf, size_t size,
                           uint16_t *occurrences, bool *proposable);

#endif /* NEBULA_AUTOMATE_H */
