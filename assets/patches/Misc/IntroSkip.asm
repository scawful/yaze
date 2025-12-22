;#ENABLED=true

;#PATCH_NAME=Intro skip
;#PATCH_AUTHOR=kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Skip the intro sooner
;#ENDPATCH_DESCRIPTION

pushpc
org $0CC123
	db 4
pullpc
