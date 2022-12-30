; 
; Credit to Zarby89 
; 
lorom

!End = $00
!Rest = $C9
!Tie = $C8

macro SetChannelVolume(v)
db $ED, <v>
endmacro

macro SetMasterVolume(v)
db $E5, <v>
endmacro

macro SetTempo(v)
db $E7, <v>
endmacro

macro SetInstrument(v)
db $E0, <v>
endmacro

macro CallSubroutine(addr, repeat)
db $EF
dw <addr>
db <repeat>
endmacro

;1/4 = $48
;1/4 double = $6C
;1/4 triplet = $30
;1/8 = $24
;1/8 double = $36
;1/8 triplet = $18
;1/16 = $12
;1/16 double = $1B
;1/32 = $09
; To make a whole note you tie 4 1/4 so something like
;%SetDuration(48)
;db !C4, !Tie, !Tie, !Tie ; will play a whole note (1/1)
;db !C4, !Tie ; will play a half note (1/2)

macro SetDuration(v)
db <v>, $7F
endmacro


!C1 = $80
!C1s = $81
!D1 = $82
!D1s = $83
!E1 = $84
!F1 = $85
!F1s = $86
!G1 = $87
!G1s = $88
!A1 = $89
!A1s = $8A
!B1 = $8B


!C2 = $8C
!C2s = $8D
!D2 = $8E
!D2s = $8F
!E2 = $90
!F2 = $91
!F2s = $92
!G2 = $93
!G2s = $94
!A2 = $95
!A2s = $96
!B2 = $97


!C3 = $98
!C3s = $99
!D3 = $9A
!D3s = $9B
!E3 = $9C
!F3 = $9D
!F3s = $9E
!G3 = $9F
!G3s = $A0
!A3 = $A1
!A3s = $A2
!B3 = $A3

!C4 = $A4
!C4s = $A5
!D4 = $A6
!D4s = $A7
!E4 = $A8
!F4 = $A9
!F4s = $AA
!G4 = $AB
!G4s = $AC
!A4 = $AD
!A4s = $AE
!B4 = $AF

!C5 = $B0
!C5s = $B1
!D5 = $B2
!D5s = $B3
!E5 = $B4
!F5 = $B5
!F5s = $B6
!G5 = $B7
!G5s = $B8
!A5 = $B9
!A5s = $BA
!B5 = $BB

!C6 = $BC
!C6s = $BD
!D6 = $BE
!D6s = $BF
!E6 = $C0
!F6 = $C1
!F6s = $C2
!G6 = $C3
!G6s = $C4
!A6 = $C5
!A6s = $C6
!B6 = $C7

org $1A9FF8 ; Hyrule Castle (Song Header information)
Sections: 
!ARAMAddr = $D0FF
!StartingAddr = Sections
dw !ARAMAddr+$0A
dw !ARAMAddr+$0A
dw $00FF
dw !ARAMAddr
dw $0000

Channels: 
!ARAMC = !ARAMAddr-Sections
dw Channel0+!ARAMC
dw $0000
dw $0000
dw $0000
dw $0000
dw $0000
dw $0000
dw $0000


Channel0:
SetMasterVolume($80)
SetTempo($40)
SetInstrument($17)

db !Rest, !Rest, !Rest

db !End