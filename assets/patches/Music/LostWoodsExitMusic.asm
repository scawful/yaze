;#ENABLED=True

;#PATCH_NAME=Lost Woods Exit Music
;#PATCH_AUTHOR=Jared_Brian_
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Changes the room that plays the lost woods theme when exiting. Note: This only works for the first byte, so if you want to play music when leaving room 13, this will also cause room 113 to play the music. In vanilla this is used for room E1 Cave (Lost Woods HP).
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=The room number.
;#type=byte
!MusicRoomNumber = $E1

;#DEFINE_END

pushpc

org $02844F
    db !MusicRoomNumber

pullpc