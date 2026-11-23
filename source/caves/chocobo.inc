# Chocobo Catcher cave  (port of NikoKV's TWEAK_MGCC_001 newmem code)
#
# Entered in place of the 7 bytes  mov [edx],eax ; pop ebp ; ret ; fild [ebp+14]
# (89 02 5D C3 DB 45 14). Ends with its own pop ebp / ret, like the original script.
# (The original's trailing "fild / jmp" after the ret was unreachable and is not needed.)
#   A_AREA   = word [FFX.exe+D2CA90]   current area id (0xDF = Chocobo Catcher)
#   A_SCHEME = byte in the cave's own memory: 0 = preset 1 (3/0/0/0), 1 = preset 2 (4/0/0/0),
#              2 = preset 3 (30/0/0/30)   (set from modules\ffx_tweaks.ini)
cave_choco:
    mov  [edx], eax                         # the original instruction
    cmp  word ptr [A_AREA], 0x00DF
    jne  c_exit
    cmp  eax, 0x64
    jne  c_exit
    cmp  byte ptr [edx+0x47], 0             # only while Tidus' ball count is still 0
    jne  c_exit
    cmp  byte ptr [A_SCHEME], 0
    je   c_s0
    cmp  byte ptr [A_SCHEME], 1
    je   c_s1
    cmp  byte ptr [A_SCHEME], 2
    je   c_s2
    jmp  c_exit
c_s0:
    add  byte ptr [edx+0x47], 3
    jmp  c_exit
c_s1:
    add  byte ptr [edx+0x47], 4
    jmp  c_exit
c_s2:
    add  byte ptr [edx+0x47], 30            # decimal 30, as in the original (#30)
    add  byte ptr [edx+0x4A], 30
c_exit:
    pop  ebp
    ret
cave_choco_end:
