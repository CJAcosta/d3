/*
 * automate.c -- mining repeated actions into reusable programs.
 *
 * After every recorded action the miner looks at the trailing window of
 * the journal at each length from 1 to PATTERN_MAX_LEN, and folds each
 * window into a table of patterns. Two windows belong to the same
 * pattern when their verbs and argument counts line up; the arguments
 * themselves are then *unified* rather than compared:
 *
 *   mkdir alpha ; cd alpha          first occurrence: all literal
 *   mkdir beta  ; cd beta           position 1 and 2 both changed, and
 *                                   changed to the same value
 *   mkdir gamma ; cd gamma          -> one parameter used twice
 *
 * yielding `mkdir $1 ; cd $1`, a program with a signature, from three
 * ordinary actions.
 *
 * Two refinements stop the obvious false positives:
 *
 *   - A periodic pattern is ignored. Typing `ls` four times makes the
 *     window `ls ls` look like a repeated pair, but the real unit of
 *     repetition is `ls`, so patterns that are a shorter pattern
 *     repeated are never proposed.
 *
 *   - When a pattern is proposed, the shorter patterns contained inside
 *     it are held back. Discovering `mkdir $1 ; cd $1` should not also
 *     produce separate proposals for `mkdir $1` and `cd $1`. They can
 *     return later if they start recurring on their own.
 */

#include "automate.h"
#include "journal.h"
#include "string.h"
#include "kprintf.h"
#include "timer.h"

#define SLOT_COUNT (PATTERN_MAX_LEN * EVENT_MAX_ARGS)
#define SLOT(step, arg) ((step) * EVENT_MAX_ARGS + (arg))

struct pattern_step {
    uint16_t verb;
    uint8_t  argc;
    uint16_t args[EVENT_MAX_ARGS];      /* the literal, while it holds */
    bool     varies[EVENT_MAX_ARGS];    /* has this position ever changed? */
};

struct pattern {
    bool     used;
    uint8_t  length;
    struct pattern_step steps[PATTERN_MAX_LEN];

    /*
     * Unification. link[i] is the earliest slot that slot i has been
     * equal to in every occurrence so far; a slot that has never matched
     * an earlier one links to itself. This is what collapses the two
     * positions of `mkdir $1 ; cd $1` into a single parameter.
     */
    uint8_t  link[SLOT_COUNT];
    uint16_t last_values[SLOT_COUNT];

    uint16_t count;
    uint32_t last_seen_ms;
    uint32_t last_serial;       /* serial of the newest event that matched */

    bool     proposed;
    bool     dismissed;
    bool     adopted;
    uint16_t hold_until;                /* re-propose only past this count */
};

static struct pattern    patterns[PATTERN_TABLE_SIZE];
static struct proposal   proposals[PROPOSAL_MAX];
static struct automation automations[AUTOMATION_MAX];
static uint16_t          next_proposal_id = 1;

/* ------------------------------------------------------------------ */
/* Pattern table                                                       */
/* ------------------------------------------------------------------ */

static bool steps_structurally_equal(const struct pattern_step *a,
                                     const struct pattern_step *b)
{
    if (a->verb != b->verb || a->argc != b->argc)
        return false;

    for (uint8_t i = 0; i < a->argc; i++) {
        if (a->varies[i] != b->varies[i])
            return false;
        if (!a->varies[i] && a->args[i] != b->args[i])
            return false;
    }

    return true;
}

/*
 * True if the pattern is just a shorter pattern seen through a sliding
 * window, in which case the shorter one is the real unit of repetition.
 *
 * This is string periodicity, and the period deliberately does not have
 * to divide the length. Alternating `cd` and `ls` produces the window
 * `cd ls cd`, which is not two whole copies of anything but is still an
 * artifact of the period-2 pattern underneath it. Requiring divisibility
 * catches `A A` and misses `A B A`.
 */
