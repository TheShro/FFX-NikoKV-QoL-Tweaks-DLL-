# Blitzball "temporary levels boost in Exhibition matches"  (port of NikoKV's TWEAK_BBSL_001)
#
# Entered in place of the 5 bytes  mov eax,ebx ; pop ebx ; pop ebp ; ret   (8B C3 5B 5D C3)
# at the end of the "read a byte stat" case. Ends with its own pop ebx / pop ebp / ret.
#   in : ebx = the stat value (a level), ecx = ADDRESS the stat was read from
#   A_AREA   = word [FFX.exe+D2CA90]  area id (0x3E = Blitzball match)
#   A_LGFLAG = byte [FFX.exe+D2E10A]  match type (2 = Exhibition)
#   A_ARRAY  = ADDRESS FFX.exe+D2E04A start of the 0x3C-byte player stat array
#   A_SLOT0..5 = bytes [FFX.exe+D2E0BE..C3]  the six "Beside Aurochs" player ids
# (The original had a "cmp eax,0 / jae" that is always true; it is left out.)
cave_blitzlvl:
    cmp  word ptr [A_AREA], 0x003E
    jne  b_orig
    cmp  byte ptr [A_LGFLAG], 2
    jne  b_orig
    mov  eax, ecx
    sub  eax, offset A_ARRAY                # index of this stat inside the player array
    cmp  eax, 0x3C
    jae  b_orig                             # unsigned: outside the array -> not a player stat
    cmp  al, byte ptr [A_SLOT0]
    je   b_agent
    cmp  al, byte ptr [A_SLOT1]
    je   b_agent
    cmp  al, byte ptr [A_SLOT2]
    je   b_agent
    cmp  al, byte ptr [A_SLOT3]
    je   b_agent
    cmp  al, byte ptr [A_SLOT4]
    je   b_agent
    cmp  al, byte ptr [A_SLOT5]
    je   b_agent
    jmp  b_orig
b_agent:
    mov  eax, ebx
    add  eax, 10                            # temporarily +10 levels (decimal, as the original #10)
    jmp  b_exit
b_orig:
    mov  eax, ebx
b_exit:
    pop  ebx
    pop  ebp
    ret
cave_blitzlvl_end:
