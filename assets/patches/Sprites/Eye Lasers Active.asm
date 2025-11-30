;#PATCH_NAME=Eye Lasers Active
;#PATCH_AUTHOR=Jared_Brian_
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Changes the wall eye lasers to always be active or always inactive reguardless of what X position they are placed on.
;Normally in vanilla every other X position is set to be inactive unless link is looking directly at them.
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=Active?
;#type=bool
!EyeActive = $00
;#DEFINE_END

pushpc

if !EyeActive == $00
    ; Make it so the eye lazers are always inactive reguardless of what X position they are placed on.
    org $1EA50B
        ; Replaced code: 
        ; LDA $0D10, X : AND.b #$10 : EOR.b #$10 : STA !requires_facing, X
        NOP #5 : LDA.b #$00 : STA $0EB0, X

    org $1EA52C
        ; Replace code:
        ; LDA $0D00, X : AND.b #$10 : STA !requires_facing, X
        NOP #3 : LDA.b #$00 : STA $0EB0, X

elseif !EyeActive == $01
    ; Make it so the eye lazers are always active reguardless of what X position they are placed on.
    org $1EA50B
        ; Replaced code: 
        ; LDA $0D10, X : AND.b #$10 : EOR.b #$10 : STA !requires_facing, X
        NOP #5 : LDA.b #$01 : STA $0EB0, X

    org $1EA52C
        ; Replace code:
        ; LDA $0D00, X : AND.b #$10 : STA !requires_facing, X
        NOP #3 : LDA.b #$01 : STA $0EB0, X

endif

pullpc