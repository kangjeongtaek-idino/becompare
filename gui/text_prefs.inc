; text_prefs.inc - Text Compare: the view options are remembered (IntelliJ keeps its diff settings too)
;
;   Folding and its context, the unified view, word / character highlight, split changes, scroll sync, whitespace marks, line
;   numbers and "ignore blank lines" are written to settings.ini ([TextCompare]) when they are changed and
;   read when a text view is created. The options that belong to a comparison (case, blanks, comments, rules)
;   stay with the session as before; the encoding is a property of the files and is not kept.
;   An unattended run (/report, /xlsx, ...) does not read them: its result must not depend on the last
;   interactive use.

extern GetPrivateProfileIntW, WritePrivateProfileStringW, g_ini

section .rdata
WSTR sec_tc_prefs, "TextCompare"
WSTR k_tp_fold, "Fold"
WSTR k_tp_ctx, "FoldContext"
WSTR k_tp_uni, "Unified"
WSTR k_tp_word, "WordHighlight"
WSTR k_tp_char, "CharHighlight"
WSTR k_tp_split, "SplitChanges"
WSTR k_tp_scrl, "SyncScroll"
WSTR k_tp_ws, "Whitespace"
WSTR k_tp_num, "LineNumbers"
WSTR k_tp_blank, "IgnoreBlankLines"
WSTR f_tp_d, "%d"
align 8
tp_tab:                              ; {control id, key}: the check boxes
    dq ID_CHK_FOLD, k_tp_fold
    dq ID_CHK_UNI, k_tp_uni
    dq ID_CHK_WORD, k_tp_word
    dq ID_CHK_CHAR, k_tp_char
    dq ID_CHK_SPLIT, k_tp_split
    dq ID_CHK_SCRL, k_tp_scrl
    dq ID_CHK_WS, k_tp_ws
    dq ID_CHK_NUM, k_tp_num
    dq ID_CHK_BLANK, k_tp_blank
    dq 0, 0
section .text

; tv_prefs_load(hwnd, tv): set the controls to what was used last
tv_prefs_load:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [g_batch], 0
    jne .d
    lea r12, [tp_tab]
.c:
    mov r13, [r12]
    test r13, r13
    jz .combo
    invoke get_chk, rsi, r13         ; the default is how the box was created
    movzx r8d, al
    invoke GetPrivateProfileIntW, &sec_tc_prefs, [r12+8], r8, &g_ini
    xor r8d, r8d
    test eax, eax
    setnz r8b
    invoke set_chk_id, rsi, r13, r8
    add r12, 16
    jmp .c
.combo:
    invoke GetPrivateProfileIntW, &sec_tc_prefs, &k_tp_ctx, 2, &g_ini
    cmp eax, 4
    jbe .cs
    mov eax, 2
.cs:
    mov r13d, eax
    invoke GetDlgItem, rsi, ID_CMB_CTX
    invoke SendMessageW, rax, 0x014E, r13, 0           ; CB_SETCURSEL
    invoke get_chk, rsi, ID_CHK_NUM  ; the gutters read this from the view, not from the box
    xor eax, 1
    and eax, 1
    mov [rbx+TV_NONUM], rax
.d:
    EPILOG 64

; tv_prefs_save(hwnd, id): the control with that id was changed by the user - remember its state
tv_prefs_save:
    PROLOG 96
    mov rsi, rcx
    mov ebx, edx
    cmp qword [g_batch], 0
    jne .d
    cmp ebx, ID_CMB_CTX
    je .combo
    lea r12, [tp_tab]
.f:
    mov rax, [r12]
    test rax, rax
    jz .d
    cmp rax, rbx
    je .chk
    add r12, 16
    jmp .f
.chk:
    invoke get_chk, rsi, rbx
    movzx r13d, al
    jmp .write
.combo:
    lea r12, [tp_tab-16]             ; (its key is not in the table)
    invoke GetDlgItem, rsi, ID_CMB_CTX
    invoke SendMessageW, rax, 0x0147, 0, 0             ; CB_GETCURSEL
    mov r13d, eax
    cmp r13d, 4
    ja .d
    invoke _snwprintf, &rsp+64, 8, &f_tp_d, r13
    mov word [rsp+64+16], 0
    invoke WritePrivateProfileStringW, &sec_tc_prefs, &k_tp_ctx, &rsp+64, &g_ini
    EPILOG 96
.write:
    invoke _snwprintf, &rsp+64, 8, &f_tp_d, r13
    mov word [rsp+64+16], 0
    invoke WritePrivateProfileStringW, &sec_tc_prefs, [r12+8], &rsp+64, &g_ini
.d:
    EPILOG 96
