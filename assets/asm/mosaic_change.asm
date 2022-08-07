
org <HOOK>
  JML AreaCheck

org <EXPANDED_SPACE>

AreaCheck:
  PHB : PHK : PLB

  TAX
  LDA .pool, X

  BEQ .noMosaic1
      PLB
      JML $02AAE5

  .noMosaic1

  LDX $8A
  LDA .pool, X

  BEQ .noMosaic2
      PLB
      JML $02AAE5

  .noMosaic2

  PLB
  JML $02AAF4

  NOP
  .pool
