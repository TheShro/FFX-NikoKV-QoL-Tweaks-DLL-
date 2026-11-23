# Blitzball auto-learn fix  (port of NikoKV's FIX_NMAD_002 newmem code)
#
# Entered in place of the 5 bytes  movzx ebx,word ptr [ecx] ; pop edi ; pop esi  (0F B7 19 5F 5E).
# Falls through at the end; the loader appends  jmp site+5  (the original epilogue).
#   A_MEP = byte [FFX.exe+D2DE26]   bit 0 = Anti-Drain   (Mep)
#   A_NIM = byte [FFX.exe+D2DDCE]   bit 0 = Anti-Drain   (Nimrook)
#   A_ZAL = byte [FFX.exe+D2DE29]   bit 2 = Anti-Venom   (Zalitz)
cave_blitzlearn:
    movzx ebx, word ptr [ecx]               # the original instructions
    pop   edi
    pop   esi
    or    byte ptr [A_MEP], 0x01
    or    byte ptr [A_NIM], 0x01
    or    byte ptr [A_ZAL], 0x04
cave_blitzlearn_end:
