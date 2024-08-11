; =========================================================
; yaze custom assembly code 
; =========================================================

namespace yaze
{

!YAZE_CUSTOM_MOSAIC = 1

if !YAZE_CUSTOM_MOSAIC != 0
  incsrc "mosaic_change.asm"
endif

}
namespace off