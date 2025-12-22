;#ENABLED=True
;#PATCH_NAME=Link Bed Starting Position
;#PATCH_AUTHOR=Zarby89, Jared_Brian_, kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Changes where Link spawns during the opening bed cutscene
;Positions can be found by temporarily moving the Link's house entrance to
;the desired location
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=Link Y Position
;#type=word
!LinkYPosition = $2182

;#name=Link Y Position
;#type=word
!LinkXPosition = $095B
;#DEFINE_END

; Link sleep position changes
org $079A31
	LDA.w #!LinkYPosition : STA.b $20 ; Y link position in bed
	LDA.w #!LinkXPosition : STA.b $22 ; X link position in bed

org $05DE52 ; These values should be the same as the ones above
	LDA.b #!LinkXPosition : STA.w $0FC2 ; X link position in bed
	LDA.b #!LinkXPosition>>8 : STA.w $0FC3 ; X link position in bed

	LDA.b #!LinkYPosition : STA.w $0FC4 ; Y link position in bed
	LDA.b #!LinkYPosition>>8 : STA.w $0FC5 ; Y link position in bed

org $0980B7
	LDA.w #(!LinkYPosition+8) : STA.b $00 ; Y link sheet in bed
	LDA.w #(!LinkXPosition-8) : STA.b $02 ; X link sheet in bed

org $05DE8C
	LDA.b #(!LinkYPosition-3) : STA.b $20 ; Y link position in bed when awoken
	LDA.b #(!LinkYPosition-3)>>8 : STA.b $21 ; Y link position in bed when awoken