; ==============================================================================
; ZScream Custom Overworld ASM
; ==============================================================================
; ==============================================================================
; Non-Expanded Space
; ==============================================================================

pushpc

AnimatedTileGFXSet = $0FC0
TransGFXModuleIndex = $0CF3

Sound_LoadLightWorldSongBank              = $808913
DecompOwAnimatedTiles                     = $80D394
GetAnimatedSpriteTile                     = $80D4DB
GetAnimatedSpriteTile_variable            = $80D4ED
LoadTransAuxGFX_sprite_continue           = $00D706
Do3To4High16Bit                           = $80DF4F
Do3To4Low16Bit                            = $00DFB8
InitTilesets                              = $80E19B
CopyFontToVram                            = $80E556
Decomp_bg_variable                        = $00E78F

DeleteCertainAncillaeStopDashing          = $828B0C
Overworld_FinishTransGfx_firstHalf_Retrun = $02ABC5
Overworld_LoadSubscreenAndSilenceSFX1     = $82AF19
Dungeon_LoadPalettes_cacheSettings        = $82C65F
LoadSubscreenOverlay                      = $82FD0D

Link_ItemReset_FromOverworldThings        = $87B107

Tagalong_Init                             = $899EFC
Sprite_ReinitWarpVortex                   = $89AF89
Sprite_ResetAll                           = $89C44E
Sprite_OverworldReloadAll                 = $89C499

Overworld_SetFixedColorAndScroll          = $8BFE70

Overworld_LoadPalettes                    = $8ED5A8
Palette_SetOwBgColor_Long                 = $8ED618
LoadGearPalettes_bunny                    = $8ED6DD

Palette_SpriteAux3                        = $9BEC77
Palette_MainSpr                           = $9BEC9E
Palette_SpriteAux1                        = $9BECC5
Palette_SpriteAux2                        = $9BECE4
Palette_Sword                             = $9BED03
Palette_Shield                            = $9BED29
Palette_MiscSpr                           = $9BED6E
Palette_ArmorAndGloves                    = $9BEDF9
Palette_Hud                               = $9BEE52
Palette_OverworldBgMain                   = $9BEEC7

; ==============================================================================
; Fixing old hooks:
; ==============================================================================

; Loads the transparent color under some load conditions.
org $0BFEB6
    STA.l $7EC500

; Main Palette loading routine.
org $0ED5E7
    JSL $9BEEA8 ; Palette_OverworldBgAux3

; After leaving special areas like Zora's and the Master Sword area.
org $02E94A
    JSL $8ED5A8 ; Overworld_LoadPalettes

; ==============================================================================
; Expanded Space
; ==============================================================================

; Reserved ZS space.
; Avoid moving this at all costs. If you do, you will have to change where ZS
; saves this data as well and previous data will be lost or corrupted.
org $288000 ; $140000
Pool:
{
    .BGColorTable ; $140000
    ; Valid values:
    ; 555 color value $0000 to $7FFF.

    ; LW
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ; DW
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ; SW
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    ;dw $0000, $0000, $0000, $0000, $0000, $0000, $0000, $0000
    warnpc $288140

    org $288140 ; $140140
    .EnableTable ; 0x20
    ; Valid values:
    ; $00 - Disabled
    ; $FF - Enabled

    org $288140 ; $140140
    .EnableBGColor ; 0x01
    db $01

    org $288141 ; $140141
    .EnableMainPalette ; 0x01
    db $01
    
    org $288142 ; $140142
    .EnableMosaic ; 0x01 Unused for now.
    db $01

    ; When non 0 this will allow animated tiles to be updated between OW
    ; transitions. Default is $FF.
    org $288143 ; $140143
    .EnableAnimated ; 0x01
    db $01
    
    ; When non 0 this will allow Subscreen Overlays to be updated between OW
    ; transitions. Default is $FF.
    org $288144 ; $140144
    .EnableSubScreenOverlay ; 0x01
    db $01

    ; This is a reserved value that ZS will write to when it has applied the
    ; ASM. That way the next time ZS loads the ROM it knows to read the custom
    ; values instead of using the default ones. The current version is 02.
    org $288145 ; $140145
    .ZSAppliedASM ; 0x01
    db $02

    ; When non 0 this will cause rain to appear on all areas in the beginning
    ; phase. Default is $FF.
    org $288146 ; $140146
    .EnableBeginningRain ; 0x01
    db $FF

    ; When non 0 this will disable the ambiant sound that plays in the mire
    ; area after the event is triggered. Default is $FF.
    org $288147 ; $140147
    .EnableRainMireEvent ; 0x01
    db $FF

    ; When non 0 this will make the game reload all gfx in between OW
    ; transitions. Default is $FF.
    org $288148 ; $140143
    .EnableTransitionGFXGroupLoad ; 0x01
    db $01

    ; The bridge color is different from the Master Sword area so we are going to
    ; hard code it here for now.
    org $288149 ; $140149
    .BGColorTable_Bridge ; 0x02
    dw $2669 ; Defualt vanilla LW green.

    ; The rest of these are extra bytes that can be used for anything else
    ; later on.
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00
    warnpc $288160

    org $288160 ; $140160
    .MainPaletteTable ; 0xA0
    ; Valid values:
    ; Main overworld palette index $00 to $05.
    ; $00 is the normal light world palette.
    ; $01 is the normal dark world palette.
    ; $02 is the normal light world death mountain palette.
    ; $03 is the normal dark world death mountain palette.
    ; $04 is the Triforce room palette.
    ; $05 is the title screen palette?

    ; LW
    ;db $00, $00, $00, $02, $00, $20, $00, $20
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ; DW
    ;db $01, $01, $01, $03, $01, $03, $01, $03
    ;db $01, $01, $01, $01, $01, $01, $01, $01
    ;db $01, $01, $01, $01, $01, $01, $01, $01
    ;db $01, $01, $01, $01, $01, $01, $01, $01
    ;db $01, $01, $01, $01, $01, $01, $01, $01
    ;db $01, $01, $01, $01, $01, $01, $01, $01
    ;db $01, $01, $01, $01, $01, $01, $01, $01
    ;db $01, $01, $01, $01, $01, $01, $01, $01
    ; SW
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $04, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    warnpc $288200
    
    org $288200 ; $140200
    .MosaicTable ; 0xA0
    ; Valid values:
    ; $01 to enable mosaic, $00 to disable.

    ; LW
    ;db $01, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ; DW
    ;db $01, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ; SW
    ;db $01, $01, $00, $00, $00, $00, $00, $00
    ;db $01, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    ;db $00, $00, $00, $00, $00, $00, $00, $00
    warnpc $2882A0

    ; Not the same as OWGFXGroupTable_sheet7. The game uses a combination of $59
    ; and $5B to create the sheet in sheet #7. This is done by first transfering
    ; all the gfx that is needed for the bottom half of the sheet (the door
    ; frames for example) which is different depending on whether we are in the
    ; LW or DW. It then loads the actual animated tile frames into a buffer
    ; where it can transfer over from durring NMI based on whether we are on
    ; Death Mountain or not (LW or DW). This table is to control the latter.
    org $2882A0 ; $1402A0
    .AnimatedTable ; 0xA0
    ; Valid values:
    ; GFX index $00 to $FF.
    ; In vanilla, $59 are the DW door frames and clouds and $5B are the Lw door
    ; frames and the regular water tiles.

    ; LW
    ;db $5B, $5B, $5B, $59, $5B, $59, $5B, $59
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ; DW
    ;db $5B, $5B, $5B, $59, $5B, $59, $5B, $59
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ; SW
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    ;db $5B, $5B, $5B, $5B, $5B, $5B, $5B, $5B
    warnpc $288340

    org $288340 ; $140340
    .OverlayTable ; 0x140
    ; Valid values:
    ; Can be any value $00 to $FF but is stored as 2 bytes instead of one to
    ; help the code out below. $FF is for no overlay area. Hopefully no crazy
    ; person decides to expand their overworld to $FF areas.

    ; $0093 is the triforce room curtain overlay.
    ; $0094 is the under the bridge overlay.
    ; $0095 is the sky background overlay.
    ; $0096 is the pyramid background overlay.
    ; $0097 is the first fog overlay.

    ; $009C is the lava background overlay.
    ; $009D is the second fog overlay.
    ; $009E is the tree canopy overlay.
    ; $009F is the rain overlay.

    ; LW
    ;dw $009D, $00FF, $00FF, $0095, $00FF, $0095, $00FF, $0095
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ; DW
    ;dw $009D, $00FF, $00FF, $009C, $00FF, $009C, $00FF, $009C
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $0096, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $009F, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ; SP
    ;dw $0097, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $0093, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    ;dw $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF, $00FF
    warnpc $288480

    org $288480 ; $140480
    .OWGFXGroupTable ; 0x500 (0xA0 * 0x08)

    ; 0xFF is used instead of 0x00 as the "don't change the sheet" value. That
    ; way, we can actually use sheet 00 if we want. Just in case 0xFF is used
    ; and there is no sheet to load when warping using the bird, unloading the
    ; map, or exiting a dungeon, the DefaultGFXGroups values are used.

    ; LW
    org $288480 ; $140480
    .OWGFXGroupTable_sheet0
    ;db $3A ; 0x00 sheet 0

    org $288481 ; $140481
    .OWGFXGroupTable_sheet1
    ;db $3B ; 0x00 sheet 1

    org $288482 ; $140482
    .OWGFXGroupTable_sheet2
    ;db $3C ; 0x00 sheet 2

    org $288483 ; $140483
    .OWGFXGroupTable_sheet3
    ;db $3D ; 0x00 sheet 3

    org $288484 ; $140484
    .OWGFXGroupTable_sheet4
    ;db $57 ; 0x00 sheet 4

    org $288485 ; $140485
    .OWGFXGroupTable_sheet5
    ;db $4C ; 0x00 sheet 5

    org $288486 ; $140486
    .OWGFXGroupTable_sheet6
    ;db $3E ; 0x00 sheet 6

    org $288487 ; $140487
    .OWGFXGroupTable_sheet7
    ;db $5B ; 0x00 sheet 7

    ;db $3A, $3B, $3C, $3D, $57, $4C, $3E, $5B ; 0x01
    ;db $3A, $3B, $3C, $3D, $57, $4C, $3E, $5B ; 0x02
    ;db $3A, $3B, $3C, $3D, $56, $4F, $3E, $5B ; 0x03
    ;db $3A, $3B, $3C, $3D, $56, $4F, $3E, $5B ; 0x04
    ;db $3A, $3B, $3C, $3D, $56, $4F, $3E, $5B ; 0x05
    ;db $3A, $3B, $3C, $3D, $56, $4F, $3E, $5B ; 0x06
    ;db $3A, $3B, $3C, $3D, $56, $4F, $3E, $5B ; 0x07

    ;db $3A, $3B, $3C, $3D, $57, $4C, $3E, $5B ; 0x08
    ;db $3A, $3B, $3C, $3D, $57, $4C, $3E, $5B ; 0x09
    ;db $3A, $3B, $3C, $3D, $57, $4C, $3E, $5B ; 0x0A
    ;db $3A, $3B, $3C, $3D, $56, $4F, $3E, $5B ; 0x0B
    ;db $3A, $3B, $3C, $3D, $56, $4F, $3E, $5B ; 0x0C
    ;db $3A, $3B, $3C, $3D, $56, $4F, $3E, $5B ; 0x0D
    ;db $3A, $3B, $3C, $3D, $56, $4F, $3E, $5B ; 0x0E
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x0F

    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x10
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x11
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x12
    ;db $3A, $3B, $3C, $3D, $50, $4B, $3E, $5B ; 0x13
    ;db $3A, $3B, $3C, $3D, $50, $4B, $3E, $5B ; 0x14
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x15
    ;db $3A, $3B, $3C, $3D, $50, $4B, $3E, $5B ; 0x16
    ;db $3A, $3B, $3C, $3D, $50, $4B, $3E, $5B ; 0x17

    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x18
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x19
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x1A
    ;db $3A, $3B, $3C, $3D, $52, $49, $3E, $5B ; 0x1B
    ;db $3A, $3B, $3C, $3D, $52, $49, $3E, $5B ; 0x1C
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x1D
    ;db $3A, $3B, $3C, $3D, $55, $4A, $3E, $5B ; 0x1E
    ;db $3A, $3B, $3C, $3D, $55, $4A, $3E, $5B ; 0x1F

    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x20
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x21
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x22
    ;db $3A, $3B, $3C, $3D, $52, $49, $3E, $5B ; 0x23
    ;db $3A, $3B, $3C, $3D, $52, $49, $3E, $5B ; 0x24
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x25
    ;db $3A, $3B, $3C, $3D, $55, $4A, $3E, $5B ; 0x26
    ;db $3A, $3B, $3C, $3D, $55, $4A, $3E, $5B ; 0x27

    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x28
    ;db $3A, $3B, $3C, $3D, $50, $4B, $3E, $5B ; 0x29
    ;db $3A, $3B, $3C, $3D, $57, $4C, $3E, $5B ; 0x2A
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x2B
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x2C
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x2D
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x2E
    ;db $3A, $3B, $3C, $3D, $55, $4A, $3E, $5B ; 0x2F

    ;db $3A, $3B, $3C, $3D, $55, $54, $3E, $5B ; 0x30
    ;db $3A, $3B, $3C, $3D, $55, $54, $3E, $5B ; 0x31
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x32
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x33
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x34
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x35
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x36
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x37

    ;db $3A, $3B, $3C, $3D, $55, $54, $3E, $5B ; 0x38
    ;db $3A, $3B, $3C, $3D, $55, $54, $3E, $5B ; 0x39
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x3A
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x3B
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x3C
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x3D
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x3E
    ;db $3A, $3B, $3C, $3D, $51, $4E, $3E, $5B ; 0x3F

    ; DW
    ;db $42, $43, $44, $45, $2D, $2E, $3F, $59 ; 0x40
    ;db $42, $43, $44, $45, $2D, $2E, $3F, $59 ; 0x41
    ;db $42, $43, $44, $45, $2D, $2E, $3F, $59 ; 0x42
    ;db $42, $43, $44, $45, $33, $34, $3F, $59 ; 0x43
    ;db $42, $43, $44, $45, $33, $34, $3F, $59 ; 0x44
    ;db $42, $43, $44, $45, $33, $34, $3F, $59 ; 0x45
    ;db $42, $43, $44, $45, $33, $34, $3F, $59 ; 0x46
    ;db $42, $43, $44, $45, $60, $34, $3F, $59 ; 0x47

    ;db $42, $43, $44, $45, $2D, $2E, $3F, $59 ; 0x48
    ;db $42, $43, $44, $45, $2D, $2E, $3F, $59 ; 0x49
    ;db $42, $43, $44, $45, $2D, $2E, $3F, $59 ; 0x4A
    ;db $42, $43, $44, $45, $33, $34, $3F, $59 ; 0x4B
    ;db $42, $43, $44, $45, $33, $34, $3F, $59 ; 0x4C
    ;db $42, $43, $44, $45, $33, $34, $3F, $59 ; 0x4D
    ;db $42, $43, $44, $45, $33, $34, $3F, $59 ; 0x4E
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x4F

    ;db $42, $43, $44, $45, $2F, $30, $3F, $59 ; 0x50
    ;db $42, $43, $44, $45, $2F, $30, $3F, $59 ; 0x51
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x52
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x53
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x54
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x55
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x56
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x57

    ;db $42, $43, $44, $45, $2F, $30, $3F, $59 ; 0x58
    ;db $42, $43, $44, $45, $2F, $30, $3F, $59 ; 0x59
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x5A
    ;db $42, $43, $44, $45, $35, $36, $3F, $59 ; 0x5B
    ;db $42, $43, $44, $45, $35, $36, $3F, $59 ; 0x5C
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x5D
    ;db $42, $43, $44, $45, $2B, $2C, $3F, $59 ; 0x5E
    ;db $42, $43, $44, $45, $2B, $2C, $3F, $59 ; 0x5F

    ;db $42, $43, $44, $45, $2F, $30, $3F, $59 ; 0x60
    ;db $42, $43, $44, $45, $2F, $30, $3F, $59 ; 0x61
    ;db $42, $43, $44, $45, $2F, $30, $3F, $59 ; 0x62
    ;db $42, $43, $44, $45, $35, $36, $3F, $59 ; 0x63
    ;db $42, $43, $44, $45, $35, $36, $3F, $59 ; 0x64
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x65
    ;db $42, $43, $44, $45, $2B, $2C, $3F, $59 ; 0x66
    ;db $42, $43, $44, $45, $2B, $2C, $3F, $59 ; 0x67

    ;db $42, $43, $44, $45, $2F, $30, $3F, $59 ; 0x68
    ;db $42, $43, $44, $45, $2F, $30, $3F, $59 ; 0x69
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x6A
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x6B
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x6C
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x6D
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x6E
    ;db $42, $43, $44, $45, $2B, $2C, $3F, $59 ; 0x6F

    ;db $42, $43, $44, $45, $31, $32, $3F, $59 ; 0x70
    ;db $42, $43, $44, $45, $31, $32, $3F, $59 ; 0x71
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x72
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x73
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x74
    ;db $42, $43, $44, $45, $31, $32, $3F, $59 ; 0x75
    ;db $42, $43, $44, $45, $31, $32, $3F, $59 ; 0x76
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x77

    ;db $42, $43, $44, $45, $31, $32, $3F, $59 ; 0x78
    ;db $42, $43, $44, $45, $31, $32, $3F, $59 ; 0x79
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x7A
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x7B
    ;db $42, $43, $44, $45, $37, $38, $3F, $59 ; 0x7C
    ;db $42, $43, $44, $45, $31, $32, $3F, $59 ; 0x7D
    ;db $42, $43, $44, $45, $31, $32, $3F, $59 ; 0x7E
    ;db $42, $43, $44, $45, $20, $2B, $3F, $59 ; 0x7F

    ; SW
    ;db $3A, $3B, $3C, $3D, $47, $48, $3E, $5B ; 0x80
    ;db $3A, $3B, $3C, $3D, $47, $48, $3E, $5B ; 0x81
    ;db $3A, $3B, $3C, $3D, $47, $48, $3E, $5B ; 0x82
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x83
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x84
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x85
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x86
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x87

    ;db $3A, $3B, $3C, $17, $40, $41, $39, $5B ; 0x88
    ;db $3A, $3B, $3C, $3D, $47, $48, $3E, $5B ; 0x89
    ;db $3A, $3B, $3C, $3D, $47, $48, $3E, $5B ; 0x8A
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x8B
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x8C
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x8D
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x8E
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x8F

    ;db $3A, $3B, $3C, $08, $00, $22, $1B, $5B ; 0x90
    ;db $3A, $3B, $3C, $08, $00, $22, $1B, $5B ; 0x91
    ;db $3A, $3B, $3C, $06, $53, $1F, $18, $5B ; 0x92
    ;db $3A, $3B, $3C, $08, $53, $22, $1B, $5B ; 0x93
    ;db $3A, $3B, $3C, $3D, $53, $47, $48, $5B ; 0x94
    ;db $3A, $3B, $3C, $3D, $53, $56, $4F, $5B ; 0x95
    ;db $3A, $3B, $3C, $3D, $35, $36, $3E, $5B ; 0x96
    ;db $3A, $3B, $3C, $3D, $57, $4C, $3E, $5B ; 0x97

    ;db $3A, $3B, $3C, $08, $00, $22, $1B, $5B ; 0x98
    ;db $3A, $3B, $3C, $08, $00, $22, $1B, $5B ; 0x99
    ;db $3A, $3B, $3C, $06, $53, $1F, $18, $5B ; 0x9A
    ;db $3A, $3B, $3C, $06, $53, $1F, $18, $5B ; 0x9B
    ;db $3A, $3B, $3C, $3D, $53, $33, $34, $5B ; 0x9C
    ;db $3A, $3B, $3C, $3D, $53, $57, $4C, $5B ; 0x9D
    ;db $3A, $3B, $3C, $3D, $57, $4C, $3E, $5B ; 0x9E
    ;db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B ; 0x9F
    warnpc $288980

    ; TODO: Add a way to edit these within ZS? Unsure.
    org $288980 ; $140980
    .DefaultGFXGroups

    ; LW
    org $288980 ; $140980
    .DefaultGFXGroups_sheet0
    db $3A ; Sheet 0

    org $288981 ; $140981
    .DefaultGFXGroups_sheet1
    db $3B ; Sheet 1

    org $288982 ; $140982
    .DefaultGFXGroups_sheet2
    db $3C ; Sheet 2

    org $288983 ; $140983
    .DefaultGFXGroups_sheet3
    db $3D ; Sheet 3

    org $288984 ; $140984
    .DefaultGFXGroups_sheet4
    db $53 ; Sheet 4

    org $288985 ; $140985
    .DefaultGFXGroups_sheet5
    db $4D ; Sheet 5

    org $288986 ; $140986
    .DefaultGFXGroups_sheet6
    db $3E ; Sheet 6

    org $288987 ; $140987
    .DefaultGFXGroups_sheet7
    db $5B ; Sheet 7

    ; DW
    db $42, $43, $44, $45, $2F, $30, $3F, $59

    ; SW
    db $3A, $3B, $3C, $3D, $47, $48, $3E, $5B

    warnpc $288998 ; $140998
}

; Debug addresses:
; $00D8D5 ; W7 Animated tiles on warp.
!Func00D8D5 = $01
; $00DA63 ; W8 Enable/Disable subscreen.
!Func00DA63 = $01
; $00E133 ; T2.5 (probably) ; Makes the game decompress the 3 static OW tile
; sheets on transition.
!Func00E133 = $01
; $006221 ; Changes the InitTilesets function to call from the long tables.
!Func00E221 = $01
; $00EEBB ; Zeros out the BG color when mirror warping to the pyramid area.
!Func00EEBB = $01
; $00FF7C ; W9 BG scrolling for HC and the pyramid area.
!Func00FF7C = $01

; Some old but now unused addresses:
; $028027
; $029C0C
; $029D1E
; $029F82

; $0283EE ; E2 ; Changes the function that loads overworld properties when
; exiting a dungeon. Includes removing asm that plays music in certain areas
; and changing how animated tiles are loaded.
!Func0283EE = $01
; $028632 ; Changes a function that loads animated tiles under certain
; conditions.
!Func028632 = $01
; $029AA6 ; E1 ; Changes part of a function that changes the sub mask color
; when leaving dungeons.
!Func029AA6 = $01
; $02AF58 ; T2 S2 W2 Main subscreen loading function.
!Func02AF58 = $01
; $02B2D4 ; W1 turns on subscreen for pyramid.
!Func02B2D4 = $01
; $02B3A1 ; W6 Activate subscreen durring pyramid warp.
!Func02B3A1 = $01 
; $02BC44 ; Controls overworld vertical subscreen movement for the pyramid BG.
!Func02BC44 = $01
; $02C02D ; T4 Changes how the pyramid BG scrolls durring transition.
!Func02C02D = $01
; $02C692 ; W3 Main palette loading routine.
!Func02C692 = $01 
; $02A4CD ; Rain animation code.
!Func02A4CD = $01
; $02AADB ; T1 Mosaic.
!Func02AADB = $01
; $02ABBE ; T3 transition animated and main palette.
!Func02ABBE = $01
; $0ABC5A ; Loads the animated tiles after the overworld map is closed.
!Func0ABC5A = $01
; $0AB8F5 ; Loads different animated tiles when returning from bird travel.
!Func0AB8F5 = $01
; $0BFEC6 ; W5 Load overlay, fixed color, and BG color.
!Func0BFEC6 = $01
; $0ED627 ; S1 W4 Transparent color durring warp and during special area enter.
!Func0ED627 = $01
; $0ED8AE ; Resets the area special color after the screen flashes.
!Func0ED8AE = $01

; Start of expanded space.
org $2892B8 ; $1412B8
pushpc

; ==============================================================================

if !Func00D8D5 = 1

; Replaces a function that decompresses animated tiles in certain mirror warp
; conditions.
org $00D8D5 ; $0058D5
AnimateMirrorWarp_DecompressAnimatedTiles:
{
    PHX

    ; The decompression function increases it by 1 so subtract 1 here.
    JSL ReadAnimatedTable : DEC : TAY

    PLX

    JSL DecompOwAnimatedTiles
            
    RTL
}
warnpc $00D8EE

else

; Undo the function above:
org $00D8D5 ; $0058D5
db $A0, $58, $A5, $8A, $29, $BF, $C9, $03
db $F0, $0A, $C9, $05, $F0, $06, $C9, $07
db $F0, $02, $A0, $5A, $22, $94, $D3, $00
db $6B

endif

pullpc
ReadAnimatedTable:
{
    PHB : PHK : PLB

    REP #$30 ; Set A, X, and Y in 16bit mode.
    LDA.b $8A : TAX
    AND.w #$00C0 : LSR #3 : TAY ; (Area / 8) = LW, DW, or SW *8
    SEP #$20 ; Set A in 8bit mode.

    ; $00 crashes the game so just double check that.
    LDA.w Pool_AnimatedTable, X : BNE .not007
        LDA.w Pool_DefaultGFXGroups_sheet7, Y

        BRA .notFF7

    .not007

    ; Load the default sheet if the value is FF.
    CMP.b #$FF : BNE .notFF7
        LDA.w Pool_DefaultGFXGroups_sheet7, Y

    .notFF7

    SEP #$10 ; Set X and Y in 8bit mode.

    PLB

    RTL
}
pushpc

; ==============================================================================

if !Func00DA63 = 1

org $00D8F4 ; $0058F4
    SheetsTable_0AA4:

; The first half of this function enables or disables BG1 for subscreen overlay
; use depending on the area. The second half reloads global sprite #2 sheet
; (rock vs skulls, different bush gfx, fish vs bone fish, etc.) based on what
; world we are in.
org $00DA63 ; $005A63
AnimateMirrorWarp_LoadSubscreen:
{
    JSL ActivateSubScreen

    ; From this point on it is the vanilla function.
    PHB : PHK : PLB
        
    ; TODO: Eventually un-hardcode this.
    ; X = 0 for LW, 8 for DW
    LDA.l SheetsTable_0AA4, X : TAY
        
    ; Get the pointer for one of the 2 Global sprite #2 sheets.
    LDA.w $D1B1, Y : STA.b $00
    LDA.w $D0D2, Y : STA.b $01
    LDA.w $CFF3, Y : STA.b $02 : STA.b $05
        
    PLB
        
    REP #$31 ; Set A, X, and Y in 16bit mode. +1 no idea.
        
    ; Source address is determined above, number of tiles is 0x0040, base
    ; target address is $7F0000.
    LDX.w #$0000
    LDY.w #$0040
        
    LDA.b $00
        
    JSR.w Do3To4High16Bit
    
    SEP #$30 ; Set A, X, and Y in 8bit mode.
        
    RTL
}
warnpc $00DABB

else

; Undo the function above:
org $00DA63 ; $005A63
db $64, $1D, $A5, $8A, $F0, $24, $C9, $70
db $F0, $20, $C9, $40, $F0, $1C, $C9, $5B
db $F0, $18, $C9, $03, $F0, $14, $C9, $05
db $F0, $10, $C9, $07, $F0, $0C, $C9, $43
db $F0, $08, $C9, $45, $F0, $04, $C9, $47
db $D0, $04, $A9, $01, $85, $1D, $8B, $4B
db $AB, $BF, $F4, $D8, $00, $A8, $B9, $B1
db $D1, $85, $00, $B9, $D2, $D0, $85, $01
db $B9, $F3, $CF, $85, $02, $85, $05, $AB
db $C2, $31, $A2, $00, $00, $A0, $40, $00
db $A5, $00, $20, $4F, $DF, $E2, $30, $6B

endif

pullpc
ActivateSubScreen:
{
    STZ.b $1D

    PHX

    REP #$20 ; Set A in 16bit mode.

    LDA.b $8A : BNE .notForest
        ; Check if we have the master sword.
        LDA.l $7EF300 : AND.w #$0040 : BEQ .notForest
            ; The forest canopy overlay.
            BRA .turnOn
    
    .notForest

    ; Check if we need to disable the rain in the misery mire.
    LDA.w Pool_EnableRainMireEvent : BEQ .notMire
        LDA.b $8A : CMP.w #$0070 : BNE .notMire
            ; Has Misery Mire been triggered yet?
            LDA.l $7EF2F0 : AND.w #$0020 : BNE .notMire
                BRA .turnOn
        
    .notMire

    ; Check if we are in the beginning phase, if not, no rain.
    ; If $7EF3C5 >= 0x02.
    LDA.l $7EF3C5 : AND.w #$00FF : CMP.w #$0002 : BCS .noRain
        BRA .turnOn
        
    .noRain
    
    ; Get the overlay value for this overworld area.
    JSL ReadOverlayArray : CMP.w #$00FF : BEQ .normal
        ; If not $FF, assume we want an overlay.

        .turnOn
        SEP #$20 ; Set A in 8bit mode.

        ; Turn on BG1.
        LDA.b #$01 : STA.b $1D

    .normal

    SEP #$20 ; Set A in 8bit mode.

    PLX

    RTL
}
pushpc

; ==============================================================================

if !Func00EEBB = 1

; Zeros out the BG color when mirror warping to the pyramid area.
; TODO: This is done in the vanilla I think as just a precaution at the apex of
; the fade to white to make sure all of the colors truly are white but it may
; not actually be needed.
org $00EEBB ; $006EBB
Func00EEBB:
{
    ; Check if we are warping to an area with the pyramid BG.
    JSL ReadOverlayArray : CMP.w #$0096 : BNE .notHyruleCastle
        ; This is annoying but I just needed a little bit of extra space.
        JSL EraseBGColors

    .notHyruleCastle

    SEP #$20 ; Set A in 8bit mode.
        
    LDA.b #$08 : STA.w $06BB
        
    STZ.w $06BA
        
    RTL
}
warnpc $00EEE0

else

; Undo the function above:
org $00EEBB ; $006EBB
db $A5, $8A, $C9, $1B, $00, $D0, $13, $A9
db $00, $00, $8F, $00, $C3, $7E, $8F, $40
db $C3, $7E, $8F, $00, $C5, $7E, $8F, $40
db $C5. $7E, $E2, $20, $A9, $08, $8D, $BB
db $06, $9C, $BA, $06, $6B

endif

pullpc
EraseBGColors:
{
    LDA.w #$0000 : STA.l $7EC300 : STA.l $7EC340 : STA.l $7EC500 : STA.l $7EC540

    RTL
}
pushpc

; ==============================================================================

if !Func00FF7C = 1

; Controls the BG scrolling for HC and the pyramid area.
org $00FF7C ; $007F7C
Func00FF7C:
{
    LDA.w $1C80 : ORA.w $1C90 : ORA.w $1CA0 : ORA.w $1CB0 : CMP.b $E2 : BNE .BRANCH_DELTA
        SEP #$30 ; Set A, X, and Y in 8bit mode.

        STZ.b $9B
        
        INC.b $B0
        
        JSL Overworld_SetFixedColorAndScroll

        REP #$30 ; Set A, X, and Y in 16bit mode.

        ; Check if we are warping to an area with the pyramid BG.
        JSL ReadOverlayArray : CMP.w #$0096 : BEQ .dont_align_bgs
            LDA.b $E2 : STA.b $E0 : STA.w $0120 : STA.w $011E
            LDA.b $E8 : STA.b $E6 : STA.w $0122 : STA.w $0124
        
        .dont_align_bgs
    .BRANCH_DELTA
    
    SEP #$30 ; Set A, X, and Y in 8bit mode.
        
    RTL
}
; This end point also uses up a null block at the end of the function.
warnpc $00FFC0

else

; Undo the function above:
org $00FF7C ; $007F7C
db $AD, $80, $1C, $0D, $90, $1C, $0D, $A0
db $1C, $0D, $B0, $1C, $C5, $E2, $D0, $28
db $E2, $20, $64, $9B, $E6, $B0, $22, $70
db $FE, $0B, $A5, $8A, $29, $3F, $C9, $1B
db $F0, $16, $C2, $20, $A5, $E2, $85, $E0
db $8D, $20, $01, $8D, $1E, $01, $A5, $E8
db $85, $E6, $8D, $22, $01, $8D, $24, $01
db $E2, $30, $6B, $FF, $FF, $FF, $FF, $FF
db $FF, $FF, $FF, $FF

endif

; ==============================================================================

if !Func0283EE = 1

; Replaces a bunch of calls to a shared function.
; Intro_SetupScreen:
org $028027 ; $010027
    JSR.w PreOverworld_LoadProperties_LoadMain_LoadMusicIfNeeded

warnpc $02802B

; Dungeon_LoadSongBankIfNeeded:
org $029C0C ; $011C0C
    JMP PreOverworld_LoadProperties_LoadMain_LoadMusicIfNeeded

warnpc $029C0F

; Mirror_LoadMusic:
org $029D1E ; $011D1E
    JSR.w PreOverworld_LoadProperties_LoadMain_LoadMusicIfNeeded

warnpc $029D21

; GanonEmerges_LOadPyramidArea:
org $029F82 ; $011F82
    JSR.w PreOverworld_LoadProperties_LoadMain_LoadMusicIfNeeded

warnpc $029F85

; Changes the function that loads overworld properties when exiting a dungeon.
; Includes removing asm that plays music in certain areas and changing how
; animated tiles are loaded.
org $0283EE ; $0103EE
PreOverworld_LoadProperties_LoadMain:
{
    LDX.b #$F3

    ; If the volume was set to half, set it back to full.
    LDA.w $0132 : CMP.b #$F2 : BEQ .setToFull
        ; If we're in the dark world
        ; If area number is < 0x40 or >= 80 we are not in the dark world.
        LDA.b $8A : CMP.b #$40 : BCC .setNormalSong
                    CMP.b #$80 : BCS .setNormalSong
            ; Does Link have a moon pearl?
            LDA.l $7EF357 : BNE .setNormalSong
                ; If not, play the music that plays when you're a bunny in the
                ; Dark World.
                LDX.b #$04

                BRA .setToFull

        .setNormalSong

        LDX.b $8A
        LDA.l $7F5B00, X : AND.b #$0F : TAX
        
    .setToFull
    
    ; The value written here will take effect during NMI.
    STX.w $0132

    ; Set the ambient sound. Removed becuase this is also done later on.
    ;LDX.b $8A
    ;LDA.l $7F5B00, X : LSR #4 : STA.w $012D
    
    ; The decompression function increases it by 1 so subtract 1 here.
    JSL ReadAnimatedTable : DEC : TAY

    JSL DecompOwAnimatedTiles

    ; Decompress all other graphics.
    JSL InitTilesets

    ; Load palettes for overworld.
    JSR.w Overworld_LoadAreaPalettes
        
    LDX.b $8A
        
    LDA.l $7EFD40, X : STA.b $00
        
    LDA.l $00FD1C, X
        
    ; Load some other palettes.
    JSL Overworld_LoadPalettes

    ; Sets the background color (changes depending on area).
    JSL Palette_SetOwBgColor_Long
        
    LDA.b $10 : CMP.b #$08 : BNE .specialArea2
        ; Copies $7EC300[0x200] to $7EC500[0x200].
        JSR.w Dungeon_LoadPalettes_cacheSettings
        
        BRA .normalArea2
    
    .specialArea2
    
    ; Apparently special overworld handles palettes a bit differently?
    JSR.w $C6EB ; $0146EB IN ROM
    
    .normalArea2
    
    ; Sets fixed colors and scroll values.
    JSL Overworld_SetFixedColorAndScroll
        
    ; Set darkness level to zero for the overworld.
    LDA.b #$00 : STA.l $7EC017
        
    ; Sets up properties in the event a tagalong shows up.
    JSL Tagalong_Init
        
    ; Set animated sprite gfx for area 0x00 and 0x40.
    LDA.b $8A : AND.b #$3F : BNE .notForestArea
        LDA.b #$1E
        
        JSL GetAnimatedSpriteTile_variable
    
    .notForestArea
    
    ; Cache the overworld mode 0x09.
    LDA.b #$09 : STA.w $010C
    JSL Sprite_OverworldReloadAll ; $09C499
        
    ; Are we in the dark world? If so, there's no warp vortex there.
    LDA.b $8A : AND.b #$40 : BNE .noWarpVortex
        JSL Sprite_ReinitWarpVortex
    
    .noWarpVortex
        
    ; Check if Blind disguised as a crystal maiden was following us when
    ; we left the dungeon area.
    LDA.l $7EF3CC : CMP.b #$06 : BNE .notBlindGirl
        ; If it is Blind, kill her!
        LDA.b #$00 : STA.l $7EF3CC
    
    .notBlindGirl
    
    ; Reset player variables.
    STZ.b $6C   ; In doorway flag
    STZ.b $3A   ; BY Bitfield
    STZ.b $3C   ; B Button timer
    STZ.b $50   ; Link strafe
    STZ.b $5E   ; Link speed handler
    STZ.w $0351 ; Link feet gfx fx
        
    ; Reinitialize many of Link's gameplay variables.
    JSR.w $8B0C ; $010B0C IN ROM
        
    LDA.l $7EF357 : BNE .notBunny
    LDA.l $7EF3CA : BEQ .notBunny
        LDA.b #$01 : STA.w $02E0 : STA.b $56
        
        LDA.b #$17 : STA.b $5D
        
        JSL LoadGearPalettes_bunny
    
    .notBunny
    
    ; Set screen to mode 1 with BG3 priority.
    LDA.b #$09 : STA.b $94
        
    LDA.b #$00 : STA.l $7EC005
        
    STZ.w $046C ; Collision BG1 flag
    STZ.b $EE   ; Reset Link layer to BG2
    STZ.w $0476 ; Another layer flag
        
    INC.b $11 ; SCAWFUL: We should verify what submodule this is moving to.
    INC.b $16 ; NMI HUD Update flag
        
    STZ.w $0402 : STZ.w $0403

    ; Vanilla alternate entry point. Called in 4 different locations all of
    ; which are overwritten above.
    .LoadMusicIfNeeded

    LDA.w $0136 : BEQ .no_music_load_needed
        SEI
        
        ; Shut down NMI until music loads.
        STZ.w $4200
        
        ; Stop all HDMA.
        STZ.w $420C
        
        STZ.w $0136
        
        LDA.b #$FF : STA.w $2140
        
        JSL Sound_LoadLightWorldSongBank
        
        ; Re-enable NMI and joypad.
        LDA.b #$81 : STA.w $4200
    
    .no_music_load_needed

    ; PLACE CUSTOM GFX LOAD HERE!
    ;JSL CheckForChangeGraphicsNormalLoadCastle
    
    RTS
}
warnpc $02856A ; $01056A

else

; Undo the function above:
org $028027 ; $010027
    db $20, $4C, $85, $C2

org $029C0C ; $011C0C
    db $4C, $4C, $85, $A5

org $029D1E ; $011D1E
    db $20, $4C, $85

org $029F82 ; $011F82
    db $20, $4C, $85

org $0283EE ; $0103EE
db $A0, $58, $A2, $02, $A5, $8A, $C9, $03
db $F0, $6F, $C9, $05, $F0, $6B, $C9, $07
db $F0, $67, $A2, $09, $A5, $8A, $C9, $43
db $F0, $5F, $C9, $45, $F0, $5B, $C9, $47
db $F0, $57, $A0, $5A, $A5, $8A, $C9, $40
db $B0, $3A, $A2, $07, $AF, $C5, $F3, $7E
db $C9, $03, $90, $02, $A2, $02, $A5, $A0
db $C9, $E3, $F0, $3D, $C9, $18, $F0, $39
db $C9, $2F, $F0, $35, $A5, $A0, $C9, $1F
db $D0, $06, $A5, $8A, $C9, $18, $F0, $29
db $A2, $05, $AF, $00, $F3, $7E, $29, $40
db $F0, $02, $A2, $02, $A5, $A0, $F0, $19
db $C9, $E1, $F0, $15, $A2, $F3, $AD, $32
db $01, $C9, $F2, $F0, $30, $A2, $02, $AF
db $C5, $F3, $7E, $C9, $02, $B0, $02, $A2
db $03, $AF, $CA, $F3, $7E, $F0, $1E, $A2
db $0D, $A5, $8A, $C9, $40, $F0, $0E, $C9
db $43, $F0, $0A, $C9, $45, $F0, $06, $C9
db $47, $F0, $02, $A2, $09, $AF, $57, $F3
db $7E, $D0, $02, $A2, $04, $8E, $32, $01
db $22, $94, $D3, $00, $22, $9B, $E1, $00
db $20, $92, $C6, $A6, $8A, $BF, $40, $FD
db $7E, $85, $00, $BF, $1C, $FD, $00, $22
db $A8, $D5, $0E, $22, $18, $D6, $0E, $A5
db $10, $C9, $08, $D0, $05, $20, $5F, $C6
db $80, $03, $20, $EB, $C6, $22, $70, $FE
db $0B, $A9, $00, $8F, $17, $C0, $7E, $22
db $FC, $9E, $09, $A5, $8A, $29, $3F, $D0
db $06, $A9, $1E, $22, $ED, $D4, $00, $A9
db $09, $8D, $0C, $01, $22, $99, $C4, $09
db $A5, $8A, $29, $40, $D0, $04, $22, $89
db $AF, $09, $A2, $05, $AF, $C5, $F3, $7E
db $C9, $02, $B0, $02, $A2, $01, $8E, $2D
db $01, $AF, $CC, $F3, $7E, $C9, $06, $D0
db $06, $A9, $00, $8F, $CC, $F3, $7E, $64
db $6C, $64, $3A, $64, $3C, $64, $50, $64
db $5E, $9C, $51, $03, $20, $0C, $8B, $AF
db $57, $F3, $7E, $D0, $15, $AF, $CA, $F3
db $7E, $F0, $0F, $A9, $01, $8D, $E0, $02
db $85, $56, $A9, $17, $85, $5D, $22, $DD
db $D6, $0E, $A9, $09, $85, $94, $A9, $00
db $8F, $05, $C0, $7E, $9C, $6C, $04, $64
db $EE, $9C, $76, $04, $E6, $11, $E6, $16
db $9C, $02, $04, $9C, $03, $04, $AD, $36
db $01, $F0, $18, $78, $9C, $00, $42, $9C
db $0C, $42, $9C, $36, $01, $A9, $FF, $8D
db $40, $21, $22, $13, $89, $00, $A9, $81
db $8D, $00, $42, $60

endif

; ==============================================================================

if !Func028632 = 1

; Changes a function that loads animated tiles under certain conditions.
org $028632 ; $010632
Func028632:
{
    ; The decompression function increases it by 1 so subtract 1 here.
    JSL ReadAnimatedTable : DEC : TAY
    
    JSL DecompOwAnimatedTiles
        
    ; SCAWFUL: Verify the submodule ID being manipulated here.
    LDA.b $11 : LSR A : TAX
        
    ; SCAWFUL: Spriteset1 $0AA3 is being modified, let's verify the table.
    LDA.l $0285E2, X : STA.w $0AA3
        
    LDA.l $0285F3, X : PHA
        
    JSL InitTilesets

    ; Load Palettes.
    JSR.w Overworld_LoadAreaPalettes
    PLA : STA.b $00
        
    LDX.b $8A
        
    LDA.l $00FD1C, X
        
    JSL Overworld_LoadPalettes
        
    LDA.b #$01 : STA.w $0AB2
        
    JSL Palette_Hud
        
    LDA.l $11 : BNE .BRANCH_4
        JSL CopyFontToVram
    
    .BRANCH_4
    
    JSR.w Dungeon_LoadPalettes_cacheSettings
    JSL Overworld_SetFixedColorAndScroll
        
    LDA.l $8A : CMP.b #$80 : BCC .BRANCH_5
        JSL Palette_SetOwBgColor_Long
    
    .BRANCH_5
    
    LDA.b #$09 : STA.b $94
        
    INC.b $B0
        
    RTS
}
warnpc $028697

else

; Undo the function above:
org $028632 ; $010632
db $A0, $58, $A5, $8A, $29, $BF, $C9, $03
db $F0, $0A, $C9, $05, $F0, $06, $C9, $07
db $F0, $02, $A0, $5A, $22, $94, $D3, $00
db $A5, $11, $4A, $AA, $BF, $E2, $85, $02
db $8D, $A3, $0A, $BF, $F3, $85, $02, $48
db $22, $9B, $E1, $00, $20, $92, $C6, $68
db $85, $00, $A6, $8A, $BF, $1C, $FD, $00
db $22, $A8, $D5, $0E, $A9, $01, $8D, $B2
db $0A, $22, $52, $EE, $1B, $A5, $11, $D0
db $04, $22, $56, $E5, $00, $20, $5F, $C6
db $22, $70, $FE, $0B, $A5, $8A, $C9, $80
db $90, $04, $22, $18, $D6, $0E, $A9, $09
db $85, $94, $E6, $B0, $60

endif

; ==============================================================================

if !Func029AA6 = 1

; Changes part of a function that changes the sub mask color when leaving
; dungeons.
org $029AA6 ; $011AA6
Func029AA6:
{
    ; Setup fixed color values based on area number.
    LDX.w #$4C26
    LDY.w #$8C4C
        
    ; TODO: Wtf why is this 0x00?
    ; Check for LW death mountain.
    JSL ReadOverlayArray : CMP.w #$0095 : BEQ .mountain
        LDX.w #$4A26 : LDY.w #$874A
        
        ; Check for DW death mountain.
        CMP.w #$009C : BEQ .mountain
            BRA .other
    
    .mountain
    
    STX.b $9C : STY.b $9D
    
    .other
    
    SEP #$30 ; Set A, X, and Y in 8bit mode.
        
    RTS
}
warnpc $029AD3

else

; Undo the function above:
org $029AA6 ; $011AA6
db $A5, $8A, $C9, $03, $00, $F0, $1F, $C9
db $05, $00, $F0, $1A, $C9, $07, $00, $F0
db $15, $A2, $26, $4A, $A0, $4A, $87, $C9
db $43, $00, $F0, $0A, $C9, $45, $00, $F0
db $05, $C9, $47, $00, $D0, $04, $86, $9C
db $84, $9D, $E2, $30, $60

endif

; ==============================================================================

if !Func02AF58 = 1

; Main subscreen overlay loading function. Changed so that they will load
; from a table. This does not change the event overlays like the lost woods 
; changing to the tree canopy, the master sword area or the misery mire rain.
; This also does not change the overlay for under the bridge because it shares
; an area with the master sword.
org $02AF58 ; $012F58
CustomOverworld_LoadSubscreenOverlay_PostInit:
{
    SEP #$20 ; Set A in 8bit mode.

    ; Check to see if we are using the mirror so that our $A0 doesn't
    ; accidentally persist and we trigger rain sounds when we don't want them.
    LDA.b $11 : CMP.b #$23 : BEQ .mirrorWarp
                CMP.b #$24 : BEQ .mirrorWarp
                CMP.b #$2C : BEQ .mirrorWarp
        ; We can't warp to or from a special area anyway so this is fine.

        REP #$20 ; Set A in 16bit mode.

        ; Check to see if we are in a SW overworld area.
        LDA.b $8A : CMP.w #$0080 : BCC .notExtendedArea
            ; $0182 is the exit room number used for getting to Zora's Domain.
            LDA.b $A0 : CMP.w #$0182 : BNE .notZoraFalls   
                SEP #$20 ; Set A in 8bit mode.

                ; Play rain (waterfall) sound.
                ; TODO: Write a patch to change/disable this.
                LDA.b #$01 : STA.w $012D

                REP #$20 ; Set A in 16bit mode.

            .notZoraFalls

            ; Check for exit rooms (the faked way of getting from one overworld
            ; area to another). $0180 is the exit room number used for getting
            ; into the mastersword area.
            LDA.b $A0 : CMP.w #$0180 : BNE .notMasterSwordArea
                ; If the Master sword is retrieved, don't do the mist overlay.
                LDA.l $7EF300 : AND.w #$0040 : BNE .masterSwordRecieved
                    JSL ReadOverlayArray : TAX

                    .loadOverlayShortcut

                    ; Save the overlay for later.
                    PHX

                    JMP .loadSubScreenOverlay

                .masterSwordRecieved

                ; TODO: Write a patch to change what overlay is loaded here?
                BRA .noSubscreenOverlay 
        
            .notMasterSwordArea

            ; TODO: Write a patch to change what overlay is loaded here?
            ; The second mastersword/under the bridge area.
            LDX.w #$0094

            ; $0181 is the exit room number used for getting into the under the
            ; bridge area.
            LDA.b $A0 : CMP.w #$0181 : BEQ .loadOverlayShortcut
                ; TODO: Write a patch to change what overlay is loaded here?
                ; The second Triforce room area.
                LDX.w #$0093

                ; $0189 is the exit room number used for getting to the
                ; Triforce room.
                CMP.w #$0189 : BEQ .loadOverlayShortcut
                    .noSubscreenOverlay
                        
                    SEP #$30 ; Set A, X, and Y in 8bit mode.
                        
                    STZ.b $1D ; Clear TSQ PPU Register, to be handled in NMI.
                            
                    INC.b $11 ; SCAWFUL: Verify the submodule we are moving to.
                            
                    RTS
        
        .notExtendedArea
    .mirrorWarp

    REP #$20 ; Set A in 16bit mode.

    JSL ReadOverlayArray : TAX
    
    LDA.b $8A : BNE .notForest
        ; Check if we have the master sword.
        LDA.l $7EF300 : AND.w #$0040 : BEQ .notForest
            ; TODO: Write a patch to change this?
            ; The forest canopy overlay.
            LDX.w #$009E
    
    .notForest

    ; Check if we need to disable the rain in the misery mire.
    LDA.l Pool_EnableRainMireEvent : BEQ .notMire
        LDA.b $8A : CMP.w #$0070 : BNE .notMire
            ; Has Misery Mire been triggered yet?
            LDA.l $7EF2F0 : AND.w #$0020 : BNE .notMire
                ; The rain overlay.
                LDX.w #$009F

                SEP #$20 ; Set A in 8bit mode.

                ; Load the rain sound effect.
                ; This is done here because of some jank in the vanilla code in
                ; this function a bit further down. Basically it loads the
                ; overlay's ambient sound instead of the acutal areas, which
                ; only seems to benefit us here.
                LDA.b #$01 : STA.w $012D

                REP #$20 ; Set A in 16bit mode.
        
    .notMire

    ; Check if we are in the beginning phase, if not, no rain.
    ; If $7EF3C5 >= 0x02.
    LDA.l Pool_EnableBeginningRain : BEQ .noRain
        LDA.l $7EF3C5 : AND.w #$00FF : CMP.w #$0002 : BCS .noRain
            ; The rain overlay.
            LDX.w #$009F
        
    .noRain
    
    ; Store the overlay for later.
    PHX

    ; If the value is 0xFF that means we didn't set any overlay so load the
    ; pyramid one by default.
    CPX.w #$00FF : BNE .notFF
        ; The pyramid background.
        LDX.w #$0096

    .notFF
    
    ; $01300B ALTERNATE ENTRY POINT ; TODO: Verify this. If it is an alternate
    ; entry I can't find where it is referenced anywhere.
    .loadSubScreenOverlay
    STY.b $84
        
    STX.b $8A : STX.b $8C

    ; Overworld map16 buffer manipulation during scrolling.
    LDA.b $84 : SEC : SBC.w #$0400 : AND.w #$0F80 : ASL A : XBA : STA.b $88
    LDA.b $84 : SEC : SBC.w #$0010 : AND.w #$003E : LSR A : STA.b $86
        
    STZ.w $0418 : STZ.w $0410 : STZ.w $0416
        
    SEP #$30 ; Set A, X, and Y in 8bit mode.
        
    ; Color +/- buffered register.
    LDA.b #$82 : STA.b $99
        
    ; Puts OBJ, BG2, and BG3 on the main screen.
    LDA.b #$16 : STA.b $1C
        
    ; Puts BG1 on the subscreen.
    LDA.b #$01 : STA.b $1D

    ; Pull the 16 bit overlay from earlier and just discard the high byte.
    PLX : PLA
        
    ; One possible configuration for $2131 (CGADSUB).
    LDA.b #$72
        
    ; Comparing different screen types.
    CPX.b #$97 : BEQ .loadOverlay ; Fog 1
    CPX.b #$94 : BEQ .loadOverlay ; Master sword/bridge 2
    CPX.b #$93 : BEQ .loadOverlay ; Triforce room 2
    CPX.b #$9D : BEQ .loadOverlay ; Fog 2
    CPX.b #$9E : BEQ .loadOverlay ; Tree canopy
    CPX.b #$9F : BEQ .loadOverlay ; Rain
        ; Alternative setting for CGADSUB (only background is enabled on
        ; subscreen).
        LDA.b #$20 
        
        CPX.b #$95 : BEQ .loadOverlay ; Sky
        CPX.b #$9C : BEQ .loadOverlay ; Lava
        CPX.b #$96 : BEQ .loadOverlay ; Pyramid BG
            LDX.b $11
        
            ; TODO: Investigate what these checks are for.
            CPX.b #$23 : BEQ .loadOverlay
            CPX.b #$2C : BEQ .loadOverlay
                STZ.b $1D
    
    .loadOverlay
    
    ; Apply the selected settings to CGADSUB's mirror ($9A).
    STA.b $9A
        
    JSR.w LoadSubscreenOverlay
        
    ; This is the "under the bridge" area.
    LDA.b $8C : CMP.b #$94 : BNE .notUnderBridge
        ; All this is doing is setting the X coordinate of BG1 to 0x0100
        ; rather than 0x0000. (this area uses the second half of the data only,
        ; similar to the master sword area).
        LDA.b $E7 : ORA.b #$01 : STA.b $E7
    
    .notUnderBridge
    
    REP #$20 ; Set A in 16bit mode.
        
    ; We were pretending to be in a different area to load the subscreen
    ; overlay, so we're restoring all those settings.
    LDA.l $7EC213 : STA.b $8A
    LDA.l $7EC215 : STA.b $84
    LDA.l $7EC217 : STA.b $88
    LDA.l $7EC219 : STA.b $86
        
    LDA.l $7EC21B : STA.w $0418
    LDA.l $7EC21D : STA.w $0410
    LDA.l $7EC21F : STA.w $0416
        
    SEP #$20 ; Set A in 8bit mode.
        
    RTS
}
warnpc $02B0D2 ; $0130D2

else

; Undo the function above:
org $02AF58 ; $012F58
db $A5, $8A, $C9, $80, $00, $90, $44, $A2
db $97, $00, $A5, $A0, $C9, $80, $01, $D0
db $12, $A2, $80, $00, $BF, $80, $F2, $7E
db $A2, $97, $00, $29, $40, $00, $D0, $24
db $4C, $0B, $B0, $A2, $94, $00, $C9, $81
db $01, $F0, $F5, $A2, $93, $00, $C9, $89
db $01, $F0, $ED, $C9, $82, $01, $F0, $05
db $C9, $83, $01, $D0, $07, $E2, $30, $A9
db $01, $8D, $2D, $01, $E2, $30, $64, $1D
db $E6, $11, $60, $29, $3F, $00, $D0, $1B
db $A5, $8A, $29, $40, $00, $D0, $0F, $A2
db $80, $00, $BF, $80, $F2, $7E, $A2, $9E
db $00, $29, $40, $00, $D0, $4D, $A2, $9D
db $00, $80, $48, $A2, $95, $00, $A5, $8A
db $C9, $03, $00, $F0, $3E, $C9, $05, $00
db $F0, $39, $C9, $07, $00, $F0, $34, $A2
db $9C, $00, $C9, $43, $00, $F0, $2C, $C9
db $45, $00, $F0, $27, $C9, $47, $00, $F0
db $22, $C9, $70, $00, $D0, $0B, $AF, $F0
db $F2, $7E, $29, $20, $00, $D0, $14, $80
db $0F, $A2, $96, $00, $AF, $C5, $F3, $7E
db $29, $FF, $00, $C9, $02, $00, $B0, $03
db $A2, $9F, $00, $84, $84, $86, $8A, $86
db $8C, $A5, $84, $38, $E9, $00, $04, $29
db $80, $0F, $0A, $EB, $85, $88, $A5, $84
db $38, $E9, $10, $00, $29, $3E, $00, $4A
db $85, $86, $9C, $18, $04, $9C, $10, $04
db $9C, $16, $04, $E2, $30, $A9, $82, $85
db $99, $A9, $16, $85, $1C, $A9, $01, $85
db $1D, $DA, $A6, $8A, $BF, $00, $5B, $7F
db $4A, $4A, $4A, $4A, $8D, $2D, $01, $FA
db $A9, $72, $E0, $97, $F0, $39, $E0, $94
db $F0, $35, $E0, $93, $F0, $31, $E0, $9D
db $F0, $2D, $E0, $9E, $F0, $29, $E0, $9F
db $F0, $25, $A9, $20, $E0, $95, $F0, $1F
db $E0, $9C, $F0, $1B, $AF, $13, $C2, $7E
db $AA, $A9, $20, $E0, $5B, $F0, $10, $E0
db $1B, $D0, $0A, $A6, $11, $E0, $23, $F0
db $06, $E0, $2C, $F0, $02, $64, $1D, $85
db $9A, $20, $0D, $FD, $A5, $8C, $C9, $94
db $D0, $06, $A5, $E7, $09, $01, $85, $E7
db $C2, $20, $AF, $13, $C2, $7E, $85, $8A
db $AF, $15, $C2, $7E, $85, $84, $AF, $17
db $C2, $7E, $85, $88, $AF, $19, $C2, $7E
db $85, $86, $AF, $1B, $C2, $7E, $8D, $18
db $04, $AF, $1D, $C2, $7E, $8D, $10, $04
db $AF, $1F, $C2, $7E, $8D, $16, $04, $E2
db $20, $60

endif

; ==============================================================================

if !Func02B2D4 = 1

; Turns on the subscreen if the pyramid is loaded.
org $02B2D4 ; $0132D4
Func02B2D4:
{
    JSR.w Overworld_LoadSubscreenAndSilenceSFX1

    ; In vanilla a check for the overlay is done here but we don't need
    ; it at all. It is handled in Func02B3A1 later on.
    ;JSL EnableSubScreenCheckForPyramid

    RTL
}
warnpc $02B2E6 ; $0132E6

else

; Undo the function above:
org $02B2D4 ; $0132D4
db $20, $19, $AF, $A5, $8A, $C9, $1B, $F0
db $04, $C9, $5B, $D0, $04, $A9, $01, $85
db $1D, $6B

endif

pullpc
EnableSubScreenCheckForPyramid:
{
    REP #$20 ; Set A in 16bit mode.

    JSL ReadOverlayArray
        
    CMP.w #$0096 : BNE .notPyramidOrCastle
        SEP #$20 ; Set A in 8bit mode.
        LDA.b #$01 : STA.b $1D
    
    .notPyramidOrCastle

    SEP #$20 ; Set A in 8bit mode.

    RTL
}
pushpc

; ==============================================================================

if !Func02B3A1 = 1

; Handles activating the subscreen and special BG color when warping to an area
; with the pyramid BG.
org $02B3A1 ; $0133A1
Func02B3A1:
{
    JSL EnableSubScreenCheckForPyramid
    
    REP #$20 ; Set A in 16bit mode.
        
    LDX.b #$00
        
    LDA.w #$7FFF
    
    .setBgPalettesToWhite
        STA.l $7EC540, X
        STA.l $7EC560, X
        STA.l $7EC580, X

        STA.l $7EC5A0, X
        STA.l $7EC5C0, X
        STA.l $7EC5E0, X
        
    INX #2 : CPX.b #$20 : BNE .setBgPalettesToWhite
        
    ; Also set the background color to white.
    STA.l $7EC500

    JSL ReadOverlayArray
        
    ; This sets the color to transparent so that we don't see an additional
    ; white layer on top of the pyramid bg.
    CMP.w #$0096 : BNE .notPyramidOfPower
        LDA.w #$0000 : STA.l $7EC500 : STA.l $7EC540
    
    .notPyramidOfPower
    
    SEP #$20 ; Set A in 8bit mode.
        
    JSL Sprite_ResetAll
    JSL Sprite_OverworldReloadAll
    JSL Link_ItemReset_FromOverworldThings
    JSR.w DeleteCertainAncillaeStopDashing
        
    LDA.b #$14 : STA.b $5D
        
    LDA.b $8A : AND.b #$40 : BNE .darkWorld
        JSL Sprite_ReinitWarpVortex
    
    .darkWorld
    
    RTL
}
warnpc $02B40A ; $01340A

else

; Undo the function above:
org $02B3A1 ; $0133A1
db $A5, $8A, $C9, $1B, $F0, $04, $C9, $5B
db $D0, $04, $A9, $01, $85, $1D, $C2, $20
db $A2, $00, $A9, $FF, $7F, $9F, $40, $C5
db $7E, $9F, $60, $C5, $7E, $9F, $80, $C5
db $7E, $9F, $A0, $C5, $7E, $9F, $C0, $C5
db $7E, $9F, $E0, $C5, $7E, $E8, $E8, $E0
db $20, $D0, $E2, $8F, $00, $C5, $7E, $A5
db $8A, $C9, $5B, $00, $D0, $0B, $A9, $00
db $00, $8F, $00, $C5, $7E, $8F, $40, $C5
db $7E, $E2, $20, $22, $4E, $C4, $09, $22
db $99, $C4, $09, $22, $07, $B1, $07, $20
db $0C, $8B, $A9, $14, $85, $5D, $A5, $8A
db $29, $40, $D0, $04, $22, $89, $AF, $09
db $6B

endif

; ==============================================================================

if !Func02BC44 = 1

; Controls overworld vertical subscreen movement for the pyramid BG.
org $02BC44 ; $013C44
Func02BC44:
{
    ; Check for the pyramid BG.
    JSL ReadOverlayArray : CMP.w #$0096 : BNE .BRANCH_IOTA
        JSL BGControl
        BRA .BRANCH_IOTA
    
    warnpc $02BC60 ; $013C60

    org $02BC60 ; $013C60
    .BRANCH_IOTA
}
warnpc $02BC60

else

; Undo the function above:
org $02BC44 ; $013C44
db $A5, $8A, $29, $3F, $00, $C9, $1B, $00
db $D0, $12, $A9, $00, $06, $C5, $E6, $90
db $02, $85, $E6, $A9, $C0, $06, $C5, $E6
db $B0, $02, $85, $E6

endif

pullpc
ReadOverlayArray:
{
    PHB : PHK : PLB

    LDA.b $8A : ASL : TAX
    LDA.w Pool_OverlayTable, X

    PLB

    RTL
}

BGControl:
{
    ; TODO: These comparison values will need to be calulated somehow or set
    ; depending on the area. Right now they are hardcoded to work with the
    ; pyramid area.

    ; Check link's Y position. This will need to be changed per area and per
    ; need.
    LDA.b $20 : CMP.w #$08E0 : BCC .startShowingMountains
        ; Lock the position so that nothing shows through the trees.
        LDA.w #$06C0 : STA.b $E6

        RTL

    .startShowingMountains

    ; Don't let the BG scroll down further than the "top" of the bg when
    ; walking up.
    LDA.w #$0600 : CMP.b $E6 : BCC .dontLock ; #$0600 
        STA.b $E6
    
    .dontLock
    
    ; Don't let the BG scroll up further than the "bottom" of the bg when
    ; walking down.
    LDA.w #$06C0 : CMP.b $E6 : BCS .dontLock2 ; #$06C0 
        STA.b $E6 ; $TODO: I had this at $E2 for some reason.

    .dontLock2

    RTL
}
pushpc

; ==============================================================================

if !Func02C02D = 1

; Changes how the pyramid BG scrolls durring transition.
org $02C02D ; $01402D
Func02C02D:
{
    PHA
    JSL ReadOverlayArray2
    PLA
    
    ; Check for the pyramid BG.
    CPY.b #$96 : BEQ .dontMoveBg1  
        ; This shifts the BG over by a half small area's width. This is to
        ; line up the mountain with the tower in the distance at the appropriate
        ; location when coming into the pyramid area from the right.
        STA.b $E0, X 

        ; NOTE: There is currently a bug in vanilla where if you exit a dungeon
        ; into the LW death mountain the sky background will become miss-aligned
        ; and this movement will cause the sky to flicker or jump when moving to
        ; another area. In order to fix this you would have to find the
        ; alignment exit code and change how the game aligns BG2 when exiting.
        ; Possibly when using the bird too.
    
    .dontMoveBg1
}
warnpc $02C039 ; $014039

else

; Undo the function above:
org $02C02D ; $01402D
db $A4, $8A, $C0, $1B, $F0, $06, $C0, $5B
db $F0, $02, $95, $E0

endif

pullpc
ReadOverlayArray2:
{
    PHX

    ; A is already 16 bit here.
    REP #$10 ; Set X and Y in 16bit mode.

    JSL ReadOverlayArray : TAY

    SEP #$10 ; Set X and Y in 8bit mode.

    PLX

    RTL
}
pushpc

; ==============================================================================

if !Func02C692 = 1

; Replaces a call to a shared function. Normally this is goes to .lightworld
; to change the main color palette manually but we change it here so that it
; just uses the same table as everything else.
org $02A07A ; $01207A
    JSR.w Overworld_LoadAreaPalettes

warnpc $02A07D ; $01207D

; The main overworld palette loading routine un-hardcoded to load the custom
; main palette.
org $02C692 ; $014692
Overworld_LoadAreaPalettes:
{
    LDX.b $8A
    LDA.l Pool_MainPaletteTable, X 
    
    ; $0AB3 =
    ; 0 - LW
    ; 1 - DW
    ; 2 - LW death mountain
    ; 3 - DW death mountain
    ; 4 - triforce room
    STA.w $0AB3

    ; Reset pal buffer high byte.
    STZ.w $0AA9
        
    ; Load SP1 through SP4.
    JSL Palette_MainSpr

    ; Load SP0 (2nd half) and SP6 (2nd half).
    JSL Palette_MiscSpr

    ; Load SP5 (1st half).
    JSL Palette_SpriteAux1

    ; Load SP6 (1st half).
    JSL Palette_SpriteAux2

    ; Load SP5 (2nd half, 1st 3 colors), which is the sword palette.
    JSL Palette_Sword
    
    ; Load SP5 (2nd half, next 4 colors), which is the shield.
    JSL Palette_Shield

    ; Load SP7 (full) Link's whole palette, including Armor.
    JSL Palette_ArmorAndGloves
        
    LDX.b #$01
        
    ; Changes the Palette_SpriteAux3 load depending on if we are in the LW or
    ; not. Will probably need it own custom table in the future? not sure.
    LDA.l $7EF3CA : AND.b #$40 : BEQ .lightWorld2
        LDX.b #$03
    
    .lightWorld2
    
    ; Reset pal buffer0.
    STX.w $0AAC
        
    ; Load SP0 (first half) (or SP7 (first half)).
    JSL Palette_SpriteAux3

    ; Load BP0 and BP1 (first halves).
    JSL Palette_Hud

    ; Load BP2 through BP5 (first halves).
    JSL Palette_OverworldBgMain
        
    RTS
}
warnpc $02C6EB ; $0146EB

else

; Undo the function above:
org $02A07A ; $01207A
db $20, $AD, $C6

org $02C692 ; $14692
db $A2, $02, $A5, $8A, $29, $3F, $C9, $03
db $F0, $0A, $C9, $05, $F0, $06, $C9, $07
db $F0, $02, $A2, $00, $A5, $8A, $29, $40
db $F0, $01, $E8, $8E, $B3, $0A, $9C, $A9
db $0A, $22, $9E, $EC, $1B, $22, $6E, $ED
db $1B, $22, $C5, $EC, $1B, $22, $E4, $EC
db $1B, $22, $03, $ED, $1B, $22, $29, $ED
db $1B, $22, $F9, $ED, $1B, $A2, $01, $AF
db $CA, $F3, $7E, $29, $40, $F0, $02, $A2
db $03, $8E, $AC, $0A, $22, $77, $EC, $1B
db $22, $52, $EE, $1B, $22, $C7, $EE, $1B
db $60

endif

; ==============================================================================

if !Func02A4CD = 1

; Rain animation code. Just replaces a single check that checks for the
; misery mire to instead check the current overlay to see if it's rain.
org $02A4CD ; $0124CD
RainAnimation:
{
    LDA.b $8C : CMP.b #$9F : BEQ .rainOverlaySet
        ; Check the progress indicator.
        LDA.l $7EF3C5 : CMP.b #$02 : BRA .skipMovement
            .rainOverlaySet

            ; If misery mire has been opened already, we're done.
            ;LDA.l $7EF2F0 : AND.b #$20 : BNE .skipMovement
                ; Check the frame counter.
                ; On the third frame do a flash of lightning.
                LDA.b $1A

                ; On the 0x03rd frame, cue the lightning.
                CMP.b #$03 : BEQ .lightning
                    ; On the 0x05th frame, normal light level.
                    CMP.b #$05 : BEQ .normalLight
                        ; On the 0x24th frame, cue the thunder.
                        CMP.b #$24 : BEQ .thunder
                            ; On the 0x2Cth frame, normal light level.
                            CMP.b #$2C : BEQ .normalLight
                                ; On the 0x58th frame, cue the lightning.
                                CMP.b #$58 : BEQ .lightning
                                    ; On the 0x5Ath frame, normal light level.
                                    CMP.b #$5A : BNE .moveOverlay

                .normalLight

                ; Keep the screen semi-dark.
                LDA.b #$72

                BRA .setBrightness

                .thunder

                ; Play the thunder sound when outdoors.
                LDX.b #$36 : STX.w $012E

                .lightning

                ; Make the screen flash with lightning.
                LDA.b #$32

                .setBrightness

                STA.b $9A

                .moveOverlay

                ; Overlay is only moved every 4th frame.
                LDA.b $1A : AND.b #$03 : BNE .skipMovement
                    LDA.w $0494 : INC A : AND.b #$03 : STA.w $0494 : TAX

                    LDA.b $E1 : CLC : ADC.l $02A46D, X : STA.b $E1
                    LDA.b $E7 : CLC : ADC.l $02A471, X : STA.b $E7

    .skipMovement

    RTL
}
warnpc $02A52D ; $01252D

else

; Undo the function above:
org $02A4CD ; $0124CD
db $A5, $8A, $C9, $70, $F0, $08, $AF, $C5
db $F3, $7E, $C9, $02, $B0, $51, $AF, $F0
db $F2, $7E, $29, $20, $D0, $49, $A5, $1A
db $C9, $03, $F0, $1D, $C9, $05, $F0, $10
db $C9, $24, $F0, $10, $C9, $2C, $F0, $08
db $C9, $58, $F0, $0D, $C9, $5A, $D0, $0D
db $A9, $72, $80, $07, $A2, $36, $8E, $2E
db $01, $A9, $32, $85, $9A, $A5, $1A, $29
db $03, $D0, $1C, $AD, $94, $04, $1A, $29
db $03, $8D, $94, $04, $AA, $A5, $E1, $18
db $7F, $6D, $A4, $02, $85, $E1, $A5, $E7
db $18, $7F, $71, $A4, $02, $85, $E7, $6B

endif

; ==============================================================================

if !Func02AADB = 1

; Main Mosaic Hook. Changes it to use a table instead of hardcoded to the woods
; areas.
org $02AADB ; $012ADB
    JML MosaicAreaCheck

warnpc $02AADF ; $012ADF

else

; Undo the function above:
org $02AADB ; $012ADB
db $29, $3F, $F0, $06

endif

pullpc
MosaicAreaCheck:
{
    PHB : PHK : PLB

    ; Check if the area we are in needs a mosaic.
    TAX
    LDA.w Pool_MosaicTable, X

    BEQ .noMosaic1
        PLB
        JML $02AAE5

    .noMosaic1

    ; Check if the area we are going to needs a mosaic.
    LDX.b $8A
    LDA.w Pool_MosaicTable, X

    BEQ .noMosaic2
        PLB
        JML $02AAE5

    .noMosaic2

    PLB
    JML $02AAF4
}
pushpc

; ==============================================================================

; Repairs an old ZS call.
org $02ABB8 ; $012BB8
db $A9, $09, $80, $02

if !Func02ABBE = 1

org $02ABBE ; $012BBE
    JSL NewOverworld_FinishTransGfx
    NOP : NOP : NOP

warnpc $02ABC5 ; $012BC5

else

; Undo the function above:
org $02ABBE ; $012BBE
db $85, $17, $8D, $10, $07, $E6, $11

endif

pullpc
; Loads the animated tiles after most of the transition gfx changes take place.
NewOverworld_FinishTransGfx:
{
    PHB : PHK : PLB
    
    ; First frame
    LDA.w TransGFXModuleIndex : BNE .notLoad
        JSR CheckForChangeGraphicsTransitionLoad

        ; Trigger NMI module: NMI_UpdateBgChrSlots_3_to_4.
        LDA.b #$09

        ; Signal for a graphics transfer in the NMI routine later.
        STA.b $17 : STA.w $0710

        ; Move on to next submodule.
        INC.b $11

        ; Move on to next subsubmodule.
        INC.w TransGFXModuleIndex

        BRA .return

    .notLoad

    ; Second frame
    CMP.b #$01 : BNE .notFinish
        ; Trigger NMI module: NMI_UpdateBgChrSlots_5_to_6.
        LDA.b #$0A

        ; Signal for a graphics transfer in the NMI routine later.
        STA.b $17 : STA.w $0710

        ; Don't move on to the next submodule yet.

        ; Move on to next subsubmodule.
        INC.w TransGFXModuleIndex

        BRA .return

    .notFinish

    ; Third frame
    CMP.b #$02 : BNE .notMain1
        LDA.w Pool_EnableTransitionGFXGroupLoad : BEQ .moveOn
            ; Prep the new static gfx tile sets.
            JSR LoadTransMainGFX
            JSR NewPrepTransAuxGFX

            ; Trigger NMI module: NMI_UpdateChr_Bg0.
            LDA.b #$0E

            ; Signal for a graphics transfer in the NMI routine later.
            STA.b $17 : STA.w $0710

            ; Move on to next subsubmodule.
            INC.w TransGFXModuleIndex

            BRA .return

    .notMain1

    ; Fourth frame
    LDA.w Pool_EnableTransitionGFXGroupLoad : BEQ .moveOn
        ; Trigger NMI module: NMI_DoNothing which we replaced with
        ; NMI_UpdateChr_Bg2HalfAndAnimated down below.
        LDA.b #$06

        ; Signal for a graphics transfer in the NMI routine later.
        STA.b $17 : STA.w $0710

    .moveOn

    ; Move on to next submodule.
    INC.b $11

    .return

    PLB

    RTL
}

CheckForChangeGraphicsTransitionLoad:
{
    ; Are we currently in a mosaic?
    LDA.b $11 : CMP.b #$0F : BEQ .mosaic
        ; Are we entering a special area?
        CMP.b #$1A : BEQ .mosaic
            ; Are we leaving a special area?
            CMP.b #$26 : BEQ .mosaic
                ; Just a normal transition, Not a mosaic.
                LDA.l Pool_EnableAnimated : BEQ .dontUpdateAnimated1
                    ; Check to see if we need to update the animated tiles
                    ; by checking what was previously loaded.
                    JSL ReadAnimatedTable : CMP.w AnimatedTileGFXSet : BEQ .dontUpdateAnimated1
                        STA.w AnimatedTileGFXSet : DEC : TAY

                        ; This forces the game toupdate the animated tiles
                        ; when going from one area to another.
                        JSL DecompOwAnimatedTiles

                .dontUpdateAnimated1

                LDA.w Pool_EnableMainPalette : BEQ .dontUpdateMain1
                    ; Check to see if we need to update the main palette by
                    ; checking what was previously loaded.
                    LDX.b $8A
                    LDA.w Pool_MainPaletteTable, X : CMP.w $0AB3 : BEQ .dontUpdateMain1
                        STA.w $0AB3

                        ; Run the modified routine that loads the buffer
                        ; and normal color ram.
                        JSL Palette_OverworldBgMain2

                .dontUpdateMain1

                LDA.w Pool_EnableBGColor : BEQ .dontUpdateBGColor1
                    REP #$30 ; Set A, X, and Y in 16bit mode.

                    LDA.b $8A : ASL : TAX ; Get area code and times it by 2.

                    ; Where ZS saves the array of palettes
                    LDA.w Pool_BGColorTable, X
                    STA.l $7EC300 : STA.l $7EC500
                    STA.l $7EC540 : STA.l $7EC340

                    SEP #$30 ; Set A, X, and Y in 8bit mode.

                    ; Don't update the CRAM until later when the overlays are
                    ; loaded so that way the BG overlays have a chance to hide
                    ; the cracks.
                    ;INC.b $15
                
                .dontUpdateBGColor1

                RTS

    .mosaic

    ; Check to see if we need to update the animated tiles by checking what
    ; was previously loaded.
    JSL ReadAnimatedTable : CMP.w AnimatedTileGFXSet : BEQ .dontUpdateAnimated2
        STA.w AnimatedTileGFXSet : DEC : TAY

        ; This forces the game to update the animated tiles when going
        ; from one area to another.
        JSL DecompOwAnimatedTiles

    .dontUpdateAnimated2

    ; Check to see if we need to update the main palette by checking
    ; what was previously loaded.
    LDX.b $8A
    LDA.w Pool_MainPaletteTable, X : CMP.w $0AB3 : BEQ .dontUpdateMain2
        STA.w $0AB3

        ; Run the vanilla routine that only loads the buffer.
        JSL Palette_OverworldBgMain

    .dontUpdateMain2

    REP #$30 ; Set A, X, and Y in 16bit mode.

    ; $0181 is the exit room number used for getting into the under the bridge
    ; area.
    LDA.b $A0 : CMP.w #$0181 : BNE .notBridge
        LDA.w Pool_BGColorTable_Bridge

        BRA .storeColor

    .notBridge

    LDA.b $8A : ASL : TAX ; Get area code and times it by 2.

    LDA.w Pool_BGColorTable, X ; Where ZS saves the array of palettes.

    .storeColor

    ; Set transparent color. only set the buffer so it fades in right
    ; during mosaic transition.
    STA.l $7EC300 : STA.l $7EC340

    LDX.w #$4020 : STX.b $9C
    LDX.w #$8040 : STX.b $9D
        
    LDX.w #$4F33
    LDY.w #$894F
        
    ; Change the fixed color depending on our sub screen overlay.
    ; Lost woods and skull woods.
    JSL ReadOverlayArray : CMP.w #$009D : BEQ .noSpecialColor
        CMP.w #$0040 : BEQ .noSpecialColor
            ; Pyramid area.
            CMP.w #$0096 : BEQ .specialColor
                LDX.w #$4C26
                LDY.w #$8C4C
                    
                ; LW death mountain.
                CMP.w #$0095 : BEQ .specialColor
                    LDX.w #$4A26
                    LDY.w #$874A
                        
                    ; DW death mountain.
                    CMP.w #$009C : BEQ .specialColor
                        BRA .noSpecialColor
                
            .specialColor

            STX.b $9C
            STY.b $9D
        
    .noSpecialColor
    
    SEP #$30 ; Set A, X, and Y in 8bit mode.

    ; Don't update the CRAM until later when the overlays are loaded so
    ; that way the BG overlays have a chance to hide the cracks.
    ;INC.b $15

    ; PLACE CUSTOM GFX LOAD HERE!
    ;JML CheckForChangeGraphicsTransitionLoadCastle

    CheckForChangeGraphicsTransitionLoadRetrun:

    RTS

    SkipOverworld_FinishTransGfx_firstHalf:

    ; Move on to next submodule.
    INC.b $11

    RTS
}

; The following 2 functions are copied from the palettes.asm but they only
; copied colors into the buffer so these copy colors into the normal ram as
; well.
Palette_OverworldBgMain2:
{
    REP #$21
        
    LDA.w $0AB3 : ASL A : TAX
        
    LDA.l $1BEC3B, X : ADC.w #$E6C8 : STA.b $00
        
    REP #$10
        
    ; Target BP2 through BP6 (first halves).
    ; Each palette has 7 colors.
    ; Load 5 palettes.
    LDA.w #$0042
    LDX.w #$0006
    LDY.w #$0004
        
    JSR.w Palette_MultiLoad2
        
    SEP #$30
        
    RTL
}

Palette_MultiLoad2:
{
    ; Description: Generally used to load multiple palettes for BGs.
    ; Upon close inspection, one sees that this algorithm is almost the same as
    ; the last subroutine.
    ; Name = Palette_MultiLoad(A, X, Y).
        
    ; Parameters: X = (number of colors in the palette - 1).
    ;             A = offset to add to $7EC300, in other words, where to write
    ;                 in palette memory.
    ;             Y = (number of palettes to load - 1).
        
    STA.b $04 ; Save the values for future reference.
    STX.b $06
    STY.b $08
        
    ; The absolute address at $00 was planted in the calling function. This
    ; value is the bank #$1B ( => D in Rom) The address is found from $0AB6 and
    ; of course, store it at $02.
    LDA.w #$001B : STA.b $02         
    
    .nextPalette
        ; $0AA8 + A parameter will be the X value.
        LDA.w $0AA8 : CLC : ADC.b $04 : TAX
        
        LDY.b $06 ; Tell me how long the palette is.
    
        .copyColors
            ; We're loading A from the address set up in the calling function.
            LDA.b [$00] : STA.l $7EC300, X : STA.l $7EC500, X 
            
            ; Increment the absolute portion of the address by two, and
            ; decrease the color count by one.
            INC.b $00 : INC.b $00
            
            INX #2
        
        ; So basically loop (Y+1) times, taking (Y * 2 bytes) to $7EC300, X.
        DEY : BPL .copyColors
        
        ; This technique bumps us up to the next 4bpp (16 color) palette.
        LDA.b $04 : CLC : ADC.w #$0020 : STA.b $04
        
        ; Decrease the number of palettes we have to load.
        DEC.b $08
        
    BPL .nextPalette
        
    ; We're done loading palettes.
        
    RTS
}

LoadTransMainGFX:
{
    ; Setup the decompression buffer address.
    ; $00[3] = $7E6000
    STZ.b $00
    LDA.b #$40 : STA.b $01
    LDA.b #$7E : STA.b $02

    REP #$30
    ; $0E = $8A * 8 
    LDA.b $8A : AND.w #$00FF : ASL #3 : STA.b $0E
    SEP #$20

    ; Sheet 0 (static 0)
    LDX.b $0E
    LDA.w Pool_OWGFXGroupTable_sheet0, X : CMP.b #$FF : BEQ .noBgGfxChange0
        SEP #$10
        
        TAY
        
        JSL Decomp_bg_variableLONG

    .noBgGfxChange0

    SEP #$10
    ; Increment buffer address by 0x0600.
    LDA.b $01 : CLC : ADC.b #$06 : STA.b $01
    REP #$10
    
    ; Sheet 1 (static 1)
    LDX.b $0E
    LDA.w Pool_OWGFXGroupTable_sheet1, X : CMP.b #$FF : BEQ .noBgGfxChange1 
        SEP #$10
        
        TAY
        
        JSL Decomp_bg_variableLONG

    .noBgGfxChange1

    SEP #$10
    ; Increment buffer address by 0x0600.
    LDA.b $01 : CLC : ADC.b #$06 : STA.b $01
    REP #$10

    ; Sheet 2 (static 2)
    LDX.b $0E
    LDA.w Pool_OWGFXGroupTable_sheet2, X : CMP.b #$FF : BEQ .noBgGfxChange2
        SEP #$10
        
        TAY
        
        JSL Decomp_bg_variableLONG

    .noBgGfxChange2

    SEP #$10
    ; Increment buffer address by 0x0600.
    LDA.b $01 : CLC : ADC.b #$06 : STA.b $01
    REP #$10

    ; Sheet 7 (animated)
    LDX.b $0E
    LDA.w Pool_OWGFXGroupTable_sheet7, X : CMP.b #$FF : BEQ .noBgGfxChange7
        SEP #$10
        
        TAY
        
        JSL Decomp_bg_variableLONG

    .noBgGfxChange7

    RTS
}

NewPrepTransAuxGFX:
{
    ; Prepares the transition graphics to be transferred to VRAM during NMI.
    ; This could occur either during this frame or any subsequent frame.
    
    ; Set bank for source address.
    LDA.b #$7E : STA.b $02 : STA.b $05
    
    REP #$31
    
    ; Source address is $7E6000, number of tiles is 0x40,
    ; base address is $7F0000.
    LDX.w #$0000
    LDY.w #$0040
    LDA.w #$4000
    
    ; The first graphics pack always uses the higher 8 palette values.
    JSL Do3To4High16BitLONG

    ; Number of tiles for next set is 0xC0.
    LDY.w #$00C0

    LDA.b $03
    
    JSL Do3To4Low16BitLONG

    SEP #$30

    RTS
}
pushpc

; ==============================================================================

if !Func0ABC5A = 1

org $0ABC5A ; $053C5A
    JSL CheckForChangeGraphicsNormalLoad

warnpc $0ABC5E ; $053C5E

else

; Undo the function above:
org $0ABC5A ; $053C5A
db $22, $9B, $E1, $00

endif

; Loads the animated tiles after the overworld map is closed.
pullpc
CheckForChangeGraphicsNormalLoad:
{
    PHB : PHK : PLB

    JSL InitTilesets ; Replaced code.

    JSL ReadAnimatedTable : STA.w AnimatedTileGFXSet : DEC : TAY

    ; This function is not needed here and is handled somewhere else. This
    ; forces the game to update the animated tiles when going from one area to
    ; another.
    ;JSL DecompOwAnimatedTiles 

    ; PLACE CUSTOM GFX LOAD HERE!
    ;JSL CheckForChangeGraphicsNormalLoadCastle
        
    PLB

    RTL
}
pushpc

; ==============================================================================

if !Func0AB8F5 = 1

; Loads different animated tiles when returning from bird travel.
org $0AB8F5 ; $0538F5
Func0AB8F5:
{
    JSL ReadAnimatedTable : STA.w AnimatedTileGFXSet : DEC : TAY
    
    ; From this point on it is the vanilla function.
    JSL DecompOwAnimatedTiles
    JSL Overworld_SetFixedColorAndScroll
        
    STZ.w $0AA9
    STZ.w $0AB2
        
    JSL InitTilesets
        
    INC.w $0200 ; SCAWFUL: Verify the interface submodule ID being used here.
    ; Provides context on where in the jump table we're at.
        
    STZ.b $B2
        
    JSL $02B1F4 ; $0131F4 IN ROM
        
    ; Play sound effect indicating we're coming out of map mode.
    LDA.b #$10 : STA.w $012F

    JSL LoadAmbientSound
        
    ; If it's a different music track than was playing where we came from,
    ; simply change to it (as opposed to setting volume back to full).
    LDA.l $7F5B00, X : AND.b #$0F : TAX : CPX.w $0130 : BNE .different_music
        ; Otherwise, just set the volume back to full.
        LDX.b #$F3
    
    .different_music
    
    STX.w $012C

    ; PLACE CUSTOM GFX LOAD HERE!
    ;JSL CheckForChangeGraphicsNormalLoadCastle
        
    RTL
}
warnpc $0AB948 ; $053948