static bool pattern_is_periodic(const struct pattern *pattern)
{
    for (uint8_t period = 1; period < pattern->length; period++) {
        bool repeats = true;
        for (uint8_t i = period; i < pattern->length && repeats; i++) {
            if (!steps_structurally_equal(&pattern->steps[i],
                                          &pattern->steps[i - period]))
                repeats = false;
        }

        if (repeats)
            return true;
    }

    return false;
}

/*
 * True if the two patterns are the same cycle seen from different
 * starting points.
 *
 * A stream of `mkdir` and `cd` contains `cd ; mkdir` exactly as often as
 * `mkdir ; cd`, because a sliding window has no idea where the user
 * thinks the unit of work begins. Once one of them is chosen, its
 * rotations are the same discovery wearing a different hat.
 */
static bool pattern_is_rotation_of(const struct pattern *a,
                                   const struct pattern *b)
{
    if (a->length != b->length || a->length < 2)
        return false;

    for (uint8_t offset = 0; offset < a->length; offset++) {
        bool all = true;

        for (uint8_t i = 0; i < a->length && all; i++) {
            if (!steps_structurally_equal(&a->steps[(i + offset) % a->length],
                                          &b->steps[i]))
                all = false;
        }

        if (all)
            return true;
    }

    return false;
}

/* True if `inner` appears as a contiguous run of steps inside `outer`. */
static bool pattern_contains(const struct pattern *outer,
                             const struct pattern *inner)
{
    if (inner->length > outer->length)
        return false;

    for (uint8_t start = 0; start + inner->length <= outer->length; start++) {
        bool all = true;

        for (uint8_t i = 0; i < inner->length && all; i++) {
            if (!steps_structurally_equal(&outer->steps[start + i],
                                          &inner->steps[i]))
                all = false;
        }

        if (all)
            return true;
    }

    return false;
}

/*
 * Does this window match the pattern's shape? Verbs and argument counts
 * must agree exactly, and a position the pattern still holds as a
 * literal must either match that literal or be allowed to generalise
 * into a parameter -- which is always allowed, and is how patterns
 * become programs.
 */
static bool window_matches(const struct pattern *pattern,
                           const struct event *const *window, uint8_t length)
{
    if (pattern->length != length)
        return false;

    for (uint8_t i = 0; i < length; i++) {
        if (pattern->steps[i].verb != window[i]->verb)
            return false;
        if (pattern->steps[i].argc != window[i]->argc)
            return false;
    }

    return true;
}

/* Reclaim the least valuable slot when the table is full: lowest count,
 * oldest as the tie-break. Adopted and proposed patterns are kept. */
static struct pattern *pattern_evict(void)
{
    struct pattern *victim = NULL;

    for (uint32_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
        struct pattern *candidate = &patterns[i];

        if (candidate->adopted || candidate->proposed)
            continue;

        if (!victim ||
            candidate->count < victim->count ||
            (candidate->count == victim->count &&
             candidate->last_seen_ms < victim->last_seen_ms))
            victim = candidate;
    }

    if (victim)
        memset(victim, 0, sizeof(*victim));

    return victim;
}

