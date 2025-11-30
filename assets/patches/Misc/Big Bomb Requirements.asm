;#ENABLED=True

;#PATCH_NAME=Big Bomb requirement
;#PATCH_AUTHOR=Zarby89,kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Modify the crystal and dwarf requirements for the big bomb
;If SmithRequirement is set to 20, you will need to save the Smith first
;#ENDPATCH_DESCRIPTION


;#DEFINE_START
;#name=Crystals Required
;#type=bitfield
;#bit0=Crystal 6
;#bit1=Crystal 1
;#bit2=Crystal 5
;#bit3=Crystal 7
;#bit4=Crystal 2
;#bit5=Crystal 4
;#bit6=Crystal 3
!CrystalRequirement =$02


;#name=Required smith saved?
;#type=bool
;#uncheckedvalue=$00
;#checkedvalue=$20
!SmithRequirement =$00


;#DEFINE_END

pushpc

org $1EE16A

	LDA.l $7EF37A : AND.b #!CrystalRequirement : CMP.b #!CrystalRequirement

	skip 2

	LDA.l $7EF3C9 : AND.b #!SmithRequirement

pullpc