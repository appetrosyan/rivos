

.global user_surface_commit
user_surface_commit:
    mv a1, a0
    addi a0, x0, 0
    ecall
    ret

.global user_surface_acquire
user_surface_acquire:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 1
    ecall
    ret

.global user_thread_sleep
user_thread_sleep:
    mv a1, a0
    addi a0, x0, 2
    ecall
    ret

.global user_wait_for_surface_draw
user_wait_for_surface_draw:
    mv a1, a0
    addi a0, x0, 3
    ecall
    ret

.global user_get_raw_mouse
user_get_raw_mouse:
    mv a2, a1
    mv a1, a0
    addi a0, x0, 4
    ecall
    ret