static struct pattern *pattern_insert(const struct event *const *window,
                                      uint8_t length)
{
    struct pattern *pattern = NULL;

    for (uint32_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
        if (!patterns[i].used) {
            pattern = &patterns[i];
            break;
        }
    }

    if (!pattern)
        pattern = pattern_evict();

    if (!pattern)
        return NULL;

    memset(pattern, 0, sizeof(*pattern));
    pattern->used   = true;
    pattern->length = length;

    for (uint8_t i = 0; i < length; i++) {
        pattern->steps[i].verb = window[i]->verb;
        pattern->steps[i].argc = window[i]->argc;

        for (uint8_t a = 0; a < window[i]->argc; a++) {
            pattern->steps[i].args[a]   = window[i]->args[a];
            pattern->steps[i].varies[a] = false;
            pattern->last_values[SLOT(i, a)] = window[i]->args[a];
        }
    }

    /*
     * Seed the unification links from the first occurrence: two
     * positions that already hold the same value are candidates for
     * being the same parameter, and later occurrences can only break
     * that link, never create it.
     */
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++) {
        pattern->link[slot] = slot;

        for (uint8_t earlier = 0; earlier < slot; earlier++) {
            uint8_t step_a = slot / EVENT_MAX_ARGS, arg_a = slot % EVENT_MAX_ARGS;
            uint8_t step_b = earlier / EVENT_MAX_ARGS, arg_b = earlier % EVENT_MAX_ARGS;

            if (step_a >= length || arg_a >= pattern->steps[step_a].argc)
                continue;
            if (step_b >= length || arg_b >= pattern->steps[step_b].argc)
                continue;

            if (pattern->last_values[slot] == pattern->last_values[earlier]) {
                pattern->link[slot] = earlier;
                break;
            }
        }
    }

    pattern->count = 1;
    pattern->last_seen_ms = timer_uptime_ms();
    pattern->last_serial  = window[length - 1]->serial;

    return pattern;
}

/* Fold another occurrence into an existing pattern. */
static void pattern_absorb(struct pattern *pattern,
                           const struct event *const *window)
{
    uint16_t values[SLOT_COUNT];

    memset(values, 0, sizeof(values));

    for (uint8_t i = 0; i < pattern->length; i++) {
        for (uint8_t a = 0; a < pattern->steps[i].argc; a++) {
            uint16_t value = window[i]->args[a];

            values[SLOT(i, a)] = value;

            /* A literal that just changed is not a literal after all. */
            if (!pattern->steps[i].varies[a] &&
                pattern->steps[i].args[a] != value)
                pattern->steps[i].varies[a] = true;
        }
    }

    /* Break any unification that this occurrence contradicts. */
    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++) {
        uint8_t root = pattern->link[slot];

        if (root != slot && values[slot] != values[root])
            pattern->link[slot] = slot;
    }

    memcpy(pattern->last_values, values, sizeof(values));

    pattern->count++;
    pattern->last_seen_ms = timer_uptime_ms();
    pattern->last_serial  = window[pattern->length - 1]->serial;
}

/* ------------------------------------------------------------------ */
/* Turning a pattern into a program                                    */
/* ------------------------------------------------------------------ */

/*
 * Assign parameter numbers. Slots that are still literal keep their
 * value; slots that vary take the number of their unification root, so
 * positions that always moved together share one parameter.
 *
 * Returns the number of distinct parameters, or -1 if the pattern needs
 * more than we allow.
 */
static int assign_parameters(const struct pattern *pattern, uint8_t *param_of)
{
    uint8_t root_param[SLOT_COUNT];
    int     count = 0;

    memset(root_param, 0, sizeof(root_param));

    for (uint8_t slot = 0; slot < SLOT_COUNT; slot++)
        param_of[slot] = 0;

    for (uint8_t i = 0; i < pattern->length; i++) {
        for (uint8_t a = 0; a < pattern->steps[i].argc; a++) {
            uint8_t slot = SLOT(i, a);

            if (!pattern->steps[i].varies[a])
                continue;

            uint8_t root = pattern->link[slot];

            /*
             * A slot may link to one that turned out to be literal; in
             * that case it is its own root for parameter purposes.
             */
            uint8_t root_step = root / EVENT_MAX_ARGS;
            uint8_t root_arg  = root % EVENT_MAX_ARGS;

            if (root != slot && !pattern->steps[root_step].varies[root_arg])
                root = slot;

            if (root_param[root] == 0) {
                if (count >= AUTOMATION_MAX_PARAMS)
                    return -1;
                root_param[root] = (uint8_t)(++count);
            }

            param_of[slot] = root_param[root];
        }
    }

    return count;
}

