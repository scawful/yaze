;#ENABLED=True
;#PATCH_NAME=Elemental Trinexx
;#PATCH_AUTHOR=Jared_Brian_
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Changes Trinexx's side heads to be both ice heads, both fire heads, or swapped.
;The heads will shoot the corrisponding beams and will also appear the correct color.
;The main head will appear the elemental color if you set that option but otherwise will be the default palette.
;You will still need to set the side heads to take damage from the appropriate elemental rod with the advanced damage editor.
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=Changes the arrangement of side heads.
;#type=choice
;#choice0=Inverted Heads
;#choice1=Ice Heads Only
;#choice2=Fire Heads Only
!ElementType = $00

;#name=Changes the main head palette.
;#type=choice
;#choice0=Default Palette
;#choice1=Ice Palette
;#choice2=Fire Palette
!MainHeadPalette = $00
;#DEFINE_END

pushpc

if !ElementType == 0
    ; Change the palettes of the side heads to the inverted palette.
    org $0DB425
        db $0D, $0B

    ; Swap which head shows the apropriate particales when charging up.
    org $1DBAD9
        db $F0 ; Replace a BNE with a BEQ.

    ; Swap which head shoots what beam.
    org $1DBAE8
        LDA $0E20, X : CMP.b #$CC

    ; Another beam related thing.
    org $1DBAF8
        LDA.b #$CC

elseif !ElementType == 1
    ; Change the palettes of the side heads to the ice palette.
    org $0DB425
        db $0D, $0D 

    ; Make the fire head show ice spakles when charging up.
    org $1DBAD9
        db $80 ; Replace a BNE with a BRA.

    ; Make the fire head shoot ice instead of fire.
    org $1DBAE8
        LDA.b #$CD : NOP #5

elseif !ElementType == 2
    ; Change the palettes of the side heads to the fire palette.
    org $0DB425
        db $0B, $0B 

    ; Make the ice head show flames when charging up.
    org $1DBAD9
        NOP : NOP ; Remove the BNE and never branch.

    ; Make the ice head shoot fire instead of ice.
    org $1DBAE8
        LDA.b #$CC : NOP : NOP : NOP
        db $80 ; Replace a BNE with a BRA.

endif

if !MainHeadPalette == 1
    ; Change the palette of all the main head to the ice.
    org $0DB424
        db $4D

    ; Change the snake trinexx palette to ice.
    org $1DB033
        db $0D

elseif !MainHeadPalette == 2
    ; Change the palette of all the main head to the fire.
    org $0DB424
        db $4B

    ; Change the snake trinexx palette to fire.
    org $1DB033
        db $0B
endif

pullpc