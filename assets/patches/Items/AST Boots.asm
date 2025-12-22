;#ENABLED=True
;#PATCH_NAME=AST Boots
;#PATCH_AUTHOR=Conn, Zarby89
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Copies the boots mechanics from Ancient Stone Tablets.
;DPad changes boots directions, and transitions can be
;optionally prevented from halting the dash
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=Keep running after transition
;#type=bool
!KeepRunningTransition = $00
;#DEFINE_END


pushpc
org $87911D
JML AstBoots

if !KeepRunningTransition != 00
	org $828B13
	db $80
endif
pullpc

AstBoots:
	BIT.b $F2
	BPL .continue

	LDA.b $F0
	AND.b #$0F
	BNE .pressing_direction

	JML $879138

.pressing_direction
	CMP.b #$0A ; up left
	BEQ +

	CMP.b #$05 ; down right
	BEQ +

	CMP.b #$09 ; down left
	BEQ +

	CMP.b #$06 ; up right
	BNE ++

+	AND.b #$0C

++	CMP.b $26
	BNE +

	JML $879138

+	STA.b $26
	STA.b $67
	STA.w $0340

	JSL $87E6A6

	JML $879138

.continue
	LDA.b #$12
	STA.b $5D

	LDA.b $3A
	AND.b #$7F
	STA.b $3A

	STZ.b $3C
	STZ.b $3D

	LDA.b #$11
	STA.w $0374

	JML $87915E
