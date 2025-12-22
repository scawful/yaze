;#ENABLED=True
;#PATCH_NAME=Torch Tags
;#PATCH_AUTHOR=Jared_Brian_
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Changes the number of torches required to open doors and chests when using the "Light_Torches_to_Open" and "Light_Torches_to_get_Chest" tags.
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=torches required to make a chest appear.
;#type=word
;#range=$01,$08
!ChestTorches =$01

;#name=torches required to open a door.
;#type=word
;#range=$01,$08
!DoorTorches =$08

;#DEFINE_END

pushpc
; Changes the amount of torches required to make a chest appear.
org $01C8CA
    dw !ChestTorches

; Changes the amount of torches required to open a door.
org $01C645
    dw !DoorTorches
pullpc