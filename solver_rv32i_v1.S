.equ CUBIES,                 7
.equ PERMUTATIONS,           5040
.equ ORIENTATIONS,           729
.equ MOVES,                  9
.equ MAX_DEPTH,              11

.equ PATTERN_CUBIES,         4
.equ PATTERN_ORIENTATIONS,   81
# Search-frame layout: 16 bytes per frame
.equ FRAME_P,                0
.equ FRAME_O,                2
.equ FRAME_PATTERN_POS,      4
.equ FRAME_PATTERN_ORI,      8
.equ FRAME_PREV_FACE,        12
.equ FRAME_NEXT_MOVE,        13
.equ FRAME_SIZE,             16

.section .bss
.align 4

# 12 frames: root depth 0 through depth 11
ida_stack:
    .space 192

ida_path:
    .space 11

ida_solution_length:
    .space 1

# Parsed input state:
# permutation[7] + orientation[7]
state_p:
    .space 7

state_o:
    .space 7

# Runtime stack used by normal function calls.
.align 4
call_stack:
    .space 512
call_stack_top:


.section .rodata
.align 2

# Initial test input for Version 1.
input_state:
    .asciz "21345671111111"
# Expected result after parse_state
expected_p:
    .byte 1, 0, 2, 3, 4, 5, 6

expected_o:
    .byte 0, 0, 0, 0, 0, 0, 0

# Non-zero orientation vector used to test rank_orientation.
# Only the first six orientations are ranked.
rank_orientation_test:
    .byte 0, 1, 2, 0, 1, 2, 0

# Destination of each old board position after one quarter turn.
# Rows: R, B, D
destination_of:
    .byte 3, 0, 2, 4, 1, 5, 6
    .byte 0, 1, 2, 6, 3, 4, 5
    .byte 0, 4, 1, 3, 5, 2, 6

# Orientation change indexed by destination position.
# Rows: R, B, D
twist:
    .byte 1, 2, 0, 2, 1, 0, 0
    .byte 0, 0, 0, 1, 2, 1, 2
    .byte 0, 0, 0, 0, 0, 0, 0

.section .text
.globl _start

_start:
    # Initialize the runtime call stack.
    la sp, call_stack_top

    # a0 = address of the 14-character input
    la a0, input_state

    # a1 = destination permutation array
    la a1, state_p

    # a2 = destination orientation array
    la a2, state_o

    call parse_state

    # parse_state returns:
    # a0 = 1 on success
    # a0 = 0 on invalid input
    beq a0, x0, parse_failed

