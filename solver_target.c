#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tables.h"

enum {
    CUBIES = 7,
    PERMUTATIONS = 5040,
    ORIENTATIONS = 729,
    STATES = PERMUTATIONS * ORIENTATIONS,
    MOVES = 9,
    MAX_DEPTH = 11,

    PATTERN_CUBIES = 4,
    PATTERN_POSITIONS = 840,
    PATTERN_ORIENTATIONS = 81,
    PATTERN_STATES = PATTERN_POSITIONS * PATTERN_ORIENTATIONS
};

static uint32_t ida_nodes;
static uint32_t ida_pruned;
static uint8_t ida_iterations;
static uint8_t ida_final_bound;

typedef struct {
    uint8_t p[CUBIES], o[CUBIES];
} state_t;

typedef struct {
    uint8_t position[PATTERN_CUBIES];
    uint8_t orientation[PATTERN_CUBIES];
} pattern_state_t;

typedef struct {
    uint16_t p;
    uint16_t o;
    pattern_state_t pattern;

    int8_t previous_face;
    uint8_t next_move;
} search_frame_t;

static uint8_t ida_path[MAX_DEPTH];
static uint8_t ida_solution_length;

static search_frame_t ida_stack[MAX_DEPTH + 1];

/*@ predicate valid_state(state_t *state) =
      (\forall integer i; 0 <= i < CUBIES ==>
         state->p[i] < CUBIES && state->o[i] < 3) &&
      (\forall integer i, j; 0 <= i < j < CUBIES ==>
         state->p[i] != state->p[j]) &&
      (state->o[0] + state->o[1] + state->o[2] + state->o[3] +
       state->o[4] + state->o[5] + state->o[6]) % 3 == 0;
 */

static uint16_t permutation_next(uint8_t face, uint16_t p)
{
    return permutation_transition[
        (uint32_t) face * PERMUTATIONS + p];
}

static uint16_t orientation_next(uint8_t face, uint16_t o)
{
    return orientation_transition[
        (uint32_t) face * ORIENTATIONS + o];
}

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2", "B'", "D",  "D2", "D'"};

/* For each old position, give its destination after one quarter turn. */
static const uint8_t destination_of[3][CUBIES] = {
    {3, 0, 2, 4, 1, 5, 6},
    {0, 1, 2, 6, 3, 4, 5},
    {0, 4, 1, 3, 5, 2, 6},
};

static const uint8_t twist[3][CUBIES] = {
    {1, 2, 0, 2, 1, 0, 0},
    {0, 0, 0, 1, 2, 1, 2},
    {0, 0, 0, 0, 0, 0, 0},
};

/* The three quarter-turns preserve the fixed front-upper-left corner. */
/*@ requires face < 3;
    assigns \nothing;
    ensures \forall integer i; 0 <= i < CUBIES ==>
              \result.p[i] == state.p[source[face][i]];
    ensures \forall integer i; 0 <= i < CUBIES ==>
              \result.o[i] == (state.o[source[face][i]] + twist[face][i]) % 3;
 */
 
/*@ requires \valid_read(state);
    requires \forall integer i; 0 <= i < CUBIES ==>
      0 <= state->p[i] < CUBIES;
    requires \forall integer i, j; 0 <= i < j < CUBIES ==>
      state->p[i] != state->p[j];
    requires \forall integer i; 0 <= i < CUBIES ==>
      0 <= state->o[i] < 3;
    assigns \nothing;
    ensures \result < STATES;
 */
static uint16_t rank_permutation(const state_t *state)
{
    uint16_t p = 0;

    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t smaller = 0;

        for (uint8_t j = (uint8_t) (i + 1U); j < CUBIES; ++j) {
            if (state->p[j] < state->p[i])
                ++smaller;
        }

        uint16_t scaled;

        switch (i) {
        case 0:
            scaled = (uint16_t) ((p << 3) - p);       /* p * 7 */
            break;
        case 1:
            scaled = (uint16_t) ((p << 2) + (p << 1)); /* p * 6 */
            break;
        case 2:
            scaled = (uint16_t) ((p << 2) + p);       /* p * 5 */
            break;
        case 3:
            scaled = (uint16_t) (p << 2);             /* p * 4 */
            break;
        case 4:
            scaled = (uint16_t) ((p << 1) + p);       /* p * 3 */
            break;
        case 5:
            scaled = (uint16_t) (p << 1);             /* p * 2 */
            break;
        default:
            scaled = p;                               /* p * 1 */
            break;
        }

        p = (uint16_t) (scaled + smaller);
    }

    return p;
}

static uint16_t rank_orientation(const state_t *state)
{
    uint16_t o = 0;

    for (uint8_t i = 0; i < 6; ++i)
        o = (uint16_t) (o * 3U + state->o[i]);

    return o;
}

static uint32_t rank_4corner_components(
    const uint8_t position[PATTERN_CUBIES],
    const uint8_t orientation[PATTERN_CUBIES])
{
    uint8_t p0 = position[0];
    uint8_t p1 = position[1];
    uint8_t p2 = position[2];
    uint8_t p3 = position[3];

    uint8_t q0 = p0;

    uint8_t q1 =
        (uint8_t) (p1 - (p0 < p1));

    uint8_t q2 =
        (uint8_t) (p2
                   - (p0 < p2)
                   - (p1 < p2));

    uint8_t q3 =
        (uint8_t) (p3
                   - (p0 < p3)
                   - (p1 < p3)
                   - (p2 < p3));

    uint16_t position_rank =
        (uint16_t) ((((q0 * 6U) + q1) * 5U + q2) * 4U + q3);

    uint8_t orientation_rank =
        (uint8_t) (((orientation[0] * 3U + orientation[1]) * 3U
                    + orientation[2]) * 3U
                   + orientation[3]);

    return (uint32_t) position_rank * PATTERN_ORIENTATIONS
           + orientation_rank;
}

static pattern_state_t extract_4corner_pattern(const state_t *state)
{
    pattern_state_t pattern;

    for (uint8_t pos = 0; pos < CUBIES; ++pos) {
        uint8_t cubie = state->p[pos];

        if (cubie < PATTERN_CUBIES) {
            pattern.position[cubie] = pos;
            pattern.orientation[cubie] = state->o[pos];
        }
    }

    return pattern;
}

static pattern_state_t quarter_turn_pattern(
    pattern_state_t pattern,
    uint8_t face)
{
    pattern_state_t result;

    for (uint8_t cubie = 0;
         cubie < PATTERN_CUBIES;
         ++cubie) {

        uint8_t old_position =
            pattern.position[cubie];

        uint8_t destination =
            destination_of[face][old_position];

        result.position[cubie] = destination;

        uint8_t new_orientation =
            (uint8_t) (pattern.orientation[cubie] +
                    twist[face][destination]);

        if (new_orientation >= 3U)
            new_orientation = (uint8_t) (new_orientation - 3U);

        result.orientation[cubie] = new_orientation;
    }

    return result;
}

static uint8_t heuristic(uint16_t p, uint16_t o, const pattern_state_t *pattern)
{
    uint8_t hp = permutation_pdb[p];
    uint8_t ho = orientation_pdb[o];

    uint32_t pattern_rank =
        rank_4corner_components(pattern->position,
                                pattern->orientation);

    uint8_t hc = corner4_pdb[pattern_rank];

    uint8_t h = hp > ho ? hp : ho;

    return h > hc ? h : hc;
}

static int ida_dfs_iterative(uint16_t p, uint16_t o, pattern_state_t pattern, uint8_t bound)
{
    int top = 0;

    ida_stack[0].p = p;
    ida_stack[0].o = o;
    ida_stack[0].pattern = pattern;
    ida_stack[0].previous_face = -1;
    ida_stack[0].next_move = 0;

    ++ida_nodes;

    uint8_t h = heuristic(p, o, &pattern);

    if (h > bound) {
        ++ida_pruned;
        return 0;
    }

    if (p == 0 && o == 0) {
        ida_solution_length = 0;
        return 1;
    }

    while (top >= 0) {
        search_frame_t *frame = &ida_stack[top];

        /*
         * All children of this node have been explored.
         * Return to the parent.
         */
        if (frame->next_move >= MOVES) {
            --top;
            continue;
        }

        /*
         * Remember the next move before descending.
         * This replaces the return state normally kept
         * by recursive function calls.
         */
        uint8_t move = frame->next_move++;

        uint8_t face;
        uint8_t turns;

        if (move < 3) {
            face = 0;
            turns = (uint8_t) (move + 1U);
        } else if (move < 6) {
            face = 1;
            turns = (uint8_t) (move - 2U);
        } else {
            face = 2;
            turns = (uint8_t) (move - 5U);
        }

        /*
         * Consecutive moves on the same face are redundant.
         */
        if ((int8_t) face == frame->previous_face)
            continue;

        uint16_t next_p = frame->p;
        uint16_t next_o = frame->o;
        pattern_state_t next_pattern = frame->pattern;

        /*
         * move = R/R2/R', B/B2/B', or D/D2/D'.
         * The transition tables contain quarter turns,
         * so apply them one, two, or three times.
         */
        for (uint8_t turn = 0; turn < turns; ++turn) {
            next_p = permutation_next(face, next_p);
            next_o = orientation_next(face, next_o);
            next_pattern =
                quarter_turn_pattern(next_pattern, face);
        }

        uint8_t depth = (uint8_t) (top + 1);

        ++ida_nodes;

        h = heuristic(next_p, next_o, &next_pattern);

        if ((uint8_t) (depth + h) > bound) {
            ++ida_pruned;
            continue;
        }

        ida_path[top] = move;

        if (next_p == 0 && next_o == 0) {
            ida_solution_length = depth;
            return 1;
        }

        /*
         * An unsolved state at the current bound cannot
         * produce another child within this iteration.
         */
        if (depth == bound)
            continue;

        /*
         * Push the child onto the explicit stack.
         */
        ++top;

        ida_stack[top].p = next_p;
        ida_stack[top].o = next_o;
        ida_stack[top].pattern = next_pattern;
        ida_stack[top].previous_face = (int8_t) face;
        ida_stack[top].next_move = 0;
    }

    return 0;
}

static int solve_ida(const state_t *state)
{
    uint16_t p = rank_permutation(state);
    uint16_t o = rank_orientation(state);

    pattern_state_t pattern = extract_4corner_pattern(state);

    uint8_t bound = heuristic(p, o, &pattern);

    ida_nodes = 0;
    ida_pruned = 0;
    ida_iterations = 0;
    ida_final_bound = 0;

    while (bound <= MAX_DEPTH) {
        ++ida_iterations;

        if (ida_dfs_iterative(p, o, pattern, bound)) {
            ida_final_bound = bound;
            return 1;
        }

        ++bound;
    }

    return 0;
}

/*@ requires \valid_read(state);
    requires \initialized(&state->p[0..6]) && \initialized(&state->o[0..6]);
    assigns \nothing;
    ensures \result != 0 ==> \forall integer i; 0 <= i < CUBIES ==>
      state->p[i] < CUBIES && state->o[i] < 3;
    ensures \result != 0 ==> \forall integer i, j; 0 <= i < j < CUBIES ==>
      state->p[i] != state->p[j];
    ensures \result != 0 ==>
      (state->o[0] + state->o[1] + state->o[2] + state->o[3] +
       state->o[4] + state->o[5] + state->o[6]) % 3 == 0;
    ensures complete: valid_state(state) ==> \result != 0;
 */

static int valid(const state_t *state)
{
    uint8_t sum = 0;
    /*@ loop invariant 0 <= i <= CUBIES;
        loop invariant sum <= 2 * i;
        loop invariant sum == (i > 0 ? state->o[0] : 0) +
          (i > 1 ? state->o[1] : 0) + (i > 2 ? state->o[2] : 0) +
          (i > 3 ? state->o[3] : 0) + (i > 4 ? state->o[4] : 0) +
          (i > 5 ? state->o[5] : 0) + (i > 6 ? state->o[6] : 0);
        loop invariant \forall integer j; 0 <= j < i ==>
          state->p[j] < CUBIES && state->o[j] < 3;
        loop invariant \forall integer j, k; 0 <= j < k < i ==>
          state->p[j] != state->p[k];
        loop assigns i, sum;
        loop variant CUBIES - i;
    */
    for (uint8_t i = 0; i < CUBIES; ++i) {
        if (state->p[i] >= CUBIES || state->o[i] >= 3)
            return 0;
        /*@ loop invariant 0 <= j <= i;
            loop invariant \forall integer k; 0 <= k < j ==>
              state->p[k] != state->p[i];
            loop assigns j;
            loop variant i - j;
        */
        for (uint8_t j = 0; j < i; ++j)
            if (state->p[j] == state->p[i])
                return 0;
        sum = (uint8_t) (sum + state->o[i]);
    }
    while (sum >= 3U)
        sum = (uint8_t) (sum - 3U);

    return sum == 0;
}

/*@ requires valid_read_string(input);
    requires \valid(state);
    assigns state->p[0..6], state->o[0..6];
    ensures \result != 0 ==> input[14] == '\0';
    ensures \result != 0 ==> \forall integer i; 0 <= i < CUBIES ==>
      state->p[i] < CUBIES && state->o[i] < 3;
    ensures \result != 0 ==> \forall integer i, j; 0 <= i < j < CUBIES ==>
      state->p[i] != state->p[j];
    ensures \result != 0 ==>
      (state->o[0] + state->o[1] + state->o[2] + state->o[3] +
       state->o[4] + state->o[5] + state->o[6]) % 3 == 0;
    ensures \result != 0 ==> \forall integer i; 0 <= i < CUBIES ==>
      state->p[i] == input[i] - '1';
    ensures \result != 0 ==> \forall integer i; 0 <= i < CUBIES ==>
      state->o[i] == input[i + CUBIES] - '1';
 */
static int parse_state(const char *input, state_t *state)
{
    /*@ loop invariant 0 <= i <= 14;
        loop invariant i <= strlen(input);
        loop invariant i <= 7 ==> \initialized(&state->p[0..i-1]);
        loop invariant i >= 7 ==> \initialized(&state->p[0..6]);
        loop invariant i >= 7 ==> \initialized(&state->o[0..i-8]);
        loop invariant \forall integer j; 0 <= j < i && j < CUBIES ==>
          state->p[j] == input[j] - '1';
        loop invariant \forall integer j; 0 <= j < i - CUBIES ==>
          state->o[j] == input[j + CUBIES] - '1';
        loop assigns i, state->p[0..6], state->o[0..6];
        loop variant 14 - i;
     */
    for (int i = 0; i < 14; ++i) {
        int limit = i < 7 ? 7 : 3;
        if (input[i] < '1' || input[i] > '0' + limit)
            return 0;
        
        uint8_t value = (uint8_t) (input[i] - '1');

        if (i < 7)
            state->p[i] = value;
        else
            state->o[i - 7] = value;
    }
    return input[14] == '\0' && valid(state);
}

/* stdout is fully buffered off a terminal, so a write error surfaces at the
 * flush, not at the printf that queued the bytes. Every exit path that has
 * produced output goes through here.
 */
static int output_failed(void)
{
    return fflush(stdout) != 0 || ferror(stdout);
}

#ifndef SOLVER_LIBRARY
int main(int argc, char **argv)
{
    state_t state;

    if (argc != 2 || !parse_state(argv[1], &state)) {
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "solver_target");
        return 2;
    }

    if (!solve_ida(&state)) {
        fputs("IDA* could not find a solution\n", stderr);
        return 1;
    }

    const char *separator = "";

    for (uint8_t i = 0; i < ida_solution_length; ++i) {
        printf("%s%s", separator, move_names[ida_path[i]]);
        separator = " ";
    }

    putchar('\n');

    return output_failed();
}

#endif