else

; Undo the function above:
org $0AB8F5 ; $0538F5
db $A0, $58, $A5, $8A, $29, $BF, $C9, $03
db $F0, $0A, $C9, $05, $F0, $06, $C9, $07
db $F0, $02, $A0, $5A, $22, $94, $D3, $00
db $22, $70, $FE, $0B, $9C, $A9, $0A, $9C
db $B2, $0A, $22, $9B, $E1, $00, $EE, $00
db $02, $64, $B2, $22, $F4, $B1, $02, $A9
db $10, $8D, $2F, $01, $A6, $8A, $BF, $00
db $5B, $7F, $4A, $4A, $4A, $4A, $8D, $2D
db $01, $BF, $00, $5B, $7F, $29, $0F, $AA
db $EC, $30, $01, $D0, $02, $A2, $F3, $8E
db $2C, $01, $6B

endif

pullpc
LoadAmbientSound:
{
    PHB : PHK : PLB

    ; Reset the ambient sound effect to what it was.
    LDX.b $8A : LDA.l $7F5B00, X : LSR #4 : STA.w $012D

    ; Check if we need to stop the rain sound in the misery mire.
    LDA.w Pool_EnableRainMireEvent : BEQ .disableRainSound
        LDA.b $8A : CMP.b #$70 : BNE .disableRainSound
            ; Has Misery Mire been triggered yet?
            LDA.l $7EF2F0 : AND.b #$20 : BNE .disableRainSound
                LDA.b #$01 : STA.w $012D
        
    .disableRainSound

    PLB

    RTL
}
pushpc

; ==============================================================================

if !Func0BFEC6 = 1

; Loads different special transparent colors and overlay speeds based on the
; overlay duringtransition and under other certain cases. Exact cases need to be
; investigated. When leaving dungeon.
org $0BFEC6 ; $05FEC6
Overworld_LoadBGColorAndSubscreenOverlay:
{
    JSL ReplaceBGColor

    ; Set fixed color to neutral.
    LDA.w #$4020 : STA.b $9C
    LDA.w #$8040 : STA.b $9D

    ; Check if we need to load the rain in the misery mire.
    LDA.l Pool_EnableRainMireEvent : BEQ .notMire
        LDA.b $8A : CMP.w #$0070 : BNE .notMire
            ; Has Misery Mire been triggered yet?
            LDA.l $7EF2F0 : AND.w #$0020 : BNE .notMire
                JMP .subscreenOnAndReturn
        
    .notMire

    JSL ReadOverlayArray

    ; Check for misery mire.
    CMP.w #$009F : BNE .notRain
        JMP .subscreenOnAndReturn
    
    .notRain
    
    ; Change the fixed color depending on our sub screen overlay.
    ; Check for lost woods?, skull woods, and pyramid area.
    CMP.w #$009D : BEQ .noCustomFixedColor
    CMP.w #$0096 : BEQ .noCustomFixedColor
        LDX.w #$4C26
        LDY.w #$8C4C
        
        ; Check for LW Death mountain.
        CMP.w #$0095 : BEQ .setCustomFixedColor
            LDX.w #$4A26
            LDY.w #$874A
            
            ; Check for DW Death mountain. (not turtle rock?).
            CMP.w #$009C : BEQ .setCustomFixedColor
                SEP #$30 ; Set A, X, and Y in 8bit mode.
                
                ; Update CGRAM this frame.
                INC.b $15
                
                RTL
        
        .setCustomFixedColor
        
        STX.b $9C
        STY.b $9D ; Set the fixed color addition color values.
    
    .noCustomFixedColor
    
    LDA.b $11 : AND.w #$00FF : CMP.w #$0004 : BEQ .BRANCH_11
        ; Make sure BG2 and BG1 Y scroll values are synchronized.
        ; Same for X scroll.
        LDA.b $E8 : STA.b $E6
        LDA.b $E2 : STA.b $E0
            
        ; Just because I need a bit more space.
        JSL ReadOverlayArray
            
        ; Are we at Hyrule Castle or Pyramid of Power?
        CMP.w #$0096 : BNE .subscreenOnAndReturn
            JSL NeedSomeSpaceForWhateverThisIs
                
            BRA .subscreenOnAndReturn
    
    .BRANCH_11
    
    ; Check for the pyramid BG.
    JSL ReadOverlayArray : CMP.w #$0096 : BNE .subscreenOnAndReturn
        ; Synchronize Y scrolls on BG0 and BG1. Same for X scrolls.
        LDA.b $E8 : STA.b $E6
        LDA.b $E2 : STA.b $E0
            
        LDA.w $0410 : AND.w #$00FF : CMP.w #$0008 : BEQ .BRANCH_12
            ; Handles scroll for special areas maybe?
            LDA.w #$0838 : STA.b $E0
        
        .BRANCH_12
        
        LDA.w #$06C0 : STA.b $E6
    
    .subscreenOnAndReturn
    
    SEP #$30 ; Set A, X, and Y in 8bit mode.
        
    ; Put BG0 on the subscreen.
    LDA.b #$01 : STA.b $1D
        
    ; Update palette.
    INC.b $15
        
    RTL
}
warnpc $0BFFA8 ; $05FFA8

else

; Undo the function above:
org $0BFEC6 ; $05FEC6
db $A9, $20, $40, $85, $9C, $A9, $40, $80
db $85, $9D, $A5, $8A, $F0, $40, $C9, $70
db $00, $D0, $03, $4C, $9D, $FF, $C9, $40
db $00, $F0, $33, $C9, $5B, $00, $F0, $2E
db $A2, $26, $4C, $A0, $4C, $8C, $C9, $03
db $00, $F0, $1F, $C9, $05, $00, $F0, $1A
db $C9, $07, $00, $F0, $15, $A2, $26, $4A
db $A0, $4A, $87, $C9, $43, $00, $F0, $0A
db $C9, $45, $00, $F0, $05, $E2, $30, $E6
db $15, $6B, $86, $9C, $84, $9D, $A5, $11
db $29, $FF, $00, $C9, $04, $00, $F0, $58
db $A5, $E8, $85, $E6, $A5, $E2, $85, $E0
db $A5, $8A, $29, $3F, $00, $C9, $1B, $00
db $D0, $6D, $A5, $E2, $38, $E9, $78, $07
db $4A, $A8, $29, $00, $40, $F0, $05, $98
db $09, $00, $80, $A8, $84, $00, $A5, $E2
db $38, $E5, $00, $85, $E0, $A5, $E6, $C9
db $C0, $06, $90, $17, $38, $E9, $00, $06
db $29, $FF, $03, $C9, $80, $01, $B0, $06
db $4A, $09, $00, $06, $80, $0E, $A9, $C0
db $06, $80, $09, $A5, $E6, $29, $FF, $00
db $4A, $09, $00, $06, $85, $E6, $80, $27
db $A5, $8A, $29, $3F, $00, $C9, $1B, $00
db $D0, $1D, $A5, $E8, $85, $E6, $A5, $E2
db $85, $E0, $AD, $10, $04, $29, $FF, $00
db $C9, $08, $00, $F0, $05, $A9, $38, $08
db $85, $E0, $A9, $C0, $06, $85, $E6, $E2
db $20, $A9, $01, $85, $1D, $E2, $30, $E6
db $15, $6B

endif

pullpc
ReplaceBGColor:
{
    PHB : PHK : PLB

    ; TODO: This may need to check if we are in a warp and then if so load the
    ; custom color. If not, then chceck if its enabled or not.
    ;SEP #$20 ; Set A in 8bit mode.

    ;LDA.w Pool_EnableBGColor : BNE .custom
        ;REP #$20 ; Set A in 16bit mode.

        ;PLB

        ;RTL

    ;.custom

    ;REP #$20 ; Set A in 16bit mode.

    LDA.b $8A : ASL : TAX ; Get area code and times it by 2.
    LDA.w Pool_BGColorTable, X ; Get the color.

    ;STA.l $7EC300 : STA.l $7EC340 ; Set the BG color.
    STA.l $7EC500 : STA.l $7EC540

    PLB

    RTL
}

; TODO: Doccument this better and fiture out what it actually does.
NeedSomeSpaceForWhateverThisIs:
{
    LDA.b $E2 : SEC : SBC.w #$0778 : LSR A : TAY : AND.w #$4000 : BEQ .BRANCH_7
        TYA : ORA.w #$8000 : TAY
            
    .BRANCH_7
            
    STY.b $00
                
    LDA.b $E2 : SEC : SBC.b $00 : STA.b $E0
                
    LDA.b $E6 : CMP.w #$06C0 : BCC .BRANCH_9
        SEC : SBC.w #$0600 : AND.w #$03FF : CMP.w #$0180 : BCS .BRANCH_8
            LSR A : ORA.w #$0600
                
                BRA .BRANCH_10
            
            .BRANCH_8
            
            LDA.w #$06C0
                
            BRA .BRANCH_10
            
    .BRANCH_9

    LDA.b $E6 : AND.w #$00FF : LSR A : ORA.w #$0600
            
    .BRANCH_10
            
    ; Set BG1 vertical scroll.
    STA.b $E6

    RTL
}
pushpc

; ==============================================================================

if !Func0ED627 = 1

; Loads the transparent color under some load conditions such as the mirror\
; warp.
; TODO: Investigate the other conditions. Exiting dungeons.
org $0ED627 ; $075627
    JML InitColorLoad2
    NOP

warnpc $0ED62C ; $07562C

else 

; Undo the function above:
org $0ED627 ; $075627
db $A5, $8A, $C9, $80, $00

endif

org $0ED652
InitColorLoad2_Return:

pullpc
InitColorLoad2:
{
    PHB : PHK : PLB

    ; $0181 is the exit room number used for getting into the under the bridge
    ; area.
    LDA.b $A0 : CMP.w #$0181 : BNE .notBridge
        LDA.w Pool_BGColorTable_Bridge

        BRA .storeColor

    .notBridge

    LDA.b $8A : ASL : TAX ; Get area code and times it by 2.
    LDA.w Pool_BGColorTable, X ; Get the color.

    .storeColor

    STA.l $7EC300 : STA.l $7EC340 ; Set transparent color.
    ;STA.l $7EC500 : STA.l $7EC540

    INC.b $15

    PLB

    JML InitColorLoad2_Return
}
pushpc

; ==============================================================================

if !Func0ED8AE = 1

; Resets the area special color after the screen flashes.
org $0ED8AE ; $0758AE
Func0ED8AE:
{
    LDA.b $1B : BNE .noSpecialColor
        REP #$30 ; Set A, X, and Y in 16bit mode.

        LDX.w #$4020 : STX.b $9C
        LDX.w #$8040 : STX.b $9D
        
        LDX.w #$4F33
        LDY.w #$894F
        
        ; Change the fixed color depending on our sub screen overlay.
        ; Lost woods and skull woods.
        JSL ReadOverlayArray : CMP.w #$009D : BEQ .noSpecialColor
            CMP.w #$0040 : BEQ .noSpecialColor
                ; Pyramid area.
                CMP.w #$0096 : BEQ .specialColor
                    LDX.w #$4C26
                    LDY.w #$8C4C
                    
                    ; LW death mountain.
                    CMP.w #$0095 : BEQ .specialColor
                        LDX.w #$4A26
                        LDY.w #$874A
                        
                        ; DW death mountain.
                        CMP.w #$009C : BEQ .specialColor
                            BRA .noSpecialColor
                
                .specialColor

                STX.b $9C
                STY.b $9D
        
        .noSpecialColor
    
    SEP #$30 ; Set A, X, and Y in 8bit mode.
        
    RTL
}
warnpc $0ED8FB ; $0758FB

else

; Undo the function above:
org $0ED8AE ; $0758AE
db $A5, $1B, $D0, $46, $C2, $10, $A2, $20
db $40, $86, $9C, $A2, $40, $80, $86, $9D
db $A2, $33, $4F, $A0, $4F, $89, $A5, $8A
db $F0, $30, $C9, $40, $F0, $2C, $C9, $5B
db $F0, $24, $A2, $26, $4C, $A0, $4C, $8C
db $C9, $03, $F0, $1A, $C9, $05, $F0, $16
db $C9, $07, $F0, $12, $A2, $26, $4A, $A0
db $4A, $87, $C9, $43, $F0, $08, $C9, $45
db $F0, $04, $C9, $47, $D0, $04, $86, $9C
db $84, $9D, $E2, $10, $6B

endif

; ==============================================================================

if !Func00E133 = 1

; Interupts the vanilla LoadTransAuxGFX function
org $00D673 ; $005673
    JML NewLoadTransAuxGFX

warnpc $00D677 ; $005677

org $008C8A ; $000C8A
dw NMI_UpdateChr_Bg2HalfAndAnimated

warnpc $00D677 ; $005677

; Replaces the UNREACHABLE_00D585 which is unused.
org $00D585 ; $005585
Decomp_bg_variableLONG:
{
    PHB : PHK : PLB

    JSR Decomp_bg_variable

    PLB

    RTL
}

Do3To4Low16BitLONG:
{
    PHB : PHK : PLB

    JSR.w Do3To4Low16Bit

    PLB

    RTL
}

Do3To4High16BitLONG:
{
    PHB : PHK : PLB

    JSR.w Do3To4High16Bit

    PLB

    RTL
}

NMI_UpdateChr_Bg2HalfAndAnimated:
{
    JSL NMI_UpdateChr_Bg2HalfAndAnimatedLONG
    
    RTS
}

warnpc $00D5CB ; $0055CB

else

; Undo the functions above:
org $00D673 ; $005673
db $A9, $60, $85, $01

org $008C8A ; $000C8A
db $4B, $8E

