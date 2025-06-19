    .module vectors

    .globl _isr_vblank
    .globl _isr_lcd
    .globl _isr_timer
    .globl _isr_serial
    .globl _isr_joypad
    
    .area    VECTORS (ABS)

    .org 0x0040
    jp _isr_vblank
    
    .org 0x0048
    jp _isr_lcd
    
    .org 0x0050
    jp _isr_timer
    
    .org 0x0058
    jp _isr_serial
    
    .org 0x0060
    jp _isr_joypad

