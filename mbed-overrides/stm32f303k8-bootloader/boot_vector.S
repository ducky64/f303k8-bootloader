/* Provides the initial low-memory boot vector into the high-memory bootloader.
*/

  .syntax unified
    .cpu cortex-m4
    .fpu softvfp
    .thumb

.global Reset_Handler

    .section    .boot_vector,"a",%progbits
    .type   g_bootVectors, %object
    .size   g_bootVectors, .-g_bootVectors


g_bootVectors:
    .word   _estack
    .word   Reset_Handler
