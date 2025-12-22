;#ENABLED=True
;#PATCH_NAME=Spike damage
;#PATCH_AUTHOR=kan
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Allows mail upgrades to reduce sprite damage
;$08 = 1 heart, $04 = half heart, $10 = 2 heart, etc...
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=Green Mail Damage
!GreenMailDamage = $08
;#name=Blue Mail Damage
!BlueMailDamage = $08
;#name=Red Mail Damage
!RedMailDamage = $08
;#DEFINE_END

pushpc
org $07BA07
	db !GreenMailDamage
	db !BlueMailDamage
	db !RedMailDamage
pullpc
