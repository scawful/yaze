;#ENABLED=True
;#PATCH_NAME=Kholdstare Speeds
;#PATCH_AUTHOR=Jared_Brian_
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Changes the speeds at which kholdstare can move.
;By default he will move the same speed on the x and y axis in all 4 diagonal directions.
;Values above 0x80 are negative.
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=X Value 1
;#type=byte
!XValue1 = $0010

;#name=X Value 2
;#type=byte
!XValue2 = $0010

;#name=X Value 3
;#type=byte
!XValue3 = $00F0

;#name=X Value 4
;#type=byte
!XValue4 = $00F0

;#name=Y Value 1
;#type=byte
!YValue1 = $00F0

;#name=Y Value 2
;#type=byte
!YValue2 = $0010

;#name=Y Value 3
;#type=byte
!YValue3 = $0010

;#name=Y Value 4
;#type=byte
!YValue4 = $00F0

;#DEFINE_END

pushpc

; Speed chagnes.
org $1E95DD
    .x_speed_limits
    db !XValue1, !XValue2, !XValue3, !XValue4
    
    .y_speed_limits
    db !YValue1, !YValue2, !YValue3, !YValue4

pullpc