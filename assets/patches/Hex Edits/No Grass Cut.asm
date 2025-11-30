;#ENABLED=True
;#PATCH_NAME=No grass cutting
;#PATCH_AUTHOR=kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Grass no longer gets cut by the sword
;#ENDPATCH_DESCRIPTION
pushpc
org $1BBE26
	BRA + : NOP #3 : +
pullpc
