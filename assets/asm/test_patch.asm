; Test assembly file for Asar integration
; This file demonstrates basic 65816 assembly features

arch 65816
lorom

; Define some constants
!test_constant = $1234
!another_constant = $5678
!string_constant = "Hello, World!"

; Define a function pointer
function test_function() = $8000

; Main entry point
main:
    ; Set up 16-bit mode
    rep #$30
    
    ; Load test constant
    lda #!test_constant
    sta $7E0000
    
    ; Call subroutine
    jsr subroutine
    
    ; Load another constant
    lda #!another_constant
    sta $7E0002
    
    ; Return
    rts

; Subroutine
subroutine:
    ; Store some data
    lda #$ABCD
    sta $7E0004
    
    ; Increment a counter
    ldx #$0000
    inx
    stx $7E0006
    
    rts

; Data section
data:
    db $01, $02, $03, $04, $05
    dw $1234, $5678, $9ABC, $DEF0
    dl $123456, $789ABC, $DEF012
    db "Test String", $00

; Another label for testing
another_label:
    nop
    nop
    rts

; Define some more constants
!flag_set = $01
!flag_clear = $00
!max_items = 100