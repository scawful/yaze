;#ENABLED=False
;#PATCH_NAME=File fairy skin color fix
;#PATCH_AUTHOR=kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Fixes the file select fairy's skin color
;#ENDPATCH_DESCRIPTION

pushpc
org $1BF02A
	db $10
pullpc
