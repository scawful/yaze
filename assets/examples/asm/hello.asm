; examples/asm/hello.asm
;
; Minimal 65816 patch used as a smoke fixture for the z3dk integration.
; Asserts the toolchain can:
;   - parse LoROM + org directives
;   - resolve two labels
;   - assemble branch + load/store instructions
;
; If you modify this file, update test/unit/core/z3dk_wrapper_test.cc too.

lorom
org $008000

Start:
    SEI
    CLC
    XCE
    LDA #$01
    STA $2100
    BRA Forever

Forever:
    NOP
    BRA Forever
