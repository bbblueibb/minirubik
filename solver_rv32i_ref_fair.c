#define SOLVER_LIBRARY
#include "solver_target.c"

#ifndef INPUT_STATE
#define INPUT_STATE "21345671111111"
#endif

static const char input_state[] = INPUT_STATE;

static const uint8_t move_face[MOVES] = {
    0, 0, 0,
    1, 1, 1,
    2, 2, 2
};

static const uint8_t move_turns[MOVES] = {
    1, 2, 3,
    1, 2, 3,
    1, 2, 3
};

int main(void)
{
    state_t state;

    if (!parse_state(input_state, &state))
        return 2;

    if (!solve_ida(&state))
        return 1;

    /*
     * Same validation convention as the final assembly:
     * replay ida_path[] using permutation/orientation ranks
     * and require p == 0 && o == 0.
     */
    uint16_t p = rank_permutation(&state);
    uint16_t o = rank_orientation(&state);

    for (uint8_t i = 0; i < ida_solution_length; ++i) {
        uint8_t move = ida_path[i];
        uint8_t face = move_face[move];
        uint8_t turns = move_turns[move];

        for (uint8_t t = 0; t < turns; ++t) {
            p = permutation_next(face, p);
            o = orientation_next(face, o);
        }
    }

    if (p != 0 || o != 0)
        return 3;

    return 0;
}
