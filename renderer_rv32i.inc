# ============================================================
# LED renderer for Ripes
#
# Uses:
#   state_p[7]
#   state_o[7]
#
# Face IDs:
#   0 = U = white
#   1 = L = orange
#   2 = F = green
#   3 = R = red
#   4 = B = blue
#   5 = D = yellow
# ============================================================


# ------------------------------------------------------------
# Renderer data
# ------------------------------------------------------------

.section .bss
.align 2

render_tmp_p:
    .space 7

render_tmp_o:
    .space 7

.section .rodata
.align 2

render_face_colors:
    .word 0x00FFFFFF      # U
    .word 0x00FF8000      # L
    .word 0x0000FF00      # F
    .word 0x00FF0000      # R
    .word 0x000000FF      # B
    .word 0x00FFFF00      # D


# Ordered face triples for report corners 0..7.
render_corner_faces:
    .byte 0, 1, 2         # 0 = FUL
    .byte 0, 2, 3         # 1 = FUR
    .byte 5, 3, 2         # 2 = FDR
    .byte 5, 2, 1         # 3 = FDL
    .byte 0, 3, 4         # 4 = BUR
    .byte 5, 4, 3         # 5 = BDR
    .byte 5, 1, 4         # 6 = BDL
    .byte 0, 4, 1         # 7 = BUL


# Corner position shown at each visible facelet.
#
# Four entries per face:
#
#   0 1
#   2 3
#
render_face_positions:
    .byte 7, 4, 0, 1      # U
    .byte 7, 0, 6, 3      # L
    .byte 0, 1, 3, 2      # F
    .byte 1, 4, 2, 5      # R
    .byte 4, 7, 5, 6      # B
    .byte 3, 2, 6, 5      # D


# Top-left position of each complete 2x2 face.
render_face_origins:
    .byte  9,  0           # U
    .byte  0,  7           # L
    .byte  9,  7           # F
    .byte 18,  7           # R
    .byte 27,  7           # B
    .byte  9, 14           # D


.section .text


# ============================================================
# render_solution
#
# For this integration checkpoint, only draw the initial state.
# Animation will be added after this is verified.
# ============================================================

render_solution:
    addi sp, sp, -16

    sw ra, 12(sp)
    sw s0, 8(sp)
    sw s1, 4(sp)

    # -----------------------------------------
    # Draw the original scrambled state first.
    # -----------------------------------------
    call render_draw_cube
    call render_delay

    # Number of moves returned by IDA*.
    la t0, ida_solution_length
    lbu s1, 0(t0)

    li s0, 0               # path index


render_solution_loop:
    bgeu s0, s1, render_solution_done

    # move = ida_path[index]
    la t0, ida_path
    add t0, t0, s0
    lbu a0, 0(t0)

    # Apply the actual solver move to state_p/state_o.
    call render_apply_move

    # Draw the new cube state.
    call render_draw_cube

    # Keep this frame visible.
    call render_delay

    addi s0, s0, 1
    j render_solution_loop


render_solution_done:
    lw s1, 4(sp)
    lw s0, 8(sp)
    lw ra, 12(sp)

    addi sp, sp, 16
    ret

# ============================================================
# render_get_sticker_color
#
# a0 = report corner position 0..7
# a1 = requested face 0..5
#
# return:
#   a0 = 0x00RRGGBB
# ============================================================
render_get_sticker_color:
    mv t0, a0              # report position
    mv t1, a1              # requested face

    # Fixed FUL corner.
    bne t0, x0, render_get_moving

    li t4, 0               # report cubie 0
    li t5, 0               # orientation 0
    j render_get_have_cubie


render_get_moving:
    # state_p/state_o use internal positions 0..6,
    # corresponding to report positions 1..7.
    addi t2, t0, -1

    la t3, state_p
    add t3, t3, t2
    lbu t4, 0(t3)

    # internal cubie 0..6 -> report cubie 1..7
    addi t4, t4, 1

    la t3, state_o
    add t3, t3, t2
    lbu t5, 0(t3)


render_get_have_cubie:
    # Find local sticker slot j on this corner position.
    la t2, render_corner_faces

    # position * 3
    slli t3, t0, 1
    add  t3, t3, t0
    add  t2, t2, t3

    li t6, 0


