;#ENABLED=True
;#PATCH_NAME=No Hardcoded Rocks
;#PATCH_AUTHOR=Jared_Brian_
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Removes the 2 hardcoded rocks that get placed in area 33 and 2F.
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=Remove rock in area 33.
;#type=bool
!RemoveRock1 = $01
;#name=Remove rock in area 2F.
;#type=bool
!RemoveRock2 = $01
;#DEFINE_END

pushpc
if !RemoveRock1 == 1
    org $02EF33
        NOP #4
endif

if !RemoveRock2 == 1
    org $02EF3C
        NOP #4
endif
pullpc