/* Render one step as a command line, with parameters as $1, $2, ... */
static void render_line(const struct pattern *pattern, uint8_t step,
                        const uint8_t *param_of, char *buf, size_t size)
{
    size_t pos = strlcpy(buf, journal_string(pattern->steps[step].verb), size);

    if (pos >= size)
        pos = size ? size - 1 : 0;

    for (uint8_t a = 0; a < pattern->steps[step].argc; a++) {
        uint8_t slot = SLOT(step, a);
        char    piece[INTERN_STRING_MAX + 4];

        if (pattern->steps[step].varies[a])
            ksnprintf(piece, sizeof(piece), "$%u", param_of[slot]);
        else
            strlcpy(piece, journal_string(pattern->steps[step].args[a]),
                    sizeof(piece));

        size_t len = strlen(piece);
        if (pos + 1 + len + 1 > size)
            break;

        buf[pos++] = ' ';
        memcpy(buf + pos, piece, len);
        pos += len;
    }

    buf[pos] = '\0';
}

/* Render the most recent occurrence, so a proposal can show real input. */
static void render_example(const struct pattern *pattern, uint8_t step,
                           char *buf, size_t size)
{
    size_t pos = strlcpy(buf, journal_string(pattern->steps[step].verb), size);

    if (pos >= size)
        pos = size ? size - 1 : 0;

    for (uint8_t a = 0; a < pattern->steps[step].argc; a++) {
        const char *value = journal_string(pattern->last_values[SLOT(step, a)]);
        size_t      len   = strlen(value);

        if (pos + 1 + len + 1 > size)
            break;

        buf[pos++] = ' ';
        memcpy(buf + pos, value, len);
        pos += len;
    }

    buf[pos] = '\0';
}

/* ------------------------------------------------------------------ */
/* Naming                                                              */
/* ------------------------------------------------------------------ */

static bool name_taken(const char *name)
{
    for (uint32_t i = 0; i < AUTOMATION_MAX; i++) {
        if (automations[i].used && strcmp(automations[i].name, name) == 0)
            return true;
    }

    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (proposals[i].live && strcmp(proposals[i].name, name) == 0)
            return true;
    }

    return false;
}

/* Append a token, keeping the result to [a-z0-9-]. */
static size_t append_sanitised(char *buf, size_t pos, size_t size,
                               const char *token, size_t max_token)
{
    size_t taken = 0;

    for (const char *p = token; *p && taken < max_token && pos + 1 < size; p++) {
        char c = *p;

        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');

        /* A path turns into its last component, which reads better. */
        if (c == '/') {
            pos -= taken;
            taken = 0;
            continue;
        }

        if (c == '.')
            break;      /* drop an extension */

        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')))
            continue;

        buf[pos++] = c;
        taken++;
    }

    buf[pos] = '\0';
    return pos;
}

static void suggest_name(const struct pattern *pattern, char *buf, size_t size)
{
    /*
     * Built in a local buffer, not in `buf`. `buf` is usually the name
     * field of the very proposal being filled in, and name_taken() scans
     * the live proposals -- so writing directly into it would have the
     * candidate compared against itself and always found taken.
     */
    char   candidate[AUTOMATION_NAME_MAX];
    size_t pos = 0;

    candidate[0] = '\0';

    if (pattern->length == 1) {
        /*
         * A single repeated command is best named for the thing it keeps
         * operating on: `append log.txt $1` reads as `log`, not `append`.
         */
        pos = append_sanitised(candidate, pos, sizeof(candidate),
                               journal_string(pattern->steps[0].verb), 8);

        for (uint8_t a = 0; a < pattern->steps[0].argc; a++) {
            if (pattern->steps[0].varies[a])
                continue;

            if (pos + 1 < sizeof(candidate))
                candidate[pos++] = '-';

            pos = append_sanitised(candidate, pos, sizeof(candidate),
                                   journal_string(pattern->steps[0].args[a]), 10);
            break;
        }
    } else {
        for (uint8_t i = 0; i < pattern->length && i < 2; i++) {
            if (i > 0 && pos + 1 < sizeof(candidate))
                candidate[pos++] = '-';

            pos = append_sanitised(candidate, pos, sizeof(candidate),
                                   journal_string(pattern->steps[i].verb), 8);
        }
    }

    if (pos == 0)
        strlcpy(candidate, "task", sizeof(candidate));

    if (!name_taken(candidate)) {
        strlcpy(buf, candidate, size);
        return;
    }

    char base[AUTOMATION_NAME_MAX];
    strlcpy(base, candidate, sizeof(base));

    for (int suffix = 2; suffix < 100; suffix++) {
        ksnprintf(candidate, sizeof(candidate), "%s%d", base, suffix);
        if (!name_taken(candidate))
            break;
    }

    strlcpy(buf, candidate, size);
}

