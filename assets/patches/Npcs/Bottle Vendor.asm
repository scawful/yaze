;#ENABLED=True
;#PATCH_NAME=Bottle Vendor
;#PATCH_AUTHOR=Zarby89
;#PATCH_VERSION=1.0
;#PATCH_DESCRIPTION
;Modify the item and price of sold by the Kakariko bottle vendor
;#ENDPATCH_DESCRIPTION

;#DEFINE_START
;#name=Item Price
;#type=word
;#decimal=true
!ItemPrice = 100

;#name=Item ID
;#type=item
!ItemID = $16
;#DEFINE_END

pushpc
org $05EAF9
	dw !BottlePrice

org $05EB34
	dw !BottlePrice

org $05EB18
	db !ItemID
pullpc
