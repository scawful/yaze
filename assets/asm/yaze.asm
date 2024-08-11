; =========================================================
; yaze custom assembly code 
; =========================================================

namespace yaze
{

!YAZE_CUSTOM_MOSAIC = 1


if !YAZE_CUSTOM_MOSAIC != 0
  incsrc "mosaic_change.asm"
endif

!ZS_CUSTOM_OVERWORLD = 1

if !ZS_CUSTOM_OVERWORLD != 0
  incsrc "ZSCustomOverworld.asm"
endif

}
namespace off