/* ------------------------------------------------------------------ */
/* Proposals                                                           */
/* ------------------------------------------------------------------ */

static bool pattern_is_proposable(const struct pattern *pattern)
{
    if (!pattern->used || pattern->dismissed || pattern->adopted ||
        pattern->proposed)
        return false;

    if (pattern->count < AUTOMATION_THRESHOLD)
        return false;

    if (pattern->count <= pattern->hold_until)
        return false;

    if (pattern_is_periodic(pattern))
        return false;

    /*
     * A single command with nothing fixed about it is just the command.
     * There has to be a literal for the automation to be worth more than
     * typing it out.
     */
    if (pattern->length == 1) {
        bool has_literal = false;

        for (uint8_t a = 0; a < pattern->steps[0].argc; a++) {
            if (!pattern->steps[0].varies[a])
                has_literal = true;
        }

        if (!has_literal)
            return false;
    }

    return true;
}

static struct proposal *proposal_slot(void)
{
    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (!proposals[i].live)
            return &proposals[i];
    }

    /* All slots busy: drop the oldest announced one to make room. */
    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (proposals[i].announced) {
            if (proposals[i].pattern < PATTERN_TABLE_SIZE)
                patterns[proposals[i].pattern].proposed = false;
            memset(&proposals[i], 0, sizeof(proposals[i]));
            return &proposals[i];
        }
    }

    return NULL;
}

/*
 * Withdraw a live proposal that a better one supersedes.
 *
 * A shorter pattern reaches the threshold first: by the third `cd /tmp`
 * the engine has seen `cd /tmp` three times but `cd /tmp ; ls` only
 * twice, so the weaker suggestion is raised one action before the
 * stronger one exists. When the longer pattern arrives and contains the
 * shorter, the earlier suggestion is retracted rather than left standing
 * next to its own improvement.
 */
static void withdraw_superseded(const struct pattern *winner,
                                uint16_t winner_index)
{
    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (!proposals[i].live || proposals[i].pattern == winner_index)
            continue;

        /*
         * Never retract something already said out loud. The user may be
         * about to type `accept 1`, and having the id vanish underneath
         * them is worse than carrying one redundant suggestion.
         */
        if (proposals[i].announced)
            continue;

        if (proposals[i].pattern >= PATTERN_TABLE_SIZE)
            continue;

        struct pattern *older = &patterns[proposals[i].pattern];

        if (older->length >= winner->length || older->count > winner->count)
            continue;

        if (!pattern_contains(winner, older))
            continue;

        older->proposed   = false;
        older->hold_until = (uint16_t)(older->count + AUTOMATION_THRESHOLD);

        memset(&proposals[i], 0, sizeof(proposals[i]));
    }
}

/*
 * Hold back the patterns that are really the same discovery: the shorter
 * runs contained inside the one just proposed, and its rotations.
 *
 * The hold is not permanent. If one of them genuinely keeps recurring on
 * its own -- `cd /tmp` on its own, long after `cd /tmp ; ls` was
 * adopted -- it climbs past the hold and can be proposed in its own
 * right.
 */
