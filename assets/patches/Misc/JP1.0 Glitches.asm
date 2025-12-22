;#ENABLED=True

;#PATCH_NAME=1.0 Glitches
;#PATCH_AUTHOR=kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Restore the JP 1.0 glitches
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=Mirror block erase
;#type=bool
!MirrorEraseBlock = $00

;#name=Spin speed and Item dash
;#type=bool
!SpinSpeedItemDash = $00

;#name=Fake flippers
;#type=bool
!FakeFlippers = $00
;#DEFINE_END

pushpc
if !MirrorEraseBlock == 1
org $07A969
	JMP.w $07A970
endif

if !SpinSpeedItemDash == 1
org $0781C0
	BRA + : NOP #4 : +
endif

if !FakeFlippers == 1
org $079665
	JMP.w $07966C
endif
pullpc