render_get_find_slot:
    add a2, t2, t6
    lbu a3, 0(a2)

    beq a3, t1, render_get_slot_found

    addi t6, t6, 1
    j render_get_find_slot


render_get_slot_found:
    # home_slot = j - orientation (mod 3)
    sub a2, t6, t5

    bge a2, x0, render_get_slot_nonnegative

    addi a2, a2, 3


render_get_slot_nonnegative:
    # Find home face of that cubie sticker.
    la a3, render_corner_faces

    slli a4, t4, 1
    add  a4, a4, t4        # cubie * 3

    add a3, a3, a4
    add a3, a3, a2

    lbu a4, 0(a3)

    # Convert face ID to RGB.
    la a3, render_face_colors
    slli a4, a4, 2
    add a3, a3, a4

    lw a0, 0(a3)
    ret


# ============================================================
# render_draw_cube
#
# Draw all 24 stickers from state_p/state_o.
# ============================================================
render_draw_cube:
    addi sp, sp, -48

    sw ra, 44(sp)
    sw s0, 40(sp)
    sw s1, 36(sp)
    sw s2, 32(sp)
    sw s3, 28(sp)
    sw s4, 24(sp)
    sw s5, 20(sp)

    li s0, 0               # face


render_cube_face_loop:
    li t0, 6
    bge s0, t0, render_cube_done

    # Load face origin.
    la t0, render_face_origins
    slli t1, s0, 1
    add t0, t0, t1

    lbu s2, 0(t0)          # base x
    lbu s3, 1(t0)          # base y

    li s1, 0               # sticker


render_cube_sticker_loop:
    li t0, 4
    bge s1, t0, render_cube_next_face

    # report corner position
    la t0, render_face_positions

    slli t1, s0, 2
    add t1, t1, s1
    add t0, t0, t1

    lbu s4, 0(t0)

    # Obtain actual sticker color.
    mv a0, s4
    mv a1, s0
    call render_get_sticker_color

    mv s5, a0

    # x = base_x + (sticker & 1) * 4
    andi t0, s1, 1
    slli t0, t0, 2
    add a0, s2, t0

    # y = base_y + (sticker >> 1) * 3
    srli t0, s1, 1

    beq t0, x0, render_cube_top_row

    addi a1, s3, 3
    j render_cube_have_y


render_cube_top_row:
    mv a1, s3


render_cube_have_y:
    mv a2, s5
    call render_draw_facelet

    addi s1, s1, 1
    j render_cube_sticker_loop


render_cube_next_face:
    addi s0, s0, 1
    j render_cube_face_loop


render_cube_done:
    lw s5, 20(sp)
    lw s4, 24(sp)
    lw s3, 28(sp)
    lw s2, 32(sp)
    lw s1, 36(sp)
    lw s0, 40(sp)
    lw ra, 44(sp)

    addi sp, sp, 48
    ret


# ============================================================
# render_draw_facelet
#
# a0 = top-left x
# a1 = top-left y
# a2 = RGB
#
# Draw 4 x 3 pixels.
# ============================================================
render_draw_facelet:
    addi sp, sp, -32

    sw ra, 28(sp)
    sw s0, 24(sp)
    sw s1, 20(sp)
    sw s2, 16(sp)
    sw s3, 12(sp)
    sw s4, 8(sp)

    mv s0, a0
    mv s1, a1
    mv s2, a2

    li s3, 0


render_facelet_row:
    li s4, 0


render_facelet_col:
    add a0, s0, s4
    add a1, s1, s3
    mv  a2, s2

    call render_put_pixel

    addi s4, s4, 1

    li t0, 4
    blt s4, t0, render_facelet_col

    addi s3, s3, 1

    li t0, 3
    blt s3, t0, render_facelet_row

    lw s4, 8(sp)
    lw s3, 12(sp)
    lw s2, 16(sp)
    lw s1, 20(sp)
    lw s0, 24(sp)
    lw ra, 28(sp)

    addi sp, sp, 32
    ret


# ============================================================
# render_put_pixel
#
# a0 = x
# a1 = y
# a2 = RGB
#
# offset = (y * 35 + x) * 4
# ============================================================
render_put_pixel:
    li t0, LED_MATRIX_0_BASE

    # y * 35 = y * 32 + y * 2 + y
    slli t1, a1, 5
    slli t2, a1, 1

    add t1, t1, t2
    add t1, t1, a1

    add t1, t1, a0

    # Four bytes per LED.
    slli t1, t1, 2

    add t0, t0, t1

    sw a2, 0(t0)
    ret

# ============================================================
# render_apply_move
#
# a0 = move code
#
# 0  R
# 1  R2
# 2  R'
# 3  B
# 4  B2
# 5  B'
# 6  D
# 7  D2
# 8  D'
# ============================================================
render_apply_move:
    addi sp, sp, -16

    sw ra, 12(sp)
    sw s0, 8(sp)
    sw s1, 4(sp)

    mv t0, a0

    li t1, 3
    bltu t0, t1, render_move_r

    li t1, 6
    bltu t0, t1, render_move_b

    # D / D2 / D'
    li s0, 2
    addi s1, t0, -5
    j render_move_turn_loop


render_move_b:
    li s0, 1
    addi s1, t0, -2
    j render_move_turn_loop


render_move_r:
    li s0, 0
    addi s1, t0, 1


render_move_turn_loop:
    mv a0, s0
    call render_quarter_turn_state

    addi s1, s1, -1
    bne s1, x0, render_move_turn_loop

    lw s1, 4(sp)
    lw s0, 8(sp)
    lw ra, 12(sp)

    addi sp, sp, 16
    ret


# ============================================================
# render_quarter_turn_state
#
# a0 = face
#      0 = R
#      1 = B
#      2 = D
#
# Mutates state_p/state_o by one quarter turn.
#
# destination_of[face][old_position] gives the new position.
# twist[face][destination] gives the orientation delta.
# ============================================================
render_quarter_turn_state:
    # face * 7 = face * 8 - face
    slli t0, a0, 3
    sub  t0, t0, a0

    # Face-specific destination table.
    la t1, destination_of
    add t1, t1, t0

    # Face-specific twist table.
    la t2, twist
    add t2, t2, t0

    li t3, 0               # old position


render_quarter_build_loop:
    # -----------------------------------------
    # Move cubie identity.
    # -----------------------------------------
    la t4, state_p
    add t4, t4, t3
    lbu t5, 0(t4)          # cubie at old position

    add t6, t1, t3
    lbu a1, 0(t6)          # destination position

    la a2, render_tmp_p
    add a2, a2, a1
    sb t5, 0(a2)

    # -----------------------------------------
    # Move and twist orientation.
    # -----------------------------------------
    la t4, state_o
    add t4, t4, t3
    lbu t5, 0(t4)          # old orientation

    # twist is indexed by destination position
    add t6, t2, a1
    lbu a2, 0(t6)

    add t5, t5, a2

    # modulo 3, without div/rem
    li a2, 3
    bltu t5, a2, render_quarter_ori_ready

    addi t5, t5, -3


render_quarter_ori_ready:
    la a2, render_tmp_o
    add a2, a2, a1
    sb t5, 0(a2)

    addi t3, t3, 1

    li a1, 7
    bltu t3, a1, render_quarter_build_loop


    # -----------------------------------------
    # Copy temporary state back to state_p/o.
    # -----------------------------------------
    li t3, 0


render_quarter_copy_loop:
    # permutation
    la t4, render_tmp_p
    add t4, t4, t3
    lbu t5, 0(t4)

    la t6, state_p
    add t6, t6, t3
    sb t5, 0(t6)

    # orientation
    la t4, render_tmp_o
    add t4, t4, t3
    lbu t5, 0(t4)

    la t6, state_o
    add t6, t6, t3
    sb t5, 0(t6)

    addi t3, t3, 1

    li t0, 7
    bltu t3, t0, render_quarter_copy_loop

    ret


# ============================================================
# render_delay
#
# Busy wait used only in RENDER=1 GUI builds.
#
# RENDER=0 does not contain this code, so performance
# measurements are unaffected.
# ============================================================
render_delay:
    li t0, 2000000

render_delay_loop:
    addi t0, t0, -1
    bne t0, x0, render_delay_loop

    ret