static void suppress_related(const struct pattern *winner)
{
    for (uint32_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
        struct pattern *other = &patterns[i];

        if (!other->used || other == winner || other->adopted || other->proposed)
            continue;

        if (other->count > winner->count)
            continue;   /* it recurs more often than the winner; leave it */

        bool related = (other->length < winner->length &&
                        pattern_contains(winner, other)) ||
                       pattern_is_rotation_of(winner, other);

        if (related)
            other->hold_until = (uint16_t)(other->count + AUTOMATION_THRESHOLD);
    }
}

static void consider_proposals(void)
{
    struct pattern *winner = NULL;
    uint32_t        winner_index = 0;

    for (uint32_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
        if (!pattern_is_proposable(&patterns[i]))
            continue;

        /*
         * Prefer the longest sequence, then the most repeated, then the
         * one that just finished. That last tie-break is what picks
         * `mkdir ; cd` over its rotation `cd ; mkdir`: both have
         * recurred equally often, but only one of them is the thing the
         * user has this moment finished doing.
         */
        bool better = !winner ||
                      patterns[i].length > winner->length ||
                      (patterns[i].length == winner->length &&
                       patterns[i].count > winner->count) ||
                      (patterns[i].length == winner->length &&
                       patterns[i].count == winner->count &&
                       patterns[i].last_serial > winner->last_serial);

        if (better) {
            winner = &patterns[i];
            winner_index = i;
        }
    }

    if (!winner)
        return;

    uint8_t param_of[SLOT_COUNT];
    int params = assign_parameters(winner, param_of);

    if (params < 0) {
        /* Too many moving parts to be a useful command. */
        winner->dismissed = true;
        return;
    }

    struct proposal *proposal = proposal_slot();
    if (!proposal)
        return;

    memset(proposal, 0, sizeof(*proposal));

    proposal->id          = next_proposal_id++;
    proposal->pattern     = (uint16_t)winner_index;
    proposal->line_count  = winner->length;
    proposal->param_count = (uint8_t)params;
    proposal->occurrences = winner->count;
    proposal->live        = true;

    for (uint8_t i = 0; i < winner->length; i++) {
        render_line(winner, i, param_of,
                    proposal->lines[i], AUTOMATION_LINE_MAX);
        render_example(winner, i, proposal->example[i], AUTOMATION_LINE_MAX);
    }

    winner->proposed = true;

    /*
     * Retire the superseded suggestions first. They are about to stop
     * existing, and until they do they hold names this proposal has a
     * better claim to -- naming before retiring produced `mkdir-cd2`
     * whose only rival was the draft it replaced.
     */
    withdraw_superseded(winner, (uint16_t)winner_index);

    suggest_name(winner, proposal->name, AUTOMATION_NAME_MAX);

    suppress_related(winner);
}

/* ------------------------------------------------------------------ */
/* Public interface                                                    */
/* ------------------------------------------------------------------ */

void automate_init(void)
{
    memset(patterns, 0, sizeof(patterns));
    memset(proposals, 0, sizeof(proposals));
    memset(automations, 0, sizeof(automations));
    next_proposal_id = 1;
}

void automate_observe(const struct event *event)
{
    if (!event)
        return;

    for (uint8_t length = 1; length <= PATTERN_MAX_LEN; length++) {
        const struct event *window[PATTERN_MAX_LEN];

        if (journal_count() < length)
            break;

        /* window[0] is the oldest event of the trailing run. */
        bool complete = true;
        for (uint8_t i = 0; i < length; i++) {
            window[i] = journal_recent((uint32_t)(length - 1 - i));
            if (!window[i])
                complete = false;
        }

        if (!complete)
            break;

        struct pattern *match = NULL;

        for (uint32_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
            if (patterns[i].used && window_matches(&patterns[i], window, length)) {
                match = &patterns[i];
                break;
            }
        }

        if (match)
            pattern_absorb(match, window);
        else
            pattern_insert(window, length);
    }

    consider_proposals();
}

