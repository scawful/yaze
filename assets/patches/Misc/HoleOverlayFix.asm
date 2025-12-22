;#ENABLED=True

;#PATCH_NAME=Hole Overlay Fix
;#PATCH_AUTHOR=kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Allow the floor collision of the hole overlay to work on every floor types
;#ENDPATCH_DESCRIPTION

pushpc

org $01B83E : JSL FigureOutFloor1

; change comparisons to our dynamic values
org $01FE6C : CMP.w $0318
org $01FE71 : CMP.w $031A

pullpc

;===================================================================================================

; Find floor 1 index and save its tiles
FigureOutFloor1:
    REP #$30

    LDX.w $046A ; read floor 1 index

    ; this reuses some memory related to conveyors
    ; the memory is very temporary so it should be safe

    ; databank is 0, so we can use abs,X
    LDA.w $009B52+0,X ; find top tile
    AND.w #$03FE ; isolate tile name
    STA.w $0318 ; save tile

    LDA.w $009B52+8,X ; find bottom tile
    AND.w #$03FE ; isolate tile name
    STA.w $031A ; save tile

    LDA.b $BA ; vanilla code and return
    RTL