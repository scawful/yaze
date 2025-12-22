;#ENABLED=false

;#PATCH_NAME=Overworld Tail Map Expansion
;#PATCH_AUTHOR=yaze
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Expands the overworld pointer tables from 160 to 192 entries,
;enabling support for Special World tail maps (0xA0-0xBF).
;
;IMPORTANT: This patch MUST be applied AFTER ZSCustomOverworld v3.
;If applied manually via Asar:
;  1. First apply ZSCustomOverworld v3 patch to your ROM
;  2. Then apply this patch: asar TailMapExpansion.asm your_rom.sfc
;
;If applied via yaze CLI:
;  z3ed overworld-doctor --rom=your_rom.sfc --apply-tail-expansion
;
;Technical details:
;  - Relocates Map32 pointer tables from $02:F94D to $28:A400
;  - Adds 32 blank entries for maps 0xA0-0xBF pointing to $30:8000
;  - Patches 8 code locations in bank $02 to use new table addresses
;  - Writes detection marker 0xEA at $28:A3FF
;#ENDPATCH_DESCRIPTION

lorom

;==============================================================================
; Constants
;==============================================================================
!OldHighTable = $02F94D         ; Vanilla high pointer table (160 entries)
!OldLowTable = $02FB2D          ; Vanilla low pointer table (160 entries)
!NewHighTable = $28A400         ; New high pointer table (192 entries)
!NewLowTable = $28A640          ; New low pointer table (192 entries)
!BlankMapHigh = $308000         ; Blank map data for tail maps (high bytes)
!BlankMapLow = $309000          ; Blank map data for tail maps (low bytes)
!MarkerAddr = $28A3FF           ; Detection marker location
!MarkerValue = $EA              ; Detection marker value (NOP opcode = "expanded")

;==============================================================================
; PC address macros (for read3 function)
;==============================================================================
; LoROM: PC = (bank * $8000) + (addr - $8000)
; $02F94D -> PC = (2 * $8000) + ($F94D - $8000) = $10000 + $794D = $1794D
!OldHighTablePC = $01794D
!OldLowTablePC = $017B2D

;==============================================================================
; Detection marker - allows yaze to detect expanded tables
;==============================================================================
org !MarkerAddr
    db !MarkerValue

;==============================================================================
; Blank map data for tail maps (0xA0-0xBF)
; Located in safe free space at bank $30
;==============================================================================
org !BlankMapHigh               ; PC $180000
BlankMap32High:
    fillbyte $00
    fill 188                    ; 0xBC bytes - compressed blank map32 high data

org !BlankMapLow                ; PC $181000
BlankMap32Low:
    fillbyte $00
    fill 4                      ; 0x04 bytes - compressed blank map32 low data

;==============================================================================
; New expanded High pointer table (192 entries x 3 bytes = 576 bytes)
; Copies existing 160 entries, adds 32 blank entries for tail maps
;==============================================================================
org !NewHighTable               ; PC $142400
ExpandedMap32HPointers:
    ; Copy 160 vanilla entries (preserves any ZSCustomOverworld modifications)
    ; Using Asar's read1() to copy bytes from current ROM state
    !n = 0
    while !n < 160
        db read1(!OldHighTablePC+(!n*3)+0)
        db read1(!OldHighTablePC+(!n*3)+1)
        db read1(!OldHighTablePC+(!n*3)+2)
        !n #= !n+1
    endwhile

    ; Add 32 blank entries for tail maps (0xA0-0xBF)
    ; Each entry is a 24-bit pointer to BlankMap32High ($30:8000)
    !n = 0
    while !n < 32
        dl BlankMap32High
        !n #= !n+1
    endwhile

;==============================================================================
; New expanded Low pointer table (192 entries x 3 bytes = 576 bytes)
;==============================================================================
org !NewLowTable                ; PC $142640
ExpandedMap32LPointers:
    ; Copy 160 vanilla entries (preserves any ZSCustomOverworld modifications)
    !n = 0
    while !n < 160
        db read1(!OldLowTablePC+(!n*3)+0)
        db read1(!OldLowTablePC+(!n*3)+1)
        db read1(!OldLowTablePC+(!n*3)+2)
        !n #= !n+1
    endwhile

    ; Add 32 blank entries for tail maps (0xA0-0xBF)
    !n = 0
    while !n < 32
        dl BlankMap32Low
        !n #= !n+1
    endwhile

;==============================================================================
; Patch game code to use new pointer tables
; Each LDA.l instruction is 4 bytes: AF xx xx xx (opcode + 24-bit address)
; We patch the 3 address bytes to point to the new tables
;==============================================================================

; Function 1: Overworld_DecompressAndDrawOneQuadrant
; Original at PC $1F59D: LDA.l $02F94D,X -> LDA.l $28A400,X
org $02F59E                     ; Address bytes of instruction at $02F59D
    dl ExpandedMap32HPointers   ; New high table address

org $02F5A4                     ; Address bytes for +1 offset access
    dl ExpandedMap32HPointers+1

; Original at PC $1F5C8: LDA.l $02FB2D,X -> LDA.l $28A640,X
org $02F5C9                     ; Address bytes of instruction at $02F5C8
    dl ExpandedMap32LPointers   ; New low table address

org $02F5CF                     ; Address bytes for +1 offset access
    dl ExpandedMap32LPointers+1

; Function 2: Secondary quadrant loader (parallel decompression path)
org $02F7E4
    dl ExpandedMap32HPointers

org $02F7EA
    dl ExpandedMap32HPointers+1

org $02F80F
    dl ExpandedMap32LPointers

org $02F815
    dl ExpandedMap32LPointers+1

;==============================================================================
; End of patch
;==============================================================================
print "Tail Map Expansion patch applied successfully."
print "- New High Table: $28:A400 (192 entries)"
print "- New Low Table: $28:A640 (192 entries)"
print "- Blank Map Data: $30:8000, $30:9000"
print "- Detection Marker: $28:A3FF = $EA"
