;#ENABLED=True

;=========================
;TODO FINISH ADDING DEFINES
;INCOMPLETE PATCH
;==========================
;#PATCH_NAME=Weathervane Position
;#PATCH_AUTHOR=Jared_Brian_
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Modify the position of where you need to use the flute to destroy weathervane and spawn bird
;#ENDPATCH_DESCRIPTION

!AREAID = $34

; Weather vane explosion changes:
; Let the flute work in special areas.
org $07A40A
    NOP : NOP
    
org $07A418
    JML NewVaneCheck

org $07A441
    VanePassed:

org $07A44F
    VaneFailed:

org $07A453
    SpawnBird:

; Disable the area check for the weather vane sprite.
org $06C2EB
LDA $8A : CMP.b #!AREAID : BNE .outside_village
    ; Check if we have the flute activated already:
    LDA $7EF34C : CMP.b #$03 : BNE .player_lacks_bird_enabled_flute
        STZ $0DD0, X
        
    .player_lacks_bird_enabled_flute
        
    RTS
    
.outside_village
    
; What to do in an area outside of the village:
LDA $7EF34C : AND.b #$02 : BEQ .player_lacks_flute_completely
    STZ $0DD0, X ; Suicide.
.player_lacks_flute_completely
RTS

warnpc $06C309

; Tile 1
org $1BC226
    dw $06CA

; Tile 2
org $1BC232
    dw $06CE

; Tile 3
org $1BC243
    dw #$074C

; Bird coords
org $098DC1
    LDA.w #$08C8 : STA $00
    LDA.w #$0460 : STA $02

; Vane Debris coords
org $098CED
    .y_coords
    db $F4, $E7, $E4, $E6, $E4, $EC, $E4, $E4, $EC, $E5, $F4, $E4
    
    .x_coords
    db $7C, $5E, $6C, $60, $62, $64, $7C, $60, $64, $62, $60, $6C

; Debris Y high byte
org $098D65
    db $08

; Debris X high byte
org $098D72
    db $04

pullpc ; Continue extended space.

NewVaneCheck:
{
    REP #$20

    ; Check if its the master sword area.
    LDA $8A : CMP.w #!AREAID : BNE .not_weathervane_trigger2
        LDA $20   
        CMP.w #$0068 : BCC .not_weathervane_trigger1
        CMP.w #$00A0 : BCS .not_weathervane_trigger1
                
        LDA $22
        CMP.w #$0040 : BCC .not_weathervane_trigger1
        CMP.w #$00A0 : BCS .not_weathervane_trigger1

        SEP #$20
        
        ; Cancel other sounds
        STZ $012E
        STZ $012F

        ; Stop player input
        INC InCutScene

        ; Trigger Zelda
        INC $0642

        JML VaneFailed

    .not_weathervane_trigger2

    SEP #$20

    ; Check if we already have the bird.
    LDA $7EF34C : CMP.b #$02 : BNE .travel_bird_not_already_released
        REP #$20

        ; Check the area for #$22.
        LDA $8A : CMP.w #$0022 : BNE .not_weathervane_trigger1
            LDA $20   
            CMP.w #$0900 : BCC .not_weathervane_trigger1
            CMP.w #$0920 : BCS .not_weathervane_trigger1
                
            LDA $22
            CMP.w #$0450 : BCC .not_weathervane_trigger1
            CMP.w #$0470 : BCS .not_weathervane_trigger1
                SEP #$20
                LDA $7EF2A2 : ORA.b #$20 : STA $7EF2A2
                REP #$20

                STZ FluteIndex

                JML VanePassed

        .not_weathervane_trigger1

        JML VaneFailed

    .travel_bird_not_already_released

    JML SpawnBird
}

pushpc ; Pause expanded space.

; ==============================================================================