; This is the old 0AA1 table. Will eventually be unused so I'll leave this here
; for future use.
org $00E073 ; $006073
db $00, $01, $10, $06, $0E, $1F, $18, $0F
db $00, $01, $10, $08, $0E, $22, $1B, $0F
db $00, $01, $10, $06, $0E, $1F, $18, $0F
db $00, $01, $13, $07, $0E, $23, $1C, $0F
db $00, $01, $10, $07, $0E, $21, $18, $0F
db $00, $01, $10, $09, $0E, $20, $19, $0F
db $02, $03, $12, $0B, $0E, $21, $1A, $0F
db $00, $01, $11, $0C, $0E, $24, $1B, $0F
db $00, $01, $11, $08, $0E, $22, $1B, $0F
db $00, $01, $11, $0C, $0E, $25, $1A, $0F
db $00, $01, $11, $0C, $0E, $26, $1B, $0F
db $00, $01, $14, $0A, $0E, $27, $1D, $0F
db $00, $01, $11, $0A, $0E, $28, $1E, $0F
db $02, $03, $12, $0B, $0E, $29, $16, $0F
db $00, $01, $15, $0D, $0E, $2A, $18, $0F
db $00, $01, $10, $07, $0E, $23, $1C, $0F
db $00, $01, $13, $07, $0E, $04, $05, $0F
db $00, $01, $13, $07, $0E, $04, $05, $0F
db $00, $01, $10, $09, $0E, $20, $1B, $0F
db $00, $01, $10, $09, $0E, $2A, $17, $0F
db $02, $03, $12, $0B, $0E, $21, $1C, $0F
db $00, $08, $11, $1B, $22, $2E, $5D, $5B
db $00, $08, $10, $18, $20, $2B, $5D, $5B
db $00, $08, $10, $18, $20, $2B, $5D, $5B
db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B
db $42, $43, $44, $45, $20, $2B, $3F, $5D
db $00, $08, $10, $18, $20, $2B, $5D, $5B
db $00, $08, $10, $18, $20, $2B, $5D, $5B
db $00, $08, $10, $18, $20, $2B, $5D, $5B
db $00, $08, $10, $18, $20, $2B, $5D, $5B
db $00, $08, $10, $18, $20, $2B, $5D, $5B
db $71, $72, $71, $72, $20, $2B, $5D, $5B
db $3A, $3B, $3C, $3D, $53, $4D, $3E, $5B
db $42, $43, $44, $45, $20, $2B, $3F, $59
db $00, $72, $71, $72, $20, $2B, $5D, $0F
db $16, $39, $1D, $17, $40, $41, $39, $1E
db $00, $46, $39, $72, $40, $41, $39, $0F

org $00D585 ; $005585
db $A0, $08, $00, $84, $0E, $85, $00, $18
db $69, $10, $00, $85, $03, $A0, $07, $00
db $A7, $00, $9F, $00, $90, $7E, $E6, $00
db $E6, $00, $A7, $03, $29, $FF, $00, $9F
db $10, $90, $7E, $E6, $03, $E8, $E8, $88
db $10, $E6, $8A, $18, $69, $10, $00, $AA
db $A5, $03, $29, $78, $00, $D0, $08, $A5
db $03, $18, $69, $80, $01, $85, $03, $A5
db $03, $C6, $0E, $D0, $C0, $60

endif

pullpc

NewLoadTransAuxGFX:
{
    PHB : PHK : PLB

    LDA.w Pool_EnableTransitionGFXGroupLoad : BNE .notNormalLoad
        ; Replaced code:
        LDA.b #$60 : STA.b $01

        PLB

        JML $00D677 ; $005677 Return to regular code.

    .notNormalLoad

    ; Setup the decompression buffer address.
    ; $00[3] = $7E6000
    STZ.b $00
    LDA.b #$60 : STA.b $01
    LDA.b #$7E : STA.b $02

    REP #$30
    ; $0E = $8A * 8 
    LDA.b $8A : AND.w #$00FF : ASL #3 : STA.b $0E
    SEP #$20

    ; Sheet 3 (variable 0)
    LDX.b $0E
    LDA.w Pool_OWGFXGroupTable_sheet3, X : CMP.b #$FF : BEQ .noBgGfxChange3
        SEP #$10
        
        TAY
        
        JSL Decomp_bg_variableLONG

    .noBgGfxChange3

    SEP #$10
    ; Increment buffer address by 0x0600.
    LDA.b $01 : CLC : ADC.b #$06 : STA.b $01
    REP #$10

    ; Sheet 4 (variable 1)
    LDX.b $0E
    LDA.w Pool_OWGFXGroupTable_sheet4, X : CMP.b #$FF : BEQ .noBgGfxChange4
        SEP #$10
        
        TAY
        
        JSL Decomp_bg_variableLONG

    .noBgGfxChange4

    SEP #$10
    ; Increment buffer address by 0x0600.
    LDA.b $01 : CLC : ADC.b #$06 : STA.b $01
    REP #$10

    ; Sheet 5 (variable 2)
    LDX.b $0E
    LDA.w Pool_OWGFXGroupTable_sheet5, X : CMP.b #$FF : BEQ .noBgGfxChange5
        SEP #$10
        
        TAY
        
        JSL Decomp_bg_variableLONG

    .noBgGfxChange5

    SEP #$10
    ; Increment buffer address by 0x0600.
    LDA.b $01 : CLC : ADC.b #$06 : STA.b $01
    REP #$10

    ; Sheet 6 (variable 3)
    LDX.b $0E
    LDA.w Pool_OWGFXGroupTable_sheet6, X : CMP.b #$FF : BEQ .noBgGfxChange6
        SEP #$10
        
        TAY
        
        JSL Decomp_bg_variableLONG

    .noBgGfxChange6

    SEP #$10
    ; Increment buffer address by 0x0600.
    LDA.b $01 : CLC : ADC.b #$06 : STA.b $01
    REP #$10

    STZ.w TransGFXModuleIndex

    PLB

    JML LoadTransAuxGFX_sprite_continue ; $005706 Return to regular code.
}

NMI_UpdateChr_Bg2HalfAndAnimatedLONG:
{
    PHB : PHK : PLB

    REP #$20
    
    ; Sheet 1
    ; Set VRAM target to $3000 (word).
    LDA.w #$2800 : STA.w $2116
    
    ; Increment on writes to $2119.
    LDY.b #$80 : STY.w $2115
    
    ; Target is $2118, write two registers once ($2118 / $2119).
    LDA.w #$1801 : STA.w $4300
    
    ; Source address is $7F1000.
    LDA.w #$1000 : STA.w $4302
    LDY.b #$7F   : STY.w $4304
    
    ; Write 0x0800 bytes.
    LDA.w #$0800 : STA.w $4305
    
    ; Transfer data on channel 1.
    LDY.b #$01 : STY.w $420B

    ; Sheet 2
    ; Set VRAM target to $3000 (word).
    LDA.w #$3E00 : STA.w $2116
    
    ; Increment on writes to $2119.
    LDY.b #$80 : STY.w $2115
    
    ; Target is $2118, write two registers once ($2118 / $2119).
    LDA.w #$1801 : STA.w $4300
    
    ; Only copy the latter half of the sheet to prevent the animated tiles from
    ; flickering on transition.
    ; Source address is $7F1C00.
    LDA.w #$1C00 : STA.w $4302
    LDY.b #$7F   : STY.w $4304
    
    ; Write 0x08400 bytes.
    LDA.w #$0400 : STA.w $4305
    
    ; Transfer data on channel 1.
    LDY.b #$01 : STY.w $420B
    
    SEP #$20
    
    STZ.w $0710

    PLB

    RTL
}
pushpc

; ==============================================================================

if !Func00E221 = 1

org $00E221 ; $006221
    JML InitTilesetsLongCalls

warnpc $00E225 ; $006225

org $00D904 ; $005904
    JML AnimateMirrorWarp_DecompressNewTileSetsLongCalls

warnpc $00D908 ; $005908

org $00D97D ; $00597D
    JML AnimateMirrorWarp_DecompressNewTileSetsLongCalls2

warnpc $00D981 ; $005981

org $00D9BC ; $0059BC
    JML AnimateMirrorWarp_DecompressBackgroundsALongCalls

warnpc $00D9C1 ; $0059C1

org $00DA2F ; $005A2F
    JML AnimateMirrorWarp_DecompressBackgroundsCLongCalls

else

; Undo the functions above:
org $00E221 ; $006221
db $AD, $A1, $0A, $29

org $00D904 ; $005904
db $AD, $A1, $0A, $29

org $00D97D ; $00597D
db $BF, $EF, $D8, $00

org $00D9BC ; $0059BC
db $BF, $F1, $D8, $00

org $00DA2F ; $005A2F
db $BF, $F3, $D8, $00

endif

pullpc
InitTilesetsLongCalls:
{
    PHB : PHK : PLB

    SEP #$20
    ; TODO: This will eventually be changed when changing the dungeon GFX.
    ; Only trigger the new code when in the:
    LDA.b $10 : CMP.b #$08 : BEQ .outdoors ; Pre-overworld main module
                CMP.b #$0E : BEQ .outdoors ; Text Mode/Item Screen/Map module
        REP #$30
        LDA.w $0AA1 : AND.w #$00FF ; Replaced code.

        PLB

        JML $00E227 ; $006227 Return to normal code.

    .outdoors

    REP #$30
    LDA.b $8A : AND.w #$00FF : ASL #3 : TAX
    LDA.b $8A : AND.w #$00C0 : LSR #3 : TAY ; (Area / 8) = LW, DW, or SW *8
    SEP #$20
    
    LDA.w Pool_OWGFXGroupTable_sheet0, X : CMP.b #$FF : BNE .notFF0
        LDA.w Pool_DefaultGFXGroups_sheet0, Y

    .notFF0
    
    STA.b $0D

    LDA.w Pool_OWGFXGroupTable_sheet1, X : CMP.b #$FF : BNE .notFF1
        LDA.w Pool_DefaultGFXGroups_sheet1, Y

    .notFF1
    
    STA.b $0C

    LDA.w Pool_OWGFXGroupTable_sheet2, X : CMP.b #$FF : BNE .notFF2
        LDA.w Pool_DefaultGFXGroups_sheet2, Y

    .notFF2

    STA.b $0B
    
    LDA.w Pool_OWGFXGroupTable_sheet3, X : CMP.b #$FF : BNE .notFF3
        LDA.w Pool_DefaultGFXGroups_sheet3, Y

    .notFF3
    
    STA.l $7EC2F8 : STA.b $0A

    LDA.w Pool_OWGFXGroupTable_sheet4, X : CMP.b #$FF : BNE .notFF4
        LDA.w Pool_DefaultGFXGroups_sheet4, Y

    .notFF4
    
    STA.l $7EC2F9 : STA.b $09

    LDA.w Pool_OWGFXGroupTable_sheet5, X : CMP.b #$FF : BNE .notFF5
        LDA.w Pool_DefaultGFXGroups_sheet5, Y

    .notFF5
    
    STA.l $7EC2FA : STA.b $08

    LDA.w Pool_OWGFXGroupTable_sheet6, X : CMP.b #$FF : BNE .notFF6
        LDA.w Pool_DefaultGFXGroups_sheet6, Y

    .notFF6
    
    STA.l $7EC2FB : STA.b $07
    
    LDA.w Pool_OWGFXGroupTable_sheet7, X : CMP.b #$FF : BNE .notFF7
        LDA.w Pool_DefaultGFXGroups_sheet7, Y

    .notFF7
    
    STA.b $06

    PLB

    JML $00E282 ; $006282 Skip normal sheet load.
}

AnimateMirrorWarp_DecompressNewTileSetsLongCalls:
{
    PHB : PHK : PLB

    LDA.b $8A : AND.w #$00FF : ASL #3 : TAX
    LDA.b $8A : AND.w #$00C0 : LSR #3 : TAY ; (Area / 8) = LW, DW, or SW *8
    SEP #$20

    LDA.w Pool_OWGFXGroupTable_sheet3, X : CMP.b #$FF : BNE .notFF3
        LDA.w Pool_DefaultGFXGroups_sheet3, Y

    .notFF3

    STA.l $7EC2F8

    LDA.w Pool_OWGFXGroupTable_sheet4, X : CMP.b #$FF : BNE .notFF4
        LDA.w Pool_DefaultGFXGroups_sheet4, Y

    .notFF4

    STA.l $7EC2F9

    LDA.w Pool_OWGFXGroupTable_sheet5, X : CMP.b #$FF : BNE .notFF5
        LDA.w Pool_DefaultGFXGroups_sheet5, Y

    .notFF5

    STA.l $7EC2FA

    LDA.w Pool_OWGFXGroupTable_sheet6, X : CMP.b #$FF : BNE .notFF6
        LDA.w Pool_DefaultGFXGroups_sheet6, Y

    .notFF6

    STA.l $7EC2FB

    PLB

    JML $00D949 ; $005949 Skip normal sheet load.
}

AnimateMirrorWarp_DecompressNewTileSetsLongCalls2:
{
    PHB : PHK : PLB

    REP #$30
    LDA.b $8A : AND.w #$00FF : ASL #3 : TAX
    LDA.b $8A : AND.w #$00C0 : LSR #3 : TAY ; (Area / 8) = LW, DW, or SW *8
    SEP #$20

    LDA.w Pool_OWGFXGroupTable_sheet1, X : CMP.b #$FF : BNE .notFF1
        LDA.w Pool_DefaultGFXGroups_sheet1, Y

    .notFF1

    STA.b $08

    LDA.w Pool_OWGFXGroupTable_sheet0, X : CMP.b #$FF : BNE .notFF0
        LDA.w Pool_DefaultGFXGroups_sheet0, Y

    .notFF0

    TAY

    SEP #$10

    PLB

    JML $00D988 ; $005988 Skip normal sheet load.
}

AnimateMirrorWarp_DecompressBackgroundsALongCalls:
{
    PHB : PHK : PLB

    REP #$30
    LDA.b $8A : AND.w #$00FF : ASL #3 : TAX
    LDA.b $8A : AND.w #$00C0 : LSR #3 : TAY ; (Area / 8) = LW, DW, or SW *8
    SEP #$20

    LDA.w Pool_OWGFXGroupTable_sheet3, X : CMP.b #$FF : BNE .notFF3
        LDA.w Pool_DefaultGFXGroups_sheet3, Y

    .notFF3
    
    STA.b $08

    LDA.w Pool_OWGFXGroupTable_sheet2, X : CMP.b #$FF : BNE .notFF2
        LDA.w Pool_DefaultGFXGroups_sheet2, Y

    .notFF2
    
    TAY

    SEP #$10

    PLB

    JML $00D9C7 ; $0059C7 Skip normal sheet load.
}

AnimateMirrorWarp_DecompressBackgroundsCLongCalls:
{
    PHB : PHK : PLB

    REP #$30
    LDA.b $8A : AND.w #$00FF : ASL #3 : TAX
    LDA.b $8A : AND.w #$00C0 : LSR #3 : TAY ; (Area / 8) = LW, DW, or SW *8
    SEP #$20

    LDA.w Pool_OWGFXGroupTable_sheet7, X : CMP.b #$FF : BNE .notFF7
        LDA.w Pool_DefaultGFXGroups_sheet7, Y

    .notFF7

    STA.b $08 : STA AnimatedTileGFXSet

    LDA.w Pool_OWGFXGroupTable_sheet6, X : CMP.b #$FF : BNE .notFF6
        LDA.w Pool_DefaultGFXGroups_sheet6, Y

    .notFF6
    
    TAY

    SEP #$10

    PLB

    JML $00DA3A ; $005A3A Skip normal sheet load.
}
pushpc

; ==============================================================================

; A second pullpc is needed here just in case someone incorperates this ASM into
; their own code base.
pullpc
pullpc