uint32_t automate_proposal_count(void)
{
    uint32_t count = 0;

    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (proposals[i].live)
            count++;
    }

    return count;
}

const struct proposal *automate_proposal_at(uint32_t index)
{
    uint32_t seen = 0;

    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (!proposals[i].live)
            continue;
        if (seen++ == index)
            return &proposals[i];
    }

    return NULL;
}

const struct proposal *automate_proposal_by_id(uint16_t id)
{
    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (proposals[i].live && proposals[i].id == id)
            return &proposals[i];
    }

    return NULL;
}

const struct proposal *automate_next_unannounced(void)
{
    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (proposals[i].live && proposals[i].settled &&
            !proposals[i].announced) {
            proposals[i].announced = true;
            return &proposals[i];
        }
    }

    return NULL;
}

void automate_settle(void)
{
    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (proposals[i].live)
            proposals[i].settled = true;
    }
}

const struct automation *automate_accept(uint16_t id, const char *name)
{
    struct proposal *proposal = NULL;

    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (proposals[i].live && proposals[i].id == id)
            proposal = &proposals[i];
    }

    if (!proposal)
        return NULL;

    const char *chosen = (name && *name) ? name : proposal->name;

    if (strlen(chosen) >= AUTOMATION_NAME_MAX)
        return NULL;

    if (name_taken(chosen) && strcmp(chosen, proposal->name) != 0)
        return NULL;

    for (uint32_t i = 0; i < AUTOMATION_MAX; i++) {
        if (automations[i].used && strcmp(automations[i].name, chosen) == 0)
            return NULL;
    }

    struct automation *automation = NULL;
    for (uint32_t i = 0; i < AUTOMATION_MAX; i++) {
        if (!automations[i].used) {
            automation = &automations[i];
            break;
        }
    }

    if (!automation)
        return NULL;

    memset(automation, 0, sizeof(*automation));
    automation->used         = true;
    automation->line_count   = proposal->line_count;
    automation->param_count  = proposal->param_count;
    automation->learned_from = proposal->occurrences;
    strlcpy(automation->name, chosen, AUTOMATION_NAME_MAX);

    for (uint8_t i = 0; i < proposal->line_count; i++)
        strlcpy(automation->lines[i], proposal->lines[i], AUTOMATION_LINE_MAX);

    if (proposal->pattern < PATTERN_TABLE_SIZE) {
        patterns[proposal->pattern].adopted  = true;
        patterns[proposal->pattern].proposed = false;
    }

    memset(proposal, 0, sizeof(*proposal));

    return automation;
}

bool automate_dismiss(uint16_t id)
{
    for (uint32_t i = 0; i < PROPOSAL_MAX; i++) {
        if (!proposals[i].live || proposals[i].id != id)
            continue;

        if (proposals[i].pattern < PATTERN_TABLE_SIZE) {
            /* Dismissed for good: the user has said this is not worth
             * automating, and repeating the suggestion is nagging. */
            patterns[proposals[i].pattern].dismissed = true;
            patterns[proposals[i].pattern].proposed  = false;
        }

        memset(&proposals[i], 0, sizeof(proposals[i]));
        return true;
    }

    return false;
}

uint32_t automate_count(void)
{
    uint32_t count = 0;

    for (uint32_t i = 0; i < AUTOMATION_MAX; i++) {
        if (automations[i].used)
            count++;
    }

    return count;
}

const struct automation *automate_at(uint32_t index)
{
    uint32_t seen = 0;

    for (uint32_t i = 0; i < AUTOMATION_MAX; i++) {
        if (!automations[i].used)
            continue;
        if (seen++ == index)
            return &automations[i];
    }

    return NULL;
}

