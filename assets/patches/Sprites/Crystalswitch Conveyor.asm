;#ENABLED=True
;#PATCH_NAME=Crystal Switch Conveyor
;#PATCH_AUTHOR=Zarby89
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Causes crystal switches to be moved by conveyors
;#ENDPATCH_DESCRIPTION
lorom

pushpc
org $06B8D0
JSL ConveyorSwitch
NOP
pullpc

ConveyorSwitch:
JSL $06E496 ; Sprite_CheckTileCollisionLong
LDA.w $0F50, X : AND.b #$F1
RTL
