;#ENABLED=True

;#PATCH_NAME=More spike directions
;#PATCH_AUTHOR=Zarby89,kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Allows more spike blocks Subtype (sprite property)
;Default Values : (00) = normal, (08) = normal vertical
;Values, ascending by speed
;00,01,02,03,04,05,06 = Horizontal
;08,09,0A,0B,0C,0D,0E = Vertical
;#ENDPATCH_DESCRIPTION
lorom

pushpc
org $0691D7 ; SpritePrep_SpikeBlock:
	JSL NewSpikePrep
	RTS

org $1EBD0E
	JSL NewSpikeCollision
	RTS
pullpc

speedValuesH:
db $20, $10, $18, $28, $30, $38, $40, $FF
db $00, $00, $00, $00, $00, $00, $00, $FF

speedValuesV:
db $00, $00, $00, $00, $00, $00, $00, $FF
db $20, $18, $20, $28, $30, $38, $40, $FF

NewSpikePrep:
	TXY

	LDX.w $0E30,Y

	LDA.l speedValuesH,X : STA.w $0D50,Y
	LDA.l speedValuesV,X : STA.w $0D40,Y

	TYX
	RTL

NewSpikeCollision:
	LDA.b #$04 : STA.w $0DF0, X

	LDA.w $0D50, X : EOR.b #$FF : INC A : STA.w $0D50, X
	LDA.w $0D40, X : EOR.b #$FF : INC A : STA.w $0D40, X

	LDA.b #$05 : JSL $0DBB7C ; Sound_SetSfx2PanLong

	RTL
