; Yaze Test Patch for Zelda3 ROM
; This patch demonstrates Asar integration with a real ROM

; Test constants
!test_ram_addr = $7E2000

; Simple code modification
org $008000
yaze_test_hook:
    ; Save original context
    pha
    phx
    phy
    
    ; Set up processor state
    rep #$30            ; 16-bit A and X/Y
    
    ; Write test signature to RAM
    lda #$CAFE
    sta !test_ram_addr
    lda #$BEEF  
    sta !test_ram_addr+2
    lda #$DEAD
    sta !test_ram_addr+4
    lda #$BABE
    sta !test_ram_addr+6
    
    ; Call test subroutine
    jsr yaze_test_function
    
    ; Restore context
    ply
    plx
    pla
    
    ; Continue with original code
    ; (In a real patch, this would jump to the original code)
    rts

; Test function to verify Asar compilation
yaze_test_function:
    pha
    
    ; Test arithmetic operations
    lda #$1000
    clc
    adc #$0234
    sta !test_ram_addr+8
    
    ; Test conditional logic
    cmp #$1234
    bne +
    lda #$600D
    sta !test_ram_addr+10
+
    
    pla
    rts

; Test data section
yaze_test_data:
    db "YAZE", $00
    dw $1234, $5678, $9ABC, $DEF0
    
yaze_test_string:
    db "ASAR INTEGRATION TEST", $00

; Test lookup table
yaze_test_table:
    dw yaze_test_hook
    dw yaze_test_function
    dw yaze_test_data
    dw yaze_test_string

; More advanced test - interrupt vector modification
; org $00FFE4
; dw yaze_test_hook   ; COP vector (for testing)

print "Yaze Asar integration test patch compiled successfully!"
print "Test hook at: ", hex(yaze_test_hook)
print "Test function at: ", hex(yaze_test_function)
print "Test data at: ", hex(yaze_test_data)
