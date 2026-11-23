# Lightning Dodger cave  (port of NikoKV's TWEAK_MGLD_005 newmem code)
#
# Entered in place of:   mov [edx],ax ; pop ebp ; ret      (66 89 02 5D C3)
# Always ends with pop ebp / ret itself, exactly like the original script.
#   in : edx -> counter, [edx-0x26] = area signature word, ax = value being written
#   A_AREA   = word  [FFX.exe+D2CA90]   current area id
#   A_STREAK = dword [FFX.exe+D2CE90]   saved dodge streak
cave_dodger:
    cmp  word ptr [A_AREA], 0x008C          # Thunder Plains South?
    je   d_south
    cmp  word ptr [A_AREA], 0x00A2          # Thunder Plains North?
    je   d_north
    jmp  d_orig
d_south:
    cmp  word ptr [edx-0x26], 0xDD2C
    je   d_found
    jmp  d_orig
d_north:
    cmp  word ptr [edx-0x26], 0x1078
    je   d_found
    jmp  d_orig
d_found:
    push ecx
    cmp  ax, 0                              # a "fail" tries to write 0 -> ignore it
    je   d_none
    mov  ecx, [A_STREAK]
    cmp  ecx, [edx]
    je   d_inc
    mov  [edx], cx                          # restore the saved streak
d_inc:
    inc  word ptr [edx]                     # count the dodge
d_none:
    pop  ecx
    pop  ebp
    ret
d_orig:
    mov  [edx], ax
    pop  ebp
    ret
cave_dodger_end:
