;#ENABLED=True
;#PATCH_NAME=No sword beams
;#PATCH_AUTHOR=kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Disables sword beams
;#ENDPATCH_DESCRIPTION

pushpc
org $079C70
	JMP.w $079CA0
pullpc
