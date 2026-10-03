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

static uint8_t ida_path[MAX_DEPTH];
static uint8_t ida_solution_length;

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

static const char *const move_names[MOVES] = {"R",  "R2", "R'", "B", "B2",
                                              "B'", "D",  "D2", "D'"};
static const uint8_t inverse_move[MOVES] = {2, 1, 0, 5, 4, 3, 8, 7, 6};
/* Each destination takes a cubie from source[face][destination]. */
static const uint8_t source[3][CUBIES] = {
    {1, 4, 2, 0, 3, 5, 6},
    {0, 1, 2, 4, 5, 6, 3},
    {0, 2, 5, 3, 1, 4, 6},
};

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
static state_t quarter_turn(state_t state, uint8_t face)
{
    state_t result;
    /*@ loop invariant 0 <= i <= CUBIES;
        loop invariant \forall integer j; 0 <= j < i ==>
          result.p[j] == state.p[source[face][j]];
        loop invariant \forall integer j; 0 <= j < i ==>
          result.o[j] == (state.o[source[face][j]] + twist[face][j]) % 3;
        loop assigns i, result.p[0..6], result.o[0..6];
        loop variant CUBIES - i;
    */
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t from = source[face][i];
        result.p[i] = state.p[from];
        result.o[i] = (uint8_t) ((state.o[from] + twist[face][i]) % 3U);
    }
    return result;
}

static state_t apply_move(state_t state, uint8_t move)
{
    uint8_t turns = (uint8_t) (move % 3U + 1U);
    for (uint8_t i = 0; i < turns; ++i)
        state = quarter_turn(state, (uint8_t) (move / 3U));
    return state;
}

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

        p = (uint16_t) (p * (CUBIES - i) + smaller);
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

static uint32_t rank_state(const state_t *state)
{
    uint16_t p = rank_permutation(state);
    uint16_t o = rank_orientation(state);

    return (uint32_t) p * ORIENTATIONS + o;
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

static uint32_t rank_4corner_pattern(const state_t *state)
{
    uint8_t position[PATTERN_CUBIES];
    uint8_t orientation[PATTERN_CUBIES];

    for (uint8_t pos = 0; pos < CUBIES; ++pos) {
        uint8_t cubie = state->p[pos];

        if (cubie < PATTERN_CUBIES) {
            position[cubie] = pos;
            orientation[cubie] = state->o[pos];
        }
    }

    return rank_4corner_components(position, orientation);
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

        result.orientation[cubie] =
            (uint8_t) (
                (pattern.orientation[cubie] +
                 twist[face][destination]) %
                3U);
    }

    return result;
}

/*@ requires \valid(state); requires rank < STATES; assigns *state; */
static void unrank_state(uint32_t rank, state_t *state)
{
    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};
    uint32_t p = rank / ORIENTATIONS, o = rank % ORIENTATIONS, f = 720;
    uint8_t sum = 0;
    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint8_t q = (uint8_t) (p / f);
        p %= f;
        state->p[i] = available[q];
        for (uint8_t j = q; j + 1 < CUBIES - i; ++j)
            available[j] = available[j + 1U];
        if (i < 5)
            f /= 6U - i;
    }
    for (uint8_t i = 6; i-- > 0;) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }
    state->o[6] = (uint8_t) ((3U - sum % 3U) % 3U);
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

static int ida_dfs(uint16_t p, uint16_t o, pattern_state_t pattern ,uint8_t depth, uint8_t bound, int8_t previous_face)
{
    ++ida_nodes;

    uint8_t h = heuristic(p, o, &pattern);

    if ((uint8_t) (depth + h) > bound) {
        ++ida_pruned;
        return 0;
    }

    if (p == 0 && o == 0) {
        ida_solution_length = depth;
        return 1;
    }

    if (depth == bound)
        return 0;

    for (uint8_t face = 0; face < 3; ++face) {
        if ((int8_t) face == previous_face){
                continue;
            }
            uint16_t next_p = p;
            uint16_t next_o = o;
            pattern_state_t next_pattern = pattern;

            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation_next(face, next_p);
                next_o = orientation_next(face, next_o);
                next_pattern = quarter_turn_pattern(next_pattern, face);

                ida_path[depth] = (uint8_t) (face * 3U + turn);

                if (ida_dfs(next_p, next_o, next_pattern, (uint8_t) (depth + 1U), bound, (int8_t) face))
                    return 1;
            }
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

        if (ida_dfs(p, o, pattern, 0, bound, -1)) {
            ida_final_bound = bound;
            return 1;
        }

        ++bound;
    }

    return 0;
}

