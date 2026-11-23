# Shorter League cave  (port of NikoKV's TWEAK_BBSL_001 newmem code)
#
# Entered in place of:   mov [edx],al      (88 02)   -- then continues at site+2 (pop ebp ; ret)
# Falls through at the end; the loader appends  jmp site+2.
#   A_AREA   = word [FFX.exe+D2CA90]   current area id
#   A_LGFLAG = byte [FFX.exe+D2E10A]
#   A_ROUND  = ADDRESS FFX.exe+D2E102  (the round-index byte; compared against edx)
cave_league:
    cmp  word ptr [A_AREA], 0x015B
    jne  l_orig
    cmp  byte ptr [A_LGFLAG], 0
    jne  l_orig
    cmp  edx, offset A_ROUND
    jne  l_orig
    cmp  al, 5                              # round 5 reached ...
    jne  l_orig
    mov  byte ptr [edx], 0x0A               # ... jump to round 10 (one byte, as the original wrote)
    jmp  l_exit
l_orig:
    mov  [edx], al
l_exit:
cave_league_end:
