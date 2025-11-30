;#ENABLED=True
;#PATCH_NAME=Misc Small Patches
;#PATCH_AUTHOR=Zarby89
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Lots of small patches to do various things
;No Zelda Telepathy is removing the timed message that tell you to rescue her every minute
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=Titlescreen forever (no intro)
;#type=bool
!TitleScreenForever = $00

;#name=Skip Ending (before credits)
;#type=bool
!SkipEnding = $00

;#name=Prevent S+Q to Dark World
;#type=bool
!NoDwSpan = $00

;#name=Disable Dungeon Map
;#type=bool
!NoDungeonMap = $00

;#name=Disable Oveworld Map
;#type=bool
!NoOWnMap = $00

;#name=No Zelda Telepathy
;#type=bool
!NoZeldaFollower = $00

;#DEFINE_END

if !TitleScreenForever = 1
org $0CC2E3
db $80
endif

if !SkipEnding = 1
org $0E9889
LDA #$20 : STA $11
RTS
endif

if !NoDwSpan = 1
org $028192
LDA #$00 : STA $7EF3CA ; Clear the DW address so game doesn't think we are in DW
JML $0281BD ; To the lightworld !
endif

if !NoDungeonMap = 1
org $0288FD ; Replace a BEQ by a BRA (dungeon map removed)
db $80
endif

if !NoOWnMap = 1
org $02A55E ; Replace a BEQ by a BRA (overworld map removed)
db $80
endif

if !NoZeldaFollower = 1
org $05DEF8
LDA.b #$00
endif