static uint8_t *build_full_distance_table(void)
{
    uint8_t *distance = malloc(STATES);
    uint32_t *queue =
        malloc((size_t) STATES * sizeof *queue);

    if (!distance || !queue) {
        free(distance);
        free(queue);
        return NULL;
    }

    memset(distance, UINT8_MAX, STATES);

    uint32_t head = 0;
    uint32_t tail = 1;

    distance[0] = 0;
    queue[0] = 0;

    while (head < tail) {
        uint32_t here = queue[head++];

        uint16_t p =
            (uint16_t) (here / ORIENTATIONS);
        uint16_t o =
            (uint16_t) (here % ORIENTATIONS);

        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p;
            uint16_t next_o = o;

            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation_next(face, next_p);
                next_o = orientation_next(face, next_o);
                uint32_t there =
                    (uint32_t) next_p * ORIENTATIONS +
                    next_o;

                if (distance[there] == UINT8_MAX) {
                    distance[there] =
                        (uint8_t) (distance[here] + 1U);

                    queue[tail++] = there;
                }
            }
        }
    }

    free(queue);

    if (tail != STATES) {
        free(distance);
        return NULL;
    }

    return distance;
}

static void state_to_input(const state_t *state, char output[15])
{
    for (uint8_t i = 0; i < CUBIES; ++i) {
        output[i] =
            (char) ('1' + state->p[i]);

        output[i + CUBIES] =
            (char) ('1' + state->o[i]);
    }

    output[14] = '\0';
}

static int benchmark_distance_11(void)
{
    uint8_t *distance = build_full_distance_table();

    if (!distance) {
        fputs("could not build full distance table\n", stderr);
        return 0;
    }

    uint32_t count = 0;
    uint32_t min_nodes = UINT32_MAX;
    uint32_t max_nodes = 0;
    uint32_t worst_rank = 0;
    uint64_t total_nodes = 0;

    state_t state;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        if (distance[rank] != 11)
            continue;

        unrank_state(rank, &state);

        if (!solve_ida(&state) ||
            ida_solution_length != 11) {
            fprintf(stderr,
                    "IDA* failed on rank %u\n",
                    rank);
            free(distance);
            return 0;
        }

        ++count;
        total_nodes += ida_nodes;

        if (ida_nodes < min_nodes)
            min_nodes = ida_nodes;

        if (ida_nodes > max_nodes) {
            max_nodes = ida_nodes;
            worst_rank = rank;
        }
    }

    free(distance);

    if (count != 2644) {
        fprintf(stderr,
                "expected 2644 distance-11 states, got %u\n",
                count);
        return 0;
    }

    unrank_state(worst_rank, &state);

    char worst_input[15];
    state_to_input(&state, worst_input);

    printf("distance-11 states: %u\n", count);
    printf("minimum nodes: %u\n", min_nodes);
    printf("average nodes: %llu\n",
           (unsigned long long) (total_nodes / count));
    printf("maximum nodes: %u\n", max_nodes);
    printf("worst-case rank: %u\n", worst_rank);
    printf("worst-case state: %s\n", worst_input);

    return 1;
}

static int test_4corner_ranking(void)
{
    uint8_t *counts = calloc(PATTERN_STATES, sizeof *counts);

    if (!counts)
        return 0;

    const state_t solved = {
        {0, 1, 2, 3, 4, 5, 6},
        {0}
    };

    if (rank_4corner_pattern(&solved) != 0) {
        free(counts);
        return 0;
    }

    state_t state;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &state);

        uint32_t pattern_rank =
            rank_4corner_pattern(&state);

        if (pattern_rank >= PATTERN_STATES) {
            free(counts);
            return 0;
        }

        ++counts[pattern_rank];
    }

    for (uint32_t rank = 0; rank < PATTERN_STATES; ++rank) {
        if (counts[rank] != 54) {
            free(counts);
            return 0;
        }
    }

    free(counts);
    return 1;
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
    return sum % 3U == 0;
}

static uint8_t *build_table(uint8_t *diameter)
{
    uint8_t *toward_solved = malloc(STATES);
    uint32_t *queue = malloc((size_t) STATES * sizeof *queue);
    uint16_t permutation[3][PERMUTATIONS], orientation[3][ORIENTATIONS];
    uint32_t head = 0, tail = 1, level_end = 1;
    state_t state;
    if (!toward_solved || !queue) {
        free(toward_solved);
        free(queue);
        return NULL;
    }
    for (uint16_t rank = 0; rank < PERMUTATIONS; ++rank) {
        unrank_state((uint32_t) rank * ORIENTATIONS, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            permutation[face][rank] =
                (uint16_t) (rank_state(&next) / ORIENTATIONS);
        }
    }
    for (uint16_t rank = 0; rank < ORIENTATIONS; ++rank) {
        unrank_state(rank, &state);
        for (uint8_t face = 0; face < 3; ++face) {
            state_t next = quarter_turn(state, face);
            orientation[face][rank] =
                (uint16_t) (rank_state(&next) % ORIENTATIONS);
        }
    }
    memset(toward_solved, UINT8_MAX, STATES);
    queue[0] = 0;
    toward_solved[0] = 0;
    *diameter = 0;
    while (head < tail) {
        if (head == level_end) {
            level_end = tail;
            ++*diameter;
        }
        uint32_t here = queue[head++];
        uint16_t p = (uint16_t) (here / ORIENTATIONS);
        uint16_t o = (uint16_t) (here % ORIENTATIONS);
        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p, next_o = o;
            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation[face][next_p];
                next_o = orientation[face][next_o];
                uint32_t there = (uint32_t) next_p * ORIENTATIONS + next_o;
                if (toward_solved[there] == UINT8_MAX) {
                    uint8_t move = (uint8_t) (face * 3U + turn);
                    toward_solved[there] = inverse_move[move];
                    queue[tail++] = there;
                }
            }
        }
    }
    free(queue);
    if (tail != STATES) {
        free(toward_solved);
        return NULL;
    }
    return toward_solved;
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
        (i < 7 ? state->p : state->o)[i % 7] = (uint8_t) (input[i] - '1');
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

static int self_test(void)
{
    const state_t solved = {{0, 1, 2, 3, 4, 5, 6}, {0}};
    state_t state;
    for (uint8_t move = 0; move < MOVES; ++move) {
        state = solved;
        state = apply_move(state, move);
        state = apply_move(state, inverse_move[move]);
        if (memcmp(&solved, &state, sizeof solved))
            return 0;
    }
    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state(rank, &state);
        if (!valid(&state) || rank_state(&state) != rank)
            return 0;
    }
    return 1;
}



int main(int argc, char **argv)
{
    state_t state;
    uint8_t diameter;

    if (argc == 2 && !strcmp(argv[1], "--pattern-test")) {
        if (!test_4corner_ranking()) {
            fputs("4-corner pattern ranking test failed\n",
                stderr);
            return 1;
        }

        printf("4-corner pattern: %u states; "
            "54 full states per pattern\n",
            PATTERN_STATES);

        return output_failed();
    }

    if (argc == 2 &&
        !strcmp(argv[1], "--benchmark-d11")) {

        if (!benchmark_distance_11())
            return 1;

        return output_failed();
    }

    if (argc == 3 && (!strcmp(argv[1], "--ida") || !strcmp(argv[1], "--ida-stats"))) {
        if (!parse_state(argv[2], &state)) {
            fputs("invalid cube state\n", stderr);
            return 2;
        }

        if (!solve_ida(&state)) {
            fputs("IDA* could not find a solution\n", stderr);
            return 1;
        }

        if (!strcmp(argv[1], "--ida-stats")) {
            fprintf(stderr,
                    "IDA*: bound=%u, iterations=%u, nodes=%u, pruned=%u\n",
                    ida_final_bound,
                    ida_iterations,
                    ida_nodes,
                    ida_pruned);
        }

        const char *separator = "";

        for (uint8_t i = 0; i < ida_solution_length; ++i) {
            printf("%s%s", separator, move_names[ida_path[i]]);
            separator = " ";
        }

        putchar('\n');
        return output_failed();
    }

    if (argc == 2 && !strcmp(argv[1], "--self-test")) {
        if (!self_test()) {
            fputs("self-test failed\n", stderr);
            return 1;
        }
        uint8_t *table = build_table(&diameter);
        if (!table) {
            fputs("could not build complete state table\n", stderr);
            return 1;
        }
        free(table);
        if (diameter != 11) {
            fputs("BFS check failed\n", stderr);
            return 1;
        }
        puts("3674160 states; diameter 11");
        return output_failed();
    }
    if (argc != 2 || !parse_state(argv[1], &state)) {
        /* C99 5.1.2.2.1 lets argv[0] be null when argc is 0. */
        fprintf(stderr, "usage: %s PPPPPPPOOOOOOO\n",
                argc > 0 && argv[0] ? argv[0] : "solver");
        return 2;
    }
    uint8_t *table = build_table(&diameter);
    if (!table) {
        fputs("could not build complete state table\n", stderr);
        return 1;
    }
    const char *separator = "";
    for (uint32_t rank = rank_state(&state); rank; rank = rank_state(&state)) {
        uint8_t move = table[rank];
        printf("%s%s", separator, move_names[move]);
        separator = " ";
        state = apply_move(state, move);
    }
    putchar('\n');
    free(table);
    return output_failed();
}
