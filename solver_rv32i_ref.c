#define SOLVER_LIBRARY
#include "solver_target.c"

#ifndef INPUT_STATE
#define INPUT_STATE "21345671111111"
#endif

static const char input_state[] = INPUT_STATE;

volatile uint8_t reference_solution_length;
volatile uint8_t reference_solution[MAX_DEPTH];

int main(void)
{
    state_t state;

    if (!parse_state(input_state, &state))
        return 2;

    if (!solve_ida(&state))
        return 1;

    reference_solution_length = ida_solution_length;

    for (uint8_t i = 0; i < ida_solution_length; ++i)
        reference_solution[i] = ida_path[i];

    return 0;
}