const struct automation *automate_find(const char *name)
{
    if (!name || !*name)
        return NULL;

    for (uint32_t i = 0; i < AUTOMATION_MAX; i++) {
        if (automations[i].used && strcmp(automations[i].name, name) == 0)
            return &automations[i];
    }

    return NULL;
}

bool automate_forget(const char *name)
{
    for (uint32_t i = 0; i < AUTOMATION_MAX; i++) {
        if (automations[i].used && strcmp(automations[i].name, name) == 0) {
            memset(&automations[i], 0, sizeof(automations[i]));
            return true;
        }
    }

    return false;
}

void automate_note_run(const struct automation *automation)
{
    /* The registry owns these, so casting away const is safe here. */
    struct automation *mutable_automation = (struct automation *)automation;

    if (mutable_automation)
        mutable_automation->run_count++;
}

bool automate_expand(const struct automation *automation, uint8_t line,
                     int argc, char **argv, char *buf, size_t size)
{
    if (!automation || line >= automation->line_count || size == 0)
        return false;

    const char *src = automation->lines[line];
    size_t      pos = 0;

    while (*src && pos + 1 < size) {
        if (src[0] == '$' && src[1] >= '1' && src[1] <= '9') {
            int index = src[1] - '0';

            if (index >= argc) {
                buf[pos] = '\0';
                return false;       /* the caller did not supply it */
            }

            const char *value = argv[index];
            size_t      len   = strlen(value);

            if (pos + len + 1 > size) {
                buf[pos] = '\0';
                return false;
            }

            memcpy(buf + pos, value, len);
            pos += len;
            src += 2;
            continue;
        }

        buf[pos++] = *src++;
    }

    buf[pos] = '\0';

    return *src == '\0';
}

uint32_t automate_watch_count(void)
{
    uint32_t count = 0;

    for (uint32_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
        if (patterns[i].used && patterns[i].count > 1 && !patterns[i].adopted)
            count++;
    }

    return count;
}

bool automate_watch_at(uint32_t rank, char *buf, size_t size,
                       uint16_t *occurrences, bool *proposable)
{
    const struct pattern *ranked[PATTERN_TABLE_SIZE];
    uint32_t              found = 0;

    for (uint32_t i = 0; i < PATTERN_TABLE_SIZE; i++) {
        const struct pattern *p = &patterns[i];

        /* Only what has actually recurred is interesting to show. */
        if (p->used && p->count > 1 && !p->adopted)
            ranked[found++] = p;
    }

    if (rank >= found)
        return false;

    /* Most repeated first, longest as the tie-break. Selection sort is
     * more than adequate for a table this small. */
    for (uint32_t i = 0; i < found; i++) {
        uint32_t best = i;

        for (uint32_t j = i + 1; j < found; j++) {
            if (ranked[j]->count > ranked[best]->count ||
                (ranked[j]->count == ranked[best]->count &&
                 ranked[j]->length > ranked[best]->length))
                best = j;
        }

        const struct pattern *swap = ranked[i];
        ranked[i] = ranked[best];
        ranked[best] = swap;
    }

    const struct pattern *chosen = ranked[rank];

    uint8_t param_of[SLOT_COUNT];
    if (assign_parameters(chosen, param_of) < 0)
        memset(param_of, 0, sizeof(param_of));

    size_t pos = 0;
    buf[0] = '\0';

    for (uint8_t i = 0; i < chosen->length; i++) {
        char line[AUTOMATION_LINE_MAX];

        render_line(chosen, i, param_of, line, sizeof(line));

        size_t len = strlen(line);
        if (pos + (i ? 3 : 0) + len + 1 > size)
            break;

        if (i) {
            memcpy(buf + pos, " ; ", 3);
            pos += 3;
        }

        memcpy(buf + pos, line, len);
        pos += len;
    }

    buf[pos] = '\0';

    if (occurrences)
        *occurrences = chosen->count;
    if (proposable)
        *proposable = pattern_is_proposable((struct pattern *)chosen);

    return true;
}