parse_success:
    call verify_parsed_state
    beq a0, x0, verify_failed

    # Test rank_orientation with a non-zero example.
    la a0, rank_orientation_test
    call rank_orientation

    li t0, 140
    bne a0, t0, rank_orientation_failed

    # Test rank_permutation using parsed state_p.
    la a0, state_p
    call rank_permutation

    li t0, 720
    bne a0, t0, rank_permutation_failed

    # Test extraction of the four-corner pattern.
    la a0, state_p
    la a1, state_o
    call extract_4corner_pattern

    # Expected positions:
    # cubie 0 -> position 1
    # cubie 1 -> position 0
    # cubie 2 -> position 2
    # cubie 3 -> position 3
    #
    # Packed little-endian bytes = [1,0,2,3]
    # so the 32-bit word is 0x03020001.
    li t0, 0x03020001
    bne a0, t0, extract_pattern_failed

    # All orientations are zero for this input.
    bne a1, x0, extract_pattern_failed

    # a0 still contains packed positions.
    # a1 still contains packed orientations.
    call rank_4corner_components

    li t0, 9720
    bne a0, t0, rank_4corner_failed

    # Test the three-PDB heuristic.
    # permutation rank = 720
    # orientation rank = 0
    # corner4 rank = 9720
    li a0, 720
    li a1, 0
    li a2, 9720
    call heuristic

    # max(
    #   permutation_pdb[720] = 7,
    #   orientation_pdb[0]   = 0,
    #   corner4_pdb[9720]    = 5
    # ) = 7
    li t0, 7
    bne a0, t0, heuristic_failed

    # Test permutation transition:
    # face 0 = R quarter turn
    # permutation_transition[0][720] = 384
    li a0, 0
    li a1, 720
    call permutation_next

    li t0, 384
    bne a0, t0, transition_failed

    # Test orientation transition:
    # orientation_transition[0][0] = 426
    li a0, 0
    li a1, 0
    call orientation_next

    li t0, 426
    bne a0, t0, transition_failed

    # Test one R quarter turn on the packed 4-corner pattern.
    li a0, 0x03020001      # positions [1,0,2,3]
    li a1, 0x00000000      # orientations [0,0,0,0]
    li a2, 0               # face 0 = R
    call quarter_turn_pattern

    # Expected new positions [0,3,2,4]
    li t0, 0x04020300
    bne a0, t0, quarter_turn_failed

    # Expected new orientations [1,2,0,1]
    li t0, 0x01000201
    bne a1, t0, quarter_turn_failed

    # Test initialization of IDA* root search frame.
    li a0, 720              # permutation rank
    li a1, 0                # orientation rank
    li a2, 0x03020001       # packed pattern positions
    li a3, 0x00000000       # packed pattern orientations
    call init_root_frame

    call verify_root_frame
    beq a0, x0, root_frame_failed

    # Test generic move application.
    #
    # Start:
    #   p   = 720
    #   o   = 0
    #   pos = 0x03020001
    #   ori = 0
    #
    # move 0 = R
    li a0, 720
    li a1, 0
    li a2, 0x03020001
    li a3, 0x00000000
    li a4, 0
    call apply_move

    li t0, 384
    bne a0, t0, apply_move_failed

    li t0, 426
    bne a1, t0, apply_move_failed

    li t0, 0x04020300
    bne a2, t0, apply_move_failed

    li t0, 0x01000201
    bne a3, t0, apply_move_failed

    # --------------------------------------------------------
    # Test iterative IDA* DFS on a one-move scramble.
    #
    # Start from solved state:
    #   p   = 0
    #   o   = 0
    #   pos = [0,1,2,3] = 0x03020100
    #   ori = 0
    #
    # Apply R once, then search with bound = 1.
    # The optimal answer must be R' (move 2).
    # --------------------------------------------------------

    li a0, 0
    li a1, 0
    li a2, 0x03020100
    li a3, 0
    li a4, 0                 # move 0 = R
    call apply_move

    # a0-a3 now contain the one-move scrambled state.
    li a4, 1                 # IDA* bound = 1
    call ida_dfs_iterative

    # Must find a solution.
    li t0, 1
    bne a0, t0, ida_dfs_failed

    # Solution length must be exactly 1.
    la t0, ida_solution_length
    lbu t1, 0(t0)
    li t2, 1
    bne t1, t2, ida_dfs_failed

    # The inverse of R is R' = move 2.
    la t0, ida_path
    lbu t1, 0(t0)
    li t2, 2
    bne t1, t2, ida_dfs_failed

    # --------------------------------------------------------
    # Test the complete iterative-deepening solver on the
    # assignment vector 21345671111111.
    #
    # Rebuild the state representation from parsed arrays.
    # --------------------------------------------------------

    la a0, state_p
    call rank_permutation
    mv s8, a0

    la a0, state_o
    call rank_orientation
    mv s9, a0

    la a0, state_p
    la a1, state_o
    call extract_4corner_pattern
    mv s10, a0
    mv s11, a1

    # Run full IDA*.
    mv a0, s8
    mv a1, s9
    mv a2, s10
    mv a3, s11
    call solve_ida

    # Solver must succeed.
    li t0, 1
    bne a0, t0, solve_ida_failed

    # This particular vector has exact distance 11.
    la t0, ida_solution_length
    lbu t1, 0(t0)
    li t2, 11
    bne t1, t2, solve_ida_failed

    # --------------------------------------------------------
    # T5: internally validate the returned solution path.
    #
    # Reapply ida_path[] to the original state and verify
    # that the final state is solved.
    # --------------------------------------------------------
    mv a0, s8
    mv a1, s9
    mv a2, s10
    mv a3, s11
    call validate_solution

    li t0, 1
    bne a0, t0, solution_validation_failed

    # ========================================================
    # Test case 2: solved state
    #
    # p   = 0
    # o   = 0
    # pos = [0,1,2,3] = 0x03020100
    # ori = 0
    #
    # Expected optimal length = 0
    # ========================================================
    li a0, 0
    li a1, 0
    li a2, 0x03020100
    li a3, 0
    call solve_ida

    li t0, 1
    bne a0, t0, solved_test_failed

    la t0, ida_solution_length
    lbu t1, 0(t0)
    bne t1, x0, solved_test_failed

    # Validate the zero-move solution as well.
    li a0, 0
    li a1, 0
    li a2, 0x03020100
    li a3, 0
    call validate_solution

    li t0, 1
    bne a0, t0, solved_test_failed


    # ========================================================
    # Test case 3: one-move short scramble
    #
    # Generate it by applying R once to solved.
    # Expected optimal answer = R' = move 2
    # Expected length = 1
    # ========================================================
    li a0, 0
    li a1, 0
    li a2, 0x03020100
    li a3, 0
    li a4, 0                  # R
    call apply_move

    # Preserve the generated scrambled state.
    mv s4, a0
    mv s5, a1
    mv s6, a2
    mv s7, a3

    # Solve the short scramble.
    mv a0, s4
    mv a1, s5
    mv a2, s6
    mv a3, s7
    call solve_ida

    li t0, 1
    bne a0, t0, short_test_failed

    # Optimal length must be 1.
    la t0, ida_solution_length
    lbu t1, 0(t0)
    li t2, 1
    bne t1, t2, short_test_failed

    # The inverse of R is R' = move 2.
    la t0, ida_path
    lbu t1, 0(t0)
    li t2, 2
    bne t1, t2, short_test_failed

    # Reapply returned path and verify solved state.
    mv a0, s4
    mv a1, s5
    mv a2, s6
    mv a3, s7
    call validate_solution

    li t0, 1
    bne a0, t0, short_test_failed

    # All tests passed.
    li a0, 0
    li a7, 10
    ecall

verify_failed:
    li a0, 3
    li a7, 10
    ecall

rank_orientation_failed:
    li a0, 4
    li a7, 10
    ecall

rank_permutation_failed:
    li a0, 5
    li a7, 10
    ecall

extract_pattern_failed:
    li a0, 6
    li a7, 10
    ecall

rank_4corner_failed:
    li a0, 7
    li a7, 10
    ecall

heuristic_failed:
    li a0, 8
    li a7, 10
    ecall

transition_failed:
    li a0, 9
    li a7, 10
    ecall

quarter_turn_failed:
    li a0, 10
    li a7, 10
    ecall

root_frame_failed:
    li a0, 11
    li a7, 10
    ecall

apply_move_failed:
    li a0, 12
    li a7, 10
    ecall

ida_dfs_failed:
    li a0, 13
    li a7, 10
    ecall

solve_ida_failed:
    li a0, 14
    li a7, 10
    ecall

solution_validation_failed:
    li a0, 15
    li a7, 10
    ecall

solved_test_failed:
    li a0, 16
    li a7, 10
    ecall

short_test_failed:
    li a0, 17
    li a7, 10
    ecall

parse_failed:
    li a0, 1
    li a7, 10
    ecall


# ------------------------------------------------------------
# parse_state
#
# Input:
#   a0 = pointer to input_state
#   a1 = pointer to state_p[7]
#   a2 = pointer to state_o[7]
#
# Return:
#   a0 = 1 success
#   a0 = 0 invalid character/range
#
# Version 1:
#   Only converts and range-checks the 14 characters.
#   Full permutation uniqueness and orientation-sum validation
#   will be added separately.
# ------------------------------------------------------------
parse_state:
    mv t0, a0          # input pointer
    mv t1, a1          # state_p pointer
    mv t2, a2          # state_o pointer

    li t3, 0           # i = 0

parse_perm_loop:
    li t4, 7
    bge t3, t4, parse_ori_begin

    lbu t5, 0(t0)

    # permutation characters must be '1' ... '7'
    li t6, '1'
    blt t5, t6, parse_invalid

    li t6, '7'
    bgt t5, t6, parse_invalid

    addi t5, t5, -'1'
    sb t5, 0(t1)

    addi t0, t0, 1
    addi t1, t1, 1
    addi t3, t3, 1

    j parse_perm_loop


parse_ori_begin:
    li t3, 0

parse_ori_loop:
    li t4, 7
    bge t3, t4, parse_done

    lbu t5, 0(t0)

    # orientation characters must be '1' ... '3'
    li t6, '1'
    blt t5, t6, parse_invalid

    li t6, '3'
    bgt t5, t6, parse_invalid

    addi t5, t5, -'1'
    sb t5, 0(t2)

    addi t0, t0, 1
    addi t2, t2, 1
    addi t3, t3, 1

    j parse_ori_loop


parse_done:
    li a0, 1
    ret

parse_invalid:
    li a0, 0
    ret


# ------------------------------------------------------------
# verify_parsed_state
#
# Return:
#   a0 = 1 if parsed state is correct
#   a0 = 0 otherwise
# ------------------------------------------------------------
verify_parsed_state:
    la t0, state_p
    la t1, expected_p
    li t2, 7

verify_p_loop:
    lbu t3, 0(t0)
    lbu t4, 0(t1)
    bne t3, t4, verify_bad

    addi t0, t0, 1
    addi t1, t1, 1
    addi t2, t2, -1
    bne t2, x0, verify_p_loop

    la t0, state_o
    la t1, expected_o
    li t2, 7

verify_o_loop:
    lbu t3, 0(t0)
    lbu t4, 0(t1)
    bne t3, t4, verify_bad

    addi t0, t0, 1
    addi t1, t1, 1
    addi t2, t2, -1
    bne t2, x0, verify_o_loop

    li a0, 1
    ret

verify_bad:
    li a0, 0
    ret

# ------------------------------------------------------------
# rank_orientation
#
# Input:
#   a0 = pointer to orientation[7]
#
# Return:
#   a0 = orientation rank, range 0..728
#
# Formula:
#   rank = rank * 3 + orientation[i]
#   for i = 0..5
#
# The seventh orientation is dependent on the first six,
# so it is not included in the rank.
# ------------------------------------------------------------
rank_orientation:
    mv t0, a0          # t0 = orientation pointer
    li t1, 6           # process first six orientations
    li t2, 0           # rank = 0

rank_orientation_loop:
    lbu t3, 0(t0)      # t3 = orientation[i]

    # rank * 3 = (rank << 1) + rank
    slli t4, t2, 1
    add t2, t4, t2

    # rank = rank * 3 + orientation[i]
    add t2, t2, t3

    addi t0, t0, 1
    addi t1, t1, -1
    bne t1, x0, rank_orientation_loop

    mv a0, t2
    ret

# ------------------------------------------------------------
# rank_permutation
#
# Input:
#   a0 = pointer to permutation[7]
#
# Return:
#   a0 = permutation rank, range 0..5039
#
# For each i:
#   smaller = number of elements to the right that are
#             smaller than permutation[i]
#
#   rank = rank * (7 - i) + smaller
#
# RV32I has no MUL, so multiplications by 7..1
# are implemented with shifts and adds/subtracts.
# ------------------------------------------------------------
rank_permutation:
    mv t0, a0          # t0 = base pointer
    li t1, 0           # i = 0
    li t2, 0           # rank = 0

rank_perm_outer:
    li t3, 7
    bge t1, t3, rank_perm_done

    # current = permutation[i]
    add t4, t0, t1
    lbu t4, 0(t4)

    li t5, 0           # smaller = 0
    addi t6, t1, 1     # j = i + 1

rank_perm_inner:
    li a1, 7
    bge t6, a1, rank_perm_scale

    add a2, t0, t6
    lbu a2, 0(a2)

    # Increment smaller when the right-side value is smaller.
    bge a2, t4, rank_perm_no_inc
    addi t5, t5, 1

rank_perm_no_inc:
    addi t6, t6, 1
    j rank_perm_inner


rank_perm_scale:
    # Multiply current rank by (7 - i).
    #
    # i=0 -> x7
    # i=1 -> x6
    # i=2 -> x5
    # i=3 -> x4
    # i=4 -> x3
    # i=5 -> x2
    # i=6 -> x1

    beq t1, x0, rank_mul7

    li a1, 1
    beq t1, a1, rank_mul6

    li a1, 2
    beq t1, a1, rank_mul5

    li a1, 3
    beq t1, a1, rank_mul4

    li a1, 4
    beq t1, a1, rank_mul3

    li a1, 5
    beq t1, a1, rank_mul2

    # i = 6 -> rank * 1
    j rank_add_smaller


rank_mul7:
    slli a1, t2, 3     # rank * 8
    sub t2, a1, t2     # rank * 7
    j rank_add_smaller

rank_mul6:
    slli a1, t2, 2     # rank * 4
    slli a2, t2, 1     # rank * 2
    add t2, a1, a2     # rank * 6
    j rank_add_smaller

rank_mul5:
    slli a1, t2, 2     # rank * 4
    add t2, a1, t2     # rank * 5
    j rank_add_smaller

rank_mul4:
    slli t2, t2, 2
    j rank_add_smaller

rank_mul3:
    slli a1, t2, 1
    add t2, a1, t2
    j rank_add_smaller

rank_mul2:
    slli t2, t2, 1

rank_add_smaller:
    add t2, t2, t5

    addi t1, t1, 1
    j rank_perm_outer


rank_perm_done:
    mv a0, t2
    ret
# ------------------------------------------------------------
# extract_4corner_pattern
#
# Input:
#   a0 = pointer to permutation[7]
#   a1 = pointer to orientation[7]
#
# Return:
#   a0 = packed positions of cubies 0..3
#   a1 = packed orientations of cubies 0..3
#
# Each result contains four 8-bit fields.
#
# Example:
#   positions [1,0,2,3]
#   -> 0x03020001
# ------------------------------------------------------------
extract_4corner_pattern:
    mv t0, a0          # permutation pointer
    mv t1, a1          # orientation pointer

    li t2, 0           # current board position
    li t3, 0           # packed pattern positions
    li t4, 0           # packed pattern orientations
    li t5, 7           # seven board positions

extract_pattern_loop:
    lbu t6, 0(t0)      # cubie at current board position

    # Only cubies 0,1,2,3 belong to this pattern.
    li a2, 4
    bgeu t6, a2, extract_pattern_skip

    # Each cubie owns one byte in the packed result.
    # shift = cubie * 8
    slli a3, t6, 3

    # Store current board position in the cubie's byte.
    sll a4, t2, a3
    or t3, t3, a4

    # Store its orientation in the corresponding byte.
    lbu a4, 0(t1)
    sll a4, a4, a3
    or t4, t4, a4

extract_pattern_skip:
    addi t0, t0, 1
    addi t1, t1, 1
    addi t2, t2, 1
    addi t5, t5, -1
    bne t5, x0, extract_pattern_loop

    mv a0, t3
    mv a1, t4
    ret

# ------------------------------------------------------------
# rank_4corner_components
#
# Input:
#   a0 = packed positions of cubies 0..3
#   a1 = packed orientations of cubies 0..3
#
# Return:
#   a0 = 4-corner pattern rank
#
# position_rank =
#   (((q0 * 6 + q1) * 5 + q2) * 4 + q3)
#
# orientation_rank =
#   (((o0 * 3 + o1) * 3 + o2) * 3 + o3)
#
# final_rank = position_rank * 81 + orientation_rank
# ------------------------------------------------------------
rank_4corner_components:

    # Unpack p0, p1, p2, p3.
    andi t0, a0, 255

    srli t1, a0, 8
    andi t1, t1, 255

    srli t2, a0, 16
    andi t2, t2, 255

    srli t3, a0, 24

    # q0 = p0
    # q1 = p1 - (p0 < p1)
    sltu a2, t0, t1
    sub t5, t1, a2

    # q2 = p2 - (p0 < p2) - (p1 < p2)
    sltu a2, t0, t2
    sltu a3, t1, t2
    sub t6, t2, a2
    sub t6, t6, a3

    # q3 = p3 - comparisons with p0, p1, p2
    sltu a2, t0, t3
    sltu a3, t1, t3
    sltu a4, t2, t3

    sub a5, t3, a2
    sub a5, a5, a3
    sub a5, a5, a4

    # position_rank = q0 * 6 + q1
    slli a2, t0, 2
    slli a3, t0, 1
    add t4, a2, a3
    add t4, t4, t5

    # position_rank = position_rank * 5 + q2
    slli a2, t4, 2
    add t4, a2, t4
    add t4, t4, t6

    # position_rank = position_rank * 4 + q3
    slli t4, t4, 2
    add t4, t4, a5


    # Unpack orientation o0.
    andi t5, a1, 255

    # orientation_rank = o0 * 3 + o1
    srli t6, a1, 8
    andi t6, t6, 255

    slli a2, t5, 1
    add t5, a2, t5
    add t5, t5, t6

    # orientation_rank = orientation_rank * 3 + o2
    srli t6, a1, 16
    andi t6, t6, 255

    slli a2, t5, 1
    add t5, a2, t5
    add t5, t5, t6

    # orientation_rank = orientation_rank * 3 + o3
    srli t6, a1, 24

    slli a2, t5, 1
    add t5, a2, t5
    add t5, t5, t6


    # final_rank = position_rank * 81 + orientation_rank
    # 81 = 64 + 16 + 1
    slli a2, t4, 6
    slli a3, t4, 4
    add a2, a2, a3
    add a2, a2, t4

    add a0, a2, t5
    ret

# ------------------------------------------------------------
# heuristic
#
# Input:
#   a0 = permutation rank       0..5039
#   a1 = orientation rank       0..728
#   a2 = 4-corner pattern rank  0..68039
#
# Return:
#   a0 = max(
#          permutation_pdb[p],
#          orientation_pdb[o],
#          corner4_pdb[c]
#        )
# ------------------------------------------------------------
heuristic:
    # hp = permutation_pdb[p]
    la t0, permutation_pdb
    add t0, t0, a0
    lbu t1, 0(t0)

    # ho = orientation_pdb[o]
    la t0, orientation_pdb
    add t0, t0, a1
    lbu t2, 0(t0)

    # hc = corner4_pdb[c]
    la t0, corner4_pdb
    add t0, t0, a2
    lbu t3, 0(t0)

    # h = max(hp, ho)
    bgeu t1, t2, heuristic_hp_ge_ho
    mv t1, t2

heuristic_hp_ge_ho:
    # h = max(h, hc)
    bgeu t1, t3, heuristic_done
    mv t1, t3

heuristic_done:
    mv a0, t1
    ret

# ------------------------------------------------------------
# permutation_next
#
# Input:
#   a0 = face, 0=R, 1=B, 2=D
#   a1 = permutation rank
#
# Return:
#   a0 = rank after one quarter turn
#
# Each entry is uint16_t.
# One face occupies:
#   5040 * 2 = 10080 bytes
# ------------------------------------------------------------
permutation_next:
    la t0, permutation_transition

    beq a0, x0, permutation_face_ready

    li t1, 1
    beq a0, t1, permutation_face_1

    # face 2
    li t1, 20160
    add t0, t0, t1
    j permutation_face_ready

permutation_face_1:
    li t1, 10080
    add t0, t0, t1

permutation_face_ready:
    # uint16_t entry => rank * 2
    slli t1, a1, 1
    add t0, t0, t1

    lhu a0, 0(t0)
    ret


# ------------------------------------------------------------
# orientation_next
#
# Input:
#   a0 = face, 0=R, 1=B, 2=D
#   a1 = orientation rank
#
# Return:
#   a0 = rank after one quarter turn
#
# Each entry is uint16_t.
# One face occupies:
#   729 * 2 = 1458 bytes
# ------------------------------------------------------------
orientation_next:
    la t0, orientation_transition

    beq a0, x0, orientation_face_ready

    li t1, 1
    beq a0, t1, orientation_face_1

    # face 2
    li t1, 2916
    add t0, t0, t1
    j orientation_face_ready

orientation_face_1:
    li t1, 1458
    add t0, t0, t1

orientation_face_ready:
    # uint16_t entry => rank * 2
    slli t1, a1, 1
    add t0, t0, t1

    lhu a0, 0(t0)
    ret

# ------------------------------------------------------------
# quarter_turn_pattern
#
# Input:
#   a0 = packed positions of cubies 0..3
#   a1 = packed orientations of cubies 0..3
#   a2 = face, 0=R, 1=B, 2=D
#
# Return:
#   a0 = updated packed positions
#   a1 = updated packed orientations
# ------------------------------------------------------------
quarter_turn_pattern:
    mv t0, a0              # original packed positions
    mv t1, a1              # original packed orientations

    li t2, 0               # cubie = 0
    li t3, 0               # bit shift = 0
    li t4, 0               # result positions
    li t5, 0               # result orientations

    # face offset = face * 7 = face * 8 - face
    slli a3, a2, 3
    sub a3, a3, a2

quarter_turn_loop:
    # Extract old position of this cubie.
    srl t6, t0, t3
    andi t6, t6, 255

    # destination = destination_of[face][old_position]
    la a4, destination_of
    add a4, a4, a3
    add a4, a4, t6
    lbu a5, 0(a4)

    # Put destination into this cubie's output byte.
    sll a7, a5, t3
    or t4, t4, a7

    # Extract old orientation.
    srl a6, t1, t3
    andi a6, a6, 255

    # twist value is indexed by destination position.
    la a4, twist
    add a4, a4, a3
    add a4, a4, a5
    lbu a7, 0(a4)

    add a6, a6, a7

    # Reduce modulo 3.
    li a7, 3
    bltu a6, a7, quarter_turn_no_reduce
    addi a6, a6, -3

quarter_turn_no_reduce:
    # Put new orientation into this cubie's output byte.
    sll a7, a6, t3
    or t5, t5, a7

    addi t2, t2, 1
    addi t3, t3, 8

    li a7, 4
    blt t2, a7, quarter_turn_loop

    mv a0, t4
    mv a1, t5
    ret

# ------------------------------------------------------------
# init_root_frame
#
# Input:
#   a0 = permutation rank
#   a1 = orientation rank
#   a2 = packed pattern positions
#   a3 = packed pattern orientations
#
# Initializes ida_stack[0].
# ------------------------------------------------------------
init_root_frame:
    la t0, ida_stack

    sh a0, FRAME_P(t0)
    sh a1, FRAME_O(t0)
    sw a2, FRAME_PATTERN_POS(t0)
    sw a3, FRAME_PATTERN_ORI(t0)

    li t1, -1
    sb t1, FRAME_PREV_FACE(t0)

    sb x0, FRAME_NEXT_MOVE(t0)

    ret


# ------------------------------------------------------------
# verify_root_frame
#
# Return:
#   a0 = 1 if root frame contains expected test values
#   a0 = 0 otherwise
# ------------------------------------------------------------
verify_root_frame:
    la t0, ida_stack

    lhu t1, FRAME_P(t0)
    li t2, 720
    bne t1, t2, verify_root_bad

    lhu t1, FRAME_O(t0)
    bne t1, x0, verify_root_bad

    lw t1, FRAME_PATTERN_POS(t0)
    li t2, 0x03020001
    bne t1, t2, verify_root_bad

    lw t1, FRAME_PATTERN_ORI(t0)
    bne t1, x0, verify_root_bad

    lb t1, FRAME_PREV_FACE(t0)
    li t2, -1
    bne t1, t2, verify_root_bad

    lbu t1, FRAME_NEXT_MOVE(t0)
    bne t1, x0, verify_root_bad

    li a0, 1
    ret

verify_root_bad:
    li a0, 0
    ret

# ------------------------------------------------------------
# apply_move
#
# Input:
#   a0 = permutation rank
#   a1 = orientation rank
#   a2 = packed pattern positions
#   a3 = packed pattern orientations
#   a4 = move, 0..8
#
# Move encoding:
#   0 R     1 R2     2 R'
#   3 B     4 B2     5 B'
#   6 D     7 D2     8 D'
#
# Return:
#   a0 = new permutation rank
#   a1 = new orientation rank
#   a2 = new packed pattern positions
#   a3 = new packed pattern orientations
# ------------------------------------------------------------
apply_move:
    # This function calls other functions, so preserve ra
    # and the callee-saved registers we use.
    addi sp, sp, -32

    sw ra, 28(sp)
    sw s0, 24(sp)
    sw s1, 20(sp)
    sw s2, 16(sp)
    sw s3, 12(sp)
    sw s4, 8(sp)
    sw s5, 4(sp)

    mv s0, a0          # current permutation rank
    mv s1, a1          # current orientation rank
    mv s2, a2          # current packed positions
    mv s3, a3          # current packed orientations

    # Decode move into:
    #   s4 = face
    #   s5 = number of quarter turns

    li t0, 3
    bltu a4, t0, apply_move_face_r

    li t0, 6
    bltu a4, t0, apply_move_face_b

    # D / D2 / D'
    li s4, 2
    addi s5, a4, -5
    j apply_move_loop

apply_move_face_b:
    li s4, 1
    addi s5, a4, -2
    j apply_move_loop

apply_move_face_r:
    li s4, 0
    addi s5, a4, 1


apply_move_loop:
    # permutation quarter turn
    mv a0, s4
    mv a1, s0
    call permutation_next
    mv s0, a0

    # orientation quarter turn
    mv a0, s4
    mv a1, s1
    call orientation_next
    mv s1, a0

    # 4-corner pattern quarter turn
    mv a0, s2
    mv a1, s3
    mv a2, s4
    call quarter_turn_pattern
    mv s2, a0
    mv s3, a1

    addi s5, s5, -1
    bne s5, x0, apply_move_loop

    # Return updated state.
    mv a0, s0
    mv a1, s1
    mv a2, s2
    mv a3, s3

    # Restore caller state.
    lw s5, 4(sp)
    lw s4, 8(sp)
    lw s3, 12(sp)
    lw s2, 16(sp)
    lw s1, 20(sp)
    lw s0, 24(sp)
    lw ra, 28(sp)

    addi sp, sp, 32
    ret

# ------------------------------------------------------------
# ida_dfs_iterative
#
# Input:
#   a0 = permutation rank
#   a1 = orientation rank
#   a2 = packed pattern positions
#   a3 = packed pattern orientations
#   a4 = search bound
#
# Return:
#   a0 = 1 if a solution is found
#   a0 = 0 otherwise
#
# ida_path[] and ida_solution_length are written on success.
# ------------------------------------------------------------
ida_dfs_iterative:
    addi sp, sp, -48

    sw ra, 44(sp)
    sw s0, 40(sp)
    sw s1, 36(sp)
    sw s2, 32(sp)
    sw s3, 28(sp)
    sw s4, 24(sp)
    sw s5, 20(sp)
    sw s6, 16(sp)
    sw s7, 12(sp)

    # s0 = stack top
    # s1 = search bound
    li s0, 0
    mv s1, a4

    # Initialize root frame.
    call init_root_frame

    # --------------------------------------------------------
    # Check heuristic of root.
    # --------------------------------------------------------
    la t0, ida_stack

    lhu s2, FRAME_P(t0)
    lhu s3, FRAME_O(t0)
    lw  s4, FRAME_PATTERN_POS(t0)
    lw  s5, FRAME_PATTERN_ORI(t0)

    # Compute corner4 rank.
    mv a0, s4
    mv a1, s5
    call rank_4corner_components

    mv a2, a0
    mv a0, s2
    mv a1, s3
    call heuristic

    # Prune when h > bound.
    bltu s1, a0, ida_dfs_not_found

    # Root already solved?
    bne s2, x0, ida_dfs_loop
    bne s3, x0, ida_dfs_loop

    la t0, ida_solution_length
    sb x0, 0(t0)

    li a0, 1
    j ida_dfs_return


# ------------------------------------------------------------
# Main explicit-stack DFS loop
# ------------------------------------------------------------
ida_dfs_loop:
    # top < 0 means the root has been popped.
    blt s0, x0, ida_dfs_not_found

    # frame = &ida_stack[top]
    slli t0, s0, 4
    la t1, ida_stack
    add t1, t1, t0

    # All 9 moves already examined?
    lbu t2, FRAME_NEXT_MOVE(t1)

    li t3, 9
    bgeu t2, t3, ida_dfs_pop

    # move = frame->next_move++
    addi t3, t2, 1
    sb t3, FRAME_NEXT_MOVE(t1)

    mv s6, t2                 # s6 = move


# ------------------------------------------------------------
# Decode move -> face
#
# 0,1,2 => R / R2 / R'
# 3,4,5 => B / B2 / B'
# 6,7,8 => D / D2 / D'
# ------------------------------------------------------------
    li t3, 3
    bltu s6, t3, ida_face_r

    li t3, 6
    bltu s6, t3, ida_face_b

    li s7, 2
    j ida_face_ready

ida_face_b:
    li s7, 1
    j ida_face_ready

ida_face_r:
    li s7, 0


ida_face_ready:
    # Skip consecutive moves on the same face.
    lb t3, FRAME_PREV_FACE(t1)
    beq t3, s7, ida_dfs_loop

    # Load state from current frame.
    lhu a0, FRAME_P(t1)
    lhu a1, FRAME_O(t1)
    lw  a2, FRAME_PATTERN_POS(t1)
    lw  a3, FRAME_PATTERN_ORI(t1)
    mv  a4, s6

    call apply_move

    # Preserve generated child state.
    mv s2, a0
    mv s3, a1
    mv s4, a2
    mv s5, a3


# ------------------------------------------------------------
# Evaluate child heuristic.
# ------------------------------------------------------------
    mv a0, s4
    mv a1, s5
    call rank_4corner_components

    mv a2, a0
    mv a0, s2
    mv a1, s3
    call heuristic

    # depth = top + 1
    addi t0, s0, 1

    # f = depth + h
    add t2, t0, a0

    # Prune when f > bound.
    bltu s1, t2, ida_dfs_loop


# ------------------------------------------------------------
# Record the path move.
# ------------------------------------------------------------
    la t1, ida_path
    add t1, t1, s0
    sb s6, 0(t1)


# ------------------------------------------------------------
# Goal test.
# ------------------------------------------------------------
    bne s2, x0, ida_dfs_not_goal
    bne s3, x0, ida_dfs_not_goal

    la t1, ida_solution_length
    sb t0, 0(t1)

    li a0, 1
    j ida_dfs_return


ida_dfs_not_goal:
    # An unsolved child exactly at the bound cannot descend.
    beq t0, s1, ida_dfs_loop


# ------------------------------------------------------------
# Push child frame.
# ------------------------------------------------------------
    addi s0, s0, 1

    slli t1, s0, 4
    la t2, ida_stack
    add t1, t2, t1

    sh s2, FRAME_P(t1)
    sh s3, FRAME_O(t1)

    sw s4, FRAME_PATTERN_POS(t1)
    sw s5, FRAME_PATTERN_ORI(t1)

    sb s7, FRAME_PREV_FACE(t1)
    sb x0, FRAME_NEXT_MOVE(t1)

    j ida_dfs_loop


# ------------------------------------------------------------
# Pop exhausted frame.
# ------------------------------------------------------------
ida_dfs_pop:
    addi s0, s0, -1
    j ida_dfs_loop


ida_dfs_not_found:
    li a0, 0


ida_dfs_return:
    lw s7, 12(sp)
    lw s6, 16(sp)
    lw s5, 20(sp)
    lw s4, 24(sp)
    lw s3, 28(sp)
    lw s2, 32(sp)
    lw s1, 36(sp)
    lw s0, 40(sp)
    lw ra, 44(sp)

    addi sp, sp, 48
    ret

# ------------------------------------------------------------
# solve_ida
#
# Input:
#   a0 = permutation rank
#   a1 = orientation rank
#   a2 = packed pattern positions
#   a3 = packed pattern orientations
#
# Return:
#   a0 = 1 if solved
#   a0 = 0 if no solution <= MAX_DEPTH
#
# Starts at the admissible heuristic value and increases
# the IDA* bound until a solution is found.
# ------------------------------------------------------------
solve_ida:
    addi sp, sp, -32

    sw ra, 28(sp)
    sw s0, 24(sp)
    sw s1, 20(sp)
    sw s2, 16(sp)
    sw s3, 12(sp)
    sw s4, 8(sp)

    # Preserve root state.
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv s3, a3

    # Compute initial bound = heuristic(root).
    mv a0, s2
    mv a1, s3
    call rank_4corner_components

    mv a2, a0
    mv a0, s0
    mv a1, s1
    call heuristic

    mv s4, a0              # current IDA* bound


solve_ida_loop:
    # Stop if bound > MAX_DEPTH.
    li t0, MAX_DEPTH
    bltu t0, s4, solve_ida_not_found

    # Search at the current bound.
    mv a0, s0
    mv a1, s1
    mv a2, s2
    mv a3, s3
    mv a4, s4

    call ida_dfs_iterative

    bne a0, x0, solve_ida_found

    # No solution at this bound; deepen by one.
    addi s4, s4, 1
    j solve_ida_loop


solve_ida_found:
    li a0, 1
    j solve_ida_return


solve_ida_not_found:
    li a0, 0


solve_ida_return:
    lw s4, 8(sp)
    lw s3, 12(sp)
    lw s2, 16(sp)
    lw s1, 20(sp)
    lw s0, 24(sp)
    lw ra, 28(sp)

    addi sp, sp, 32
    ret

# ------------------------------------------------------------
# validate_solution
#
# Input:
#   a0 = original permutation rank
#   a1 = original orientation rank
#   a2 = original packed pattern positions
#   a3 = original packed pattern orientations
#
# Uses:
#   ida_path[]
#   ida_solution_length
#
# Return:
#   a0 = 1 if applying every move reaches solved state
#   a0 = 0 otherwise
# ------------------------------------------------------------
validate_solution:
    addi sp, sp, -32

    sw ra, 28(sp)
    sw s0, 24(sp)
    sw s1, 20(sp)
    sw s2, 16(sp)
    sw s3, 12(sp)
    sw s4, 8(sp)
    sw s5, 4(sp)

    # Current state.
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv s3, a3

    # s4 = path index
    li s4, 0

    # s5 = solution length
    la t0, ida_solution_length
    lbu s5, 0(t0)


validate_solution_loop:
    # Finished applying all moves?
    bgeu s4, s5, validate_solution_check

    # move = ida_path[s4]
    la t0, ida_path
    add t0, t0, s4
    lbu a4, 0(t0)

    # Apply this move to current state.
    mv a0, s0
    mv a1, s1
    mv a2, s2
    mv a3, s3

    call apply_move

    # Save updated state.
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv s3, a3

    addi s4, s4, 1
    j validate_solution_loop


validate_solution_check:
    # Full cube state must be solved.
    bne s0, x0, validate_solution_bad
    bne s1, x0, validate_solution_bad

    # Also verify that the 4-corner abstraction agrees
    # with the solved state.
    li t0, 0x03020100
    bne s2, t0, validate_solution_bad
    bne s3, x0, validate_solution_bad

    li a0, 1
    j validate_solution_return


validate_solution_bad:
    li a0, 0


validate_solution_return:
    lw s5, 4(sp)
    lw s4, 8(sp)
    lw s3, 12(sp)
    lw s2, 16(sp)
    lw s1, 20(sp)
    lw s0, 24(sp)
    lw ra, 28(sp)

    addi sp, sp, 32
    ret
