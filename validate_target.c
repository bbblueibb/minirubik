#define SOLVER_LIBRARY
#include "solver_target.c"

/*
 * H2 host-side validator.
 *
 * This file is not part of the RV32I target.
 * It verifies the static tables used by solver_target.c.
 */

static int check_permutation_transitions(void)
{
    static uint8_t seen[PERMUTATIONS];

    for (uint8_t face = 0; face < 3; ++face) {
        memset(seen, 0, sizeof seen);

        for (uint16_t p = 0; p < PERMUTATIONS; ++p) {
            uint16_t next = permutation_next(face, p);

            if (next >= PERMUTATIONS) {
                fprintf(stderr,
                        "permutation transition out of range: "
                        "face=%u state=%u next=%u\n",
                        face, p, next);
                return 0;
            }

            if (seen[next]) {
                fprintf(stderr,
                        "duplicate permutation transition: "
                        "face=%u next=%u\n",
                        face, next);
                return 0;
            }

            seen[next] = 1;

            /*
             * Four quarter turns of the same face must
             * return to the original abstract state.
             */
            uint16_t q = p;

            for (uint8_t i = 0; i < 4; ++i)
                q = permutation_next(face, q);

            if (q != p) {
                fprintf(stderr,
                        "permutation 4-turn check failed: "
                        "face=%u state=%u\n",
                        face, p);
                return 0;
            }
        }
    }

    printf("permutation transitions: "
           "%u entries; valid and complete\n",
           3U * PERMUTATIONS);

    return 1;
}


static int check_orientation_transitions(void)
{
    static uint8_t seen[ORIENTATIONS];

    for (uint8_t face = 0; face < 3; ++face) {
        memset(seen, 0, sizeof seen);

        for (uint16_t o = 0; o < ORIENTATIONS; ++o) {
            uint16_t next = orientation_next(face, o);

            if (next >= ORIENTATIONS) {
                fprintf(stderr,
                        "orientation transition out of range: "
                        "face=%u state=%u next=%u\n",
                        face, o, next);
                return 0;
            }

            if (seen[next]) {
                fprintf(stderr,
                        "duplicate orientation transition: "
                        "face=%u next=%u\n",
                        face, next);
                return 0;
            }

            seen[next] = 1;

            uint16_t q = o;

            for (uint8_t i = 0; i < 4; ++i)
                q = orientation_next(face, q);

            if (q != o) {
                fprintf(stderr,
                        "orientation 4-turn check failed: "
                        "face=%u state=%u\n",
                        face, o);
                return 0;
            }
        }
    }

    printf("orientation transitions: "
           "%u entries; valid and complete\n",
           3U * ORIENTATIONS);

    return 1;
}


static int check_pdb(const char *name,
                     const uint8_t *table,
                     uint32_t count,
                     uint8_t expected_max)
{
    uint8_t max_distance = 0;

    if (table[0] != 0) {
        fprintf(stderr,
                "%s solved entry is %u, expected 0\n",
                name, table[0]);
        return 0;
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (table[i] == UINT8_MAX) {
            fprintf(stderr,
                    "%s contains an unfilled entry at %u\n",
                    name, i);
            return 0;
        }

        if (table[i] > max_distance)
            max_distance = table[i];
    }

    if (max_distance != expected_max) {
        fprintf(stderr,
                "%s max distance is %u, expected %u\n",
                name, max_distance, expected_max);
        return 0;
    }

    printf("%s: %u entries; solved=0; max=%u; complete\n",
           name, count, max_distance);

    return 1;
}


static int run_h2(void)
{
    if (!check_permutation_transitions())
        return 0;

    if (!check_orientation_transitions())
        return 0;

    if (!check_pdb("permutation PDB",
                   permutation_pdb,
                   PERMUTATIONS,
                   7))
        return 0;

    if (!check_pdb("orientation PDB",
                   orientation_pdb,
                   ORIENTATIONS,
                   6))
        return 0;

    if (!check_pdb("4-corner PDB",
                   corner4_pdb,
                   PATTERN_STATES,
                   8))
        return 0;

    puts("H2: PASS");
    return 1;
}

static void unrank_state_host(uint32_t rank, state_t *state)
{
    uint16_t p = (uint16_t) (rank / ORIENTATIONS);
    uint16_t o = (uint16_t) (rank % ORIENTATIONS);

    uint8_t available[CUBIES] = {0, 1, 2, 3, 4, 5, 6};

    for (uint8_t i = 0; i < CUBIES; ++i) {
        uint16_t f = 1;

        for (uint8_t j = 2; j < CUBIES - i; ++j)
            f = (uint16_t) (f * j);

        uint8_t q = (uint8_t) (p / f);
        p %= f;

        state->p[i] = available[q];

        for (uint8_t j = q; j + 1 < CUBIES - i; ++j)
            available[j] = available[j + 1U];
    }

    uint8_t sum = 0;

    for (int i = CUBIES - 2; i >= 0; --i) {
        state->o[i] = (uint8_t) (o % 3U);
        sum = (uint8_t) (sum + state->o[i]);
        o /= 3U;
    }

    state->o[CUBIES - 1] =
        (uint8_t) ((3U - (sum % 3U)) % 3U);
}

static uint8_t *build_exact_distances(void)
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
        uint32_t rank = queue[head++];

        uint16_t p =
            (uint16_t) (rank / ORIENTATIONS);
        uint16_t o =
            (uint16_t) (rank % ORIENTATIONS);

        for (uint8_t face = 0; face < 3; ++face) {
            uint16_t next_p = p;
            uint16_t next_o = o;

            for (uint8_t turn = 0; turn < 3; ++turn) {
                next_p = permutation_next(face, next_p);
                next_o = orientation_next(face, next_o);

                uint32_t next_rank =
                    (uint32_t) next_p * ORIENTATIONS +
                    next_o;

                if (distance[next_rank] == UINT8_MAX) {
                    distance[next_rank] =
                        (uint8_t) (distance[rank] + 1U);

                    queue[tail++] = next_rank;
                }
            }
        }
    }

    free(queue);

    if (tail != STATES) {
        fprintf(stderr,
                "exact BFS reached %u states, expected %u\n",
                tail, STATES);

        free(distance);
        return NULL;
    }

    return distance;
}

static int run_h1(void)
{
    uint8_t *distance = build_exact_distances();

    if (!distance) {
        fputs("could not build exact distance table\n", stderr);
        return 0;
    }

    uint32_t checked = 0;
    uint8_t maximum_h = 0;
    uint8_t maximum_distance = 0;

    state_t state;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        uint16_t p =
            (uint16_t) (rank / ORIENTATIONS);

        uint16_t o =
            (uint16_t) (rank % ORIENTATIONS);

        unrank_state_host(rank, &state);

        pattern_state_t pattern =
            extract_4corner_pattern(&state);

        uint8_t h =
            heuristic(p, o, &pattern);

        uint8_t d = distance[rank];

        if (h > d) {
            fprintf(stderr,
                    "H1 failed: rank=%u h=%u distance=%u\n",
                    rank, h, d);

            free(distance);
            return 0;
        }

        if (h > maximum_h)
            maximum_h = h;

        if (d > maximum_distance)
            maximum_distance = d;

        ++checked;
    }

    free(distance);

    printf("states checked: %u\n", checked);
    printf("maximum heuristic: %u\n", maximum_h);
    printf("maximum exact distance: %u\n", maximum_distance);
    puts("H1: PASS");

    return 1;
}

static int run_h3(void)
{
    uint8_t *distance = build_exact_distances();

    if (!distance) {
        fputs("could not build exact distance table\n", stderr);
        return 0;
    }

    state_t state;

    uint32_t checked = 0;
    uint64_t total_nodes = 0;
    uint32_t maximum_nodes = 0;
    uint32_t worst_rank = 0;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        unrank_state_host(rank, &state);

        if (!solve_ida(&state)) {
            fprintf(stderr,
                    "H3 failed: solver found no solution "
                    "for rank %u\n",
                    rank);

            free(distance);
            return 0;
        }

        if (ida_solution_length != distance[rank]) {
            fprintf(stderr,
                    "H3 failed: rank=%u "
                    "solver=%u exact=%u\n",
                    rank,
                    ida_solution_length,
                    distance[rank]);

            free(distance);
            return 0;
        }

        total_nodes += ida_nodes;

        if (ida_nodes > maximum_nodes) {
            maximum_nodes = ida_nodes;
            worst_rank = rank;
        }

        ++checked;

        if (checked % 100000U == 0)
            fprintf(stderr,
                    "checked %u / %u states\n",
                    checked, STATES);
    }

    free(distance);

    printf("states checked: %u\n", checked);
    printf("maximum search nodes: %u\n", maximum_nodes);
    printf("worst-case rank: %u\n", worst_rank);
    printf("average search nodes: %llu\n",
        (unsigned long long) (total_nodes / checked));
    puts("H3: PASS");

    return 1;
}

int main(int argc, char **argv)
{
    if (argc == 2 && !strcmp(argv[1], "--h1"))
        return run_h1() ? 0 : 1;

    if (argc == 2 && !strcmp(argv[1], "--h2"))
        return run_h2() ? 0 : 1;

    if (argc == 2 && !strcmp(argv[1], "--h3"))
        return run_h3() ? 0 : 1;

    fprintf(stderr,
            "usage: %s --h1 | --h2 | --h3\n",
            argv[0]);

    return 2;
}