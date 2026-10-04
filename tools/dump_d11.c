#include <stdio.h>
#include <stdlib.h>

/*
 * Reuse the already-validated exact-distance builder and
 * unrank_state_host() from validate_target.c.
 *
 * Rename its main() so this file can provide its own main().
 */
#define main validate_target_original_main
#include "../validate_target.c"
#undef main

int main(void)
{
    uint8_t *distance = build_exact_distances();

    if (!distance) {
        fprintf(stderr, "failed to build exact-distance table\n");
        return 1;
    }

    state_t state;
    uint32_t count = 0;

    for (uint32_t rank = 0; rank < STATES; ++rank) {
        if (distance[rank] != 11)
            continue;

        unrank_state_host(rank, &state);

        /* 7 permutation digits: internal 0..6 -> input '1'..'7' */
        for (uint8_t i = 0; i < CUBIES; ++i)
            putchar((int)('1' + state.p[i]));

        /* 7 orientation digits: internal 0..2 -> input '1'..'3' */
        for (uint8_t i = 0; i < CUBIES; ++i)
            putchar((int)('1' + state.o[i]));

        putchar('\n');
        ++count;
    }

    fprintf(stderr, "distance-11 states: %u\n", count);

    free(distance);
    return 0;
}
