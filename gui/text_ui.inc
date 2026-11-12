; text_ui.inc - Text Compare: rows, colouring, compare, navigation, copy, save, window procedure

; tv_build(tv, LA, LB, hunks): aligned rows + blocks + display strings
tv_build:
    PROLOG 192
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov r15, r9
    invoke free, [r12+TV_ROWS]
    invoke free, [r12+TV_BLK]
    invoke free, [r12+TV_BUFA]
    invoke free, [r12+TV_BUFB]
    mov rax, [r13+8]
    add rax, [r14+8]
    add rax, 2
    mov rcx, rax
    shl rcx, 4
    call malloc
    mov [r12+TV_ROWS], rax
    mov rcx, [r15+8]
    add rcx, 2
    shl rcx, 3
    call malloc
    mov [r12+TV_BLK], rax
    mov [rsp+96], rax
    invoke sum_len, r13
    mov [rsp+104], rax
    invoke sum_len, r14
    add rax, [rsp+104]
    mov rcx, [r13+8]
    add rcx, [r14+8]
    lea rax, [rax+rcx*2+16]          ; characters per buffer (generous)
    lea rcx, [rax*2]
    mov [rsp+104], rcx
    call malloc
    mov [r12+TV_BUFA], rax
    mov rcx, [rsp+104]
    call malloc
    mov [r12+TV_BUFB], rax
    mov rbx, [r12+TV_ROWS]
    xor esi, esi                     ; row count
    xor ebp, ebp                     ; block count
    xor edi, edi                     ; hunk index
.h:
    cmp rdi, [r15+8]
    jae .rowsdone
    lea rax, [rdi+rdi*2]
    shl rax, 3
    add rax, [r15]
    mov ecx, [rax]                   ; type
    mov r8d, [rax+4]                 ; aStart
    mov r9d, [rax+8]                 ; aLen
    mov r10d, [rax+12]               ; bStart
    mov r11d, [rax+16]               ; bLen
    test ecx, ecx
    jnz .chg
.eq:
    test r9d, r9d
    jz .hn
    ADDROW 0, r8d, r10d, -1
    inc r8d
    inc r10d
    dec r9d
    jmp .eq
.chg:
    mov [rsp+112], esi               ; first row of the block
    mov edx, r9d
    cmp edx, r11d
    jbe .pm
    mov edx, r11d                    ; pairs = min(aLen, bLen)
.pm:
    mov eax, r9d
    sub eax, edx
    mov [rsp+120], eax               ; extra rows only in A
    mov eax, r11d
    sub eax, edx
    mov [rsp+124], eax               ; extra rows only in B
.pr:
    test edx, edx
    jz .la
    ADDROW 1, r8d, r10d, ebp
    inc r8d
    inc r10d
    dec edx
    jmp .pr
.la:
    mov ecx, [rsp+120]
.lal:
    test ecx, ecx
    jz .lb
    ADDROW 2, r8d, -1, ebp
    inc r8d
    dec ecx
    jmp .lal
.lb:
    mov ecx, [rsp+124]
.lbl:
    test ecx, ecx
    jz .blk
    ADDROW 3, -1, r10d, ebp
    inc r10d
    dec ecx
    jmp .lbl
.blk:
    mov eax, [rsp+112]
    cmp esi, eax
    jbe .hn
    mov rcx, [rsp+96]
    mov [rcx+rbp*8], eax
    mov [rcx+rbp*8+4], esi
    inc ebp
.hn:
    inc rdi
    jmp .h
.rowsdone:
    mov [r12+TV_NROWS], rsi
    mov [r12+TV_NBLK], rbp
    mov r15, rsi                     ; rows
    mov rdi, [r12+TV_BUFA]
    mov rbp, [r12+TV_BUFB]
    xor esi, esi
.t:
    cmp rsi, r15
    jae .tdone
    mov rax, rsi
    shl rax, 4
    add rax, rbx
    mov ecx, [rax+4]                 ; aIdx
    cmp ecx, -1
    je .fa
    mov rdx, [r13]
    mov rdx, [rdx+rcx*8]
.ca:
    movzx r8d, word [rdx]
    test r8d, r8d
    jz .adone
    mov [rdi], r8w
    add rdi, 2
    add rdx, 2
    jmp .ca
.fa:
    mov word [rdi], FILLER
    add rdi, 2
.adone:
    lea r9, [rsi+1]
    cmp r9, r15
    jae .bside
    mov word [rdi], 13
    add rdi, 2
.bside:
    mov ecx, [rax+8]                 ; bIdx
    cmp ecx, -1
    je .fb
    mov rdx, [r14]
    mov rdx, [rdx+rcx*8]
.cb:
    movzx r8d, word [rdx]
    test r8d, r8d
    jz .bdone
    mov [rbp], r8w
    add rbp, 2
    add rdx, 2
    jmp .cb
.fb:
    mov word [rbp], FILLER
    add rbp, 2
.bdone:
    lea r9, [rsi+1]
    cmp r9, r15
    jae .nx
    mov word [rbp], 13
    add rbp, 2
.nx:
    inc rsi
    jmp .t
.tdone:
    mov word [rdi], 0
    mov word [rbp], 0
    mov rax, rdi
    sub rax, [r12+TV_BUFA]
    shr rax, 1
    mov [r12+TV_LENA], rax
    mov rax, rbp
    sub rax, [r12+TV_BUFB]
    shr rax, 1
    mov [r12+TV_LENB], rax
    EPILOG 192

; tv_color(tv, LA, LB): paint changed rows and the differing span inside them
tv_color:
    PROLOG 224
    mov r12, rcx
    mov r13, rdx
    mov r14, r8
    mov rbx, [r12+TV_ROWS]
    mov r15, [r12+TV_NROWS]
    xor esi, esi
    xor edi, edi                     ; char offset in pane A
    xor ebp, ebp                     ; char offset in pane B
.r:
    cmp rsi, r15
    jae .done
    mov rax, rsi
    shl rax, 4
    add rax, rbx
    mov [rsp+96], rax
    mov ecx, [rax+4]
    cmp ecx, -1
    je .fa
    mov rdx, [r13]
    mov rdx, [rdx+rcx*8]
    mov [rsp+104], rdx
    xor ecx, ecx
.la:
    cmp word [rdx+rcx*2], 0
    je .lad
    inc rcx
    jmp .la
.lad:
    mov [rsp+112], rcx
    jmp .bs
.fa:
    mov qword [rsp+104], 0
    mov qword [rsp+112], 1
.bs:
    mov rax, [rsp+96]
    mov ecx, [rax+8]
    cmp ecx, -1
    je .fb
    mov rdx, [r14]
    mov rdx, [rdx+rcx*8]
    mov [rsp+120], rdx
    xor ecx, ecx
.lb:
    cmp word [rdx+rcx*2], 0
    je .lbd
    inc rcx
    jmp .lb
.lbd:
    mov [rsp+128], rcx
    jmp .kind
.fb:
    mov qword [rsp+120], 0
    mov qword [rsp+128], 1
.kind:
    mov rax, [rsp+96]
    mov ecx, [rax]
    test ecx, ecx
    jz .adv
    cmp ecx, 1
    je .k1
    cmp ecx, 2
    je .k2
    ; kind 3: only in B
    mov r8, [rsp+112]
    add r8, rdi
    invoke rich_bk, [r12+TV_HA], rdi, r8, #[th_tc_gap]
    mov r8, [rsp+128]
    add r8, rbp
    invoke rich_bk, [r12+TV_HB], rbp, r8, #[th_tc_ins]
    jmp .adv
.k2:
    mov r8, [rsp+112]
    add r8, rdi
    invoke rich_bk, [r12+TV_HA], rdi, r8, #[th_tc_del]
    mov r8, [rsp+128]
    add r8, rbp
    invoke rich_bk, [r12+TV_HB], rbp, r8, #[th_tc_gap]
    jmp .adv
.k1:
    mov r8, [rsp+112]
    add r8, rdi
    invoke rich_bk, [r12+TV_HA], rdi, r8, #[th_tc_mod]
    mov r8, [rsp+128]
    add r8, rbp
    invoke rich_bk, [r12+TV_HB], rbp, r8, #[th_tc_mod]
    ; word level changes (IntelliJ style); long lines fall back to the prefix / suffix span below
    invoke tv_wordhl, r12, &rsp+104, rdi, rbp
    test eax, eax
    jnz .adv
    ; common prefix of the two lines
    mov rdx, [rsp+104]
    mov r8, [rsp+120]
    xor ecx, ecx
.p:
    movzx eax, word [rdx+rcx*2]
    test eax, eax
    jz .pd
    movzx r9d, word [r8+rcx*2]
    cmp eax, r9d
    jne .pd
    inc rcx
    jmp .p
.pd:
    mov [rsp+136], rcx
    xor r11d, r11d
.s:
    mov rax, [rsp+112]
    sub rax, [rsp+136]
    cmp r11, rax
    jae .sd
    mov r10, [rsp+128]
    sub r10, [rsp+136]
    cmp r11, r10
    jae .sd
    mov rcx, [rsp+112]
    sub rcx, r11
    movzx eax, word [rdx+rcx*2-2]
    mov rcx, [rsp+128]
    sub rcx, r11
    movzx r9d, word [r8+rcx*2-2]
    cmp eax, r9d
    jne .sd
    inc r11
    jmp .s
.sd:
    mov [rsp+144], r11
    mov rax, rdi
    add rax, [rsp+136]
    mov r8, rdi
    add r8, [rsp+112]
    sub r8, [rsp+144]
    cmp rax, r8
    jae .nohl_a
    invoke rich_bk, [r12+TV_HA], rax, r8, #[th_tc_modin]
.nohl_a:
    mov rax, rbp
    add rax, [rsp+136]
    mov r8, rbp
    add r8, [rsp+128]
    sub r8, [rsp+144]
    cmp rax, r8
    jae .adv
    invoke rich_bk, [r12+TV_HB], rax, r8, #[th_tc_modin]
.adv:
    add rdi, [rsp+112]
    inc rdi
    add rbp, [rsp+128]
    inc rbp
    inc rsi
    jmp .r
.done:
    EPILOG 224

; ---------------------------------------------------------------- load one side
; text_side(hwnd, tv, side) -> eax 1 ok : load from file, or take the (edited) pane text
text_side:
    PROLOG 1216
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    mov r13d, ID_EDIT_A
    mov r14d, ID_RA
    lea r15, [rbx+TV_TXA]
    lea rdi, [rbx+TV_PATHA]
    lea rbp, [rbx+TV_LDA]
    test r12d, r12d
    jz .ids
    mov r13d, ID_EDIT_B
    mov r14d, ID_RB
    lea r15, [rbx+TV_TXB]
    lea rdi, [rbx+TV_PATHB]
    lea rbp, [rbx+TV_LDB]
.ids:
    invoke GetDlgItem, rsi, r13
    invoke GetWindowTextW, rax, &rsp+96, 519
    cmp dword [rbp], 0
    je .load
    invoke wcscmp, &rsp+96, rdi
    test eax, eax
    jnz .load
    invoke GetDlgItem, rsi, r14
    invoke rich_get_text, rax, &rsp+1140
    test rax, rax
    jz .fail
    mov r13, rax
    invoke strip_fillers, r13, [rsp+1140]
    mov r14, rax                     ; new length (free() would clobber rax)
    invoke free, [r15]
    mov [r15], r13
    mov [r15+8], r14
    mov eax, 1
    EPILOG 1216
.load:
    cmp word [rsp+96], 0
    je .fail
    cmp dword [rbp], 0
    je .fresh
    invoke free, [r15]
    mov qword [r15], 0
.fresh:
    mov dword [rbp], 0
    invoke text_load, &rsp+96, r15
    test eax, eax
    jz .fail
    invoke wcscpy, rdi, &rsp+96
    mov dword [rbp], 1
    mov eax, 1
    EPILOG 1216
.fail:
    xor eax, eax
    EPILOG 1216

; ---------------------------------------------------------------- info line
; tv_info(hwnd, tv)
tv_info:
    PROLOG 480
    mov rsi, rcx
    mov rbx, rdx
    mov r12, [rbx+TV_NBLK]
    mov eax, [rbx+TV_TXA+16]
    and eax, 3
    lea rcx, [enc_names]
    mov r14, [rcx+rax*8]
    mov eax, [rbx+TV_TXA+20]
    and eax, 1
    lea rcx, [eol_names]
    mov r15, [rcx+rax*8]
    test r12, r12
    jnz .diffs
    mov r9, [rbx+TV_NLA]
    mov rax, [rbx+TV_NLB]
    mov [rsp+32], rax
    mov [rsp+40], r14
    mov [rsp+48], r15
    lea rcx, [rsp+96]
    mov edx, 140
    lea r8, [f_same]
    call _snwprintf
    jmp .set
.diffs:
    mov rax, [rbx+TV_CUR]
    inc rax
    mov [rsp+32], r12
    mov rcx, [rbx+TV_NLA]
    mov [rsp+40], rcx
    mov rcx, [rbx+TV_NLB]
    mov [rsp+48], rcx
    mov [rsp+56], r14
    mov [rsp+64], r15
    mov r9, rax
    lea rcx, [rsp+96]
    mov edx, 140
    lea r8, [f_info]
    ; args: buf, cnt, fmt, nblk, cur, nblk, NLA, NLB, enc, eol -> r9 = nblk
    mov r9, r12
    mov [rsp+32], rax
    mov [rsp+40], r12
    mov rax, [rbx+TV_NLA]
    mov [rsp+48], rax
    mov rax, [rbx+TV_NLB]
    mov [rsp+56], rax
    mov [rsp+64], r14
    mov [rsp+72], r15
    call _snwprintf
.set:
    invoke GetDlgItem, rsi, ID_LBL_INFO
    invoke SetWindowTextW, rax, &rsp+96
    invoke _snwprintf, &rsp+420, 30, &f_enceol, r14, r15
    invoke set_status_part, 2, &rsp+420
    EPILOG 480

; ---------------------------------------------------------------- compare
; text_compare(hwnd, tv, sess)
%define C_LA   96
%define C_LB   112
%define C_HV   128
%define C_CP   160
%define C_DA   288
%define C_DB   296
%define C_TT   304                  ; title buffer (wchar[64] = 128 bytes) -> 432
text_compare:
    PROLOG 448
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    invoke text_side, rsi, rbx, 0
    test eax, eax
    jz .err
    invoke text_side, rsi, rbx, 1
    test eax, eax
    jz .err
    ; Preserve dirty state across redraw/recomparison and theme changes.
    invoke SendMessageW, [rbx+TV_HA], EM_GETMODIFY, 0, 0
    mov [rsp+432], rax
    invoke SendMessageW, [rbx+TV_HB], EM_GETMODIFY, 0, 0
    mov [rsp+440], rax
    ; private copies for in-place line splitting
    mov rax, [rbx+TV_TXA+8]
    lea rcx, [rax*2+8]
    call malloc
    mov [rsp+C_DA], rax
    mov rdx, [rbx+TV_TXA+8]
    lea r8, [rdx*2+2]
    invoke memcpy, rax, [rbx+TV_TXA], r8
    mov rax, [rbx+TV_TXB+8]
    lea rcx, [rax*2+8]
    call malloc
    mov [rsp+C_DB], rax
    mov rdx, [rbx+TV_TXB+8]
    lea r8, [rdx*2+2]
    invoke memcpy, rax, [rbx+TV_TXB], r8
    invoke lines_split, [rsp+C_DA], [rbx+TV_TXA+8], &rsp+C_LA
    invoke lines_split, [rsp+C_DB], [rbx+TV_TXB+8], &rsp+C_LB
    mov rax, [rsp+C_LA+8]
    mov [rbx+TV_NLA], rax
    mov rax, [rsp+C_LB+8]
    mov [rbx+TV_NLB], rax
    ; flags
    xor r13d, r13d
    invoke get_chk, rsi, ID_CHK_CASE
    or r13d, eax
    invoke get_chk, rsi, ID_CHK_SPC
    shl eax, 1
    or r13d, eax
    invoke get_chk, rsi, ID_CHK_TRIM
    shl eax, 2
    or r13d, eax
    invoke get_chk, rsi, ID_CHK_CMT
    shl eax, 3
    or r13d, eax
    invoke GetDlgItem, rsi, ID_EDIT_CMT
    invoke GetWindowTextW, rax, &rsp+C_CP, 60
    xor eax, eax
    mov [rsp+C_HV], rax
    mov [rsp+C_HV+8], rax
    mov [rsp+C_HV+16], rax
    invoke get_chk, rsi, ID_CHK_IDS
    shl eax, 4
    or r13d, eax
    invoke GetDlgItem, rsi, ID_EDIT_RULES
    invoke GetWindowTextW, rax, &rbx+TV_RULES, 120
    cmp word [rbx+TV_RULES], 0
    je .norules
    or r13d, 32
    lea rax, [rbx+TV_RULES]
    mov [g_tc_rules], rax
.norules:
    invoke tv_cmp_anch, rbx, &rsp+C_LA, &rsp+C_LB, r13, &rsp+C_CP, &rsp+C_HV
    invoke tv_build, rbx, &rsp+C_LA, &rsp+C_LB, &rsp+C_HV
    invoke SendMessageW, [rbx+TV_HA], WM_SETREDRAW, 0, 0
    invoke SendMessageW, [rbx+TV_HB], WM_SETREDRAW, 0, 0
    invoke rich_set_text, [rbx+TV_HA], [rbx+TV_BUFA]
    invoke rich_set_text, [rbx+TV_HB], [rbx+TV_BUFB]
    invoke rich_theme, [rbx+TV_HA]
    invoke rich_theme, [rbx+TV_HB]
    invoke tv_color, rbx, &rsp+C_LA, &rsp+C_LB
    invoke rich_unsel, [rbx+TV_HA]
    invoke rich_unsel, [rbx+TV_HB]
    invoke tv_syn_apply, rsi, rbx
    invoke SendMessageW, [rbx+TV_HA], EM_SETMODIFY, [rsp+432], 0
    invoke SendMessageW, [rbx+TV_HB], EM_SETMODIFY, [rsp+440], 0
    invoke SendMessageW, [rbx+TV_HA], WM_SETREDRAW, 1, 0
    invoke SendMessageW, [rbx+TV_HB], WM_SETREDRAW, 1, 0
    invoke InvalidateRect, [rbx+TV_HA], 0, 1
    invoke InvalidateRect, [rbx+TV_HB], 0, 1
    mov qword [rbx+TV_CUR], -1
    mov dword [rbx+TV_NBM], 0
    invoke free, [rsp+C_DA]
    invoke free, [rsp+C_DB]
    invoke free, [rsp+C_LA]
    invoke free, [rsp+C_LB]
    invoke hunk_free, &rsp+C_HV
    ; tab title: base names
    invoke wcsrchr, &rbx+TV_PATHA, 0x5C
    lea r13, [rbx+TV_PATHA]
    test rax, rax
    jz .ta
    lea r13, [rax+2]
.ta:
    invoke wcsrchr, &rbx+TV_PATHB, 0x5C
    lea r14, [rbx+TV_PATHB]
    test rax, rax
    jz .tb
    lea r14, [rax+2]
.tb:
    invoke _snwprintf, &rsp+C_TT, 60, &f_title, r13, r14
    invoke set_tab_title, r12, &rsp+C_TT
    invoke tv_info, rsi, rbx
    invoke tv_redraw_aux, rbx
    cmp qword [rbx+TV_NBLK], 0
    je .ok
    invoke tv_goto, rsi, rbx, 0
.ok:
    test qword [g_batch], 0x10        ; /xlsx file: export the first compare and close
    jz .ok2
    and qword [g_batch], ~0x10
    invoke text_xlsx_path, rbx, &g_xl_path
    test eax, eax
    jnz .okx
    mov qword [g_exitcode], 2
.okx:
    cmp qword [rbx+TV_NBLK], 0        ; exit code like /report: 0 same, 1 different, 2 error
    je .okq
    cmp qword [g_exitcode], 2
    je .okq
    mov qword [g_exitcode], 1
.okq:
    test qword [g_batch], 4
    jz .ok2
    invoke PostMessageW, [g_hframe], WM_CLOSE, 0, 0
.ok2:
    mov eax, 1
    EPILOG 448
.err:
    invoke msgbox, rsi, &s_cantread, MB_OK|MB_ICONWARNING
    xor eax, eax
    EPILOG 448

; ---------------------------------------------------------------- navigation
; tv_caretrow(tv) -> eax row of the caret in the last focused pane
tv_caretrow:
    PROLOG 96
    mov rbx, rcx
    mov rsi, [rbx+TV_HA]
    cmp qword [rbx+TV_LASTP], 0
    je .go
    mov rsi, [rbx+TV_HB]
.go:
    invoke SendMessageW, rsi, EM_EXGETSEL, 0, &rsp+80
    mov eax, [rsp+80]
    invoke SendMessageW, rsi, EM_LINEFROMCHAR, rax, 0
    EPILOG 96

; tv_goto(hwnd, tv, block): put the caret on the first row of a block and scroll it into view
tv_goto:
    PROLOG 96
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov rax, [rbx+TV_BLK]
    mov r13d, [rax+r12*8]            ; first row
    mov qword [rbx+TV_GUARD], 1
    xor r14d, r14d
.pane:
    mov rdi, [rbx+TV_HA]
    test r14d, r14d
    jz .go
    mov rdi, [rbx+TV_HB]
.go:
    invoke SendMessageW, rdi, EM_LINEINDEX, r13, 0
    movsxd rax, eax
    mov r15, rax
    invoke SendMessageW, rdi, EM_SETSEL, r15, r15
    invoke SendMessageW, rdi, EM_GETFIRSTVISIBLELINE, 0, 0
    mov ecx, r13d
    sub ecx, 3
    jns .t
    xor ecx, ecx
.t:
    sub ecx, eax
    movsxd r15, ecx
    invoke SendMessageW, rdi, EM_LINESCROLL, 0, r15
    inc r14d
    cmp r14d, 2
    jb .pane
    mov qword [rbx+TV_GUARD], 0
    mov [rbx+TV_CUR], r12
    invoke tv_info, rsi, rbx
    invoke tv_redraw_aux, rbx
    EPILOG 96

; tv_step(hwnd, tv, dir 1=next 0=prev)
tv_step:
    PROLOG 96
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    cmp qword [rbx+TV_NBLK], 0
    je .none
    invoke tv_caretrow, rbx
    mov r13d, eax                    ; caret row
    mov rcx, [rbx+TV_BLK]
    xor edx, edx
    mov r14, -1                      ; answer
    test r12d, r12d
    jz .prev
.nx:
    cmp rdx, [rbx+TV_NBLK]
    jae .have
    mov eax, [rcx+rdx*8]
    cmp eax, r13d
    ja .nfound
    inc rdx
    jmp .nx
.nfound:
    mov r14, rdx
    jmp .have
.prev:
    cmp rdx, [rbx+TV_NBLK]
    jae .have
    mov eax, [rcx+rdx*8]
    cmp eax, r13d
    jae .have
    mov r14, rdx
    inc rdx
    jmp .prev
.have:
    cmp r14, -1
    je .none
    invoke tv_goto, rsi, rbx, r14
    EPILOG 96
.none:
    invoke set_status, &s_nodiff
    EPILOG 96

; tv_curblock(tv) -> rax block containing the caret row (or the next one), -1 if none
tv_curblock:
    PROLOG 64
    mov rbx, rcx
    invoke tv_caretrow, rbx
    mov esi, eax
    mov rcx, [rbx+TV_BLK]
    xor edx, edx
    mov rax, -1
.l:
    cmp rdx, [rbx+TV_NBLK]
    jae .d
    mov edi, [rcx+rdx*8]
    cmp edi, esi
    ja .after
    cmp esi, [rcx+rdx*8+4]
    jb .inside
    inc rdx
    jmp .l
.after:
    mov rax, rdx
    jmp .d
.inside:
    mov rax, rdx
.d:
    EPILOG 64

; tv_copy(hwnd, tv, sess, dir 0 = left->right, 1 = right->left)
tv_copy:
    PROLOG 192
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov r13d, r9d
    mov rax, [rbx+TV_FORCE]
    cmp rax, -1
    jne .forced
    invoke tv_curblock, rbx
    cmp rax, -1
    je .none
    jmp .gotblk
.forced:
    mov qword [rbx+TV_FORCE], -1
.gotblk:
    mov r14, rax
    mov rcx, [rbx+TV_BLK]
    mov r15d, [rcx+r14*8]            ; first row
    mov eax, [rcx+r14*8+4]
    mov [rsp+96], eax                ; end row (exclusive)
    mov rdi, [rbx+TV_HA]             ; source
    mov rbp, [rbx+TV_HB]             ; target
    test r13d, r13d
    jz .go
    xchg rdi, rbp
.go:
    ; character ranges: source [cs, ce), target [ds, de)
    invoke SendMessageW, rdi, EM_LINEINDEX, r15, 0
    movsxd rax, eax
    mov [rsp+104], rax               ; cs
    invoke SendMessageW, rbp, EM_LINEINDEX, r15, 0
    movsxd rax, eax
    mov [rsp+112], rax               ; ds
    mov eax, [rsp+96]
    cmp rax, [rbx+TV_NROWS]
    jae .tail
    invoke SendMessageW, rdi, EM_LINEINDEX, rax, 0
    movsxd rax, eax
    mov [rsp+120], rax               ; ce
    mov eax, [rsp+96]
    invoke SendMessageW, rbp, EM_LINEINDEX, rax, 0
    movsxd rax, eax
    mov [rsp+128], rax               ; de
    jmp .range
.tail:
    invoke rich_len, rdi
    mov [rsp+120], rax
    invoke rich_len, rbp
    mov [rsp+128], rax
.range:
    mov rax, [rsp+120]
    sub rax, [rsp+104]
    lea rcx, [rax*2+16]
    call malloc
    mov r14, rax
    mov rax, [rsp+104]
    mov [rsp+136], eax               ; CHARRANGE cpMin
    mov rax, [rsp+120]
    mov [rsp+140], eax               ; cpMax
    invoke SendMessageW, rdi, EM_EXSETSEL, 0, &rsp+136
    invoke SendMessageW, rdi, 0x043E, 0, r14                     ; EM_GETSELTEXT
    mov rax, [rsp+112]
    mov [rsp+136], eax
    mov rax, [rsp+128]
    mov [rsp+140], eax
    invoke SendMessageW, rbp, EM_EXSETSEL, 0, &rsp+136
    invoke SendMessageW, rbp, EM_REPLACESEL, 1, r14
    invoke free, r14
    ; re-compare from the panes and keep the scroll position
    invoke SendMessageW, [rbx+TV_HA], EM_GETFIRSTVISIBLELINE, 0, 0
    mov r14d, eax
    invoke text_compare, rsi, rbx, r12
    invoke SendMessageW, [rbx+TV_HA], EM_GETFIRSTVISIBLELINE, 0, 0
    sub r14d, eax
    movsxd r14, r14d
    mov qword [rbx+TV_GUARD], 1
    invoke SendMessageW, [rbx+TV_HA], EM_LINESCROLL, 0, r14
    invoke SendMessageW, [rbx+TV_HB], EM_LINESCROLL, 0, r14
    mov qword [rbx+TV_GUARD], 0
    EPILOG 192
.none:
    invoke set_status, &s_nocur
    EPILOG 192

; ---------------------------------------------------------------- save
; tv_save(hwnd, tv, side) -> eax 1 saved
tv_save:
    PROLOG 1216
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    mov r13d, ID_EDIT_A
    mov r14d, ID_RA
    lea r15, [rbx+TV_TXA]
    test r12d, r12d
    jz .ids
    mov r13d, ID_EDIT_B
    mov r14d, ID_RB
    lea r15, [rbx+TV_TXB]
.ids:
    invoke GetDlgItem, rsi, r13
    invoke GetWindowTextW, rax, &rsp+96, 519
    cmp word [rsp+96], 0
    je .nopath
    invoke GetDlgItem, rsi, r14
    mov rdi, rax
    invoke rich_get_text, rdi, &rsp+1140
    test rax, rax
    jz .fail
    mov rbp, rax
    invoke strip_fillers, rbp, [rsp+1140]
    mov r13, rax
    cmp dword [r15+24], 0            ; the original ended with a line break
    je .nofinal
    mov word [rbp+r13*2], 13
    inc r13
    mov word [rbp+r13*2], 0
.nofinal:
    mov r9d, [r15+20]                ; crlf
    shl r9d, 8
    or r9d, [r15+16]                 ; enc
    invoke text_save, &rsp+96, rbp, r13, r9
    mov r14d, eax
    invoke free, rbp
    test r14d, r14d
    jnz .saved
    invoke save_error, rsi
    jmp .fail
.saved:
    invoke SendMessageW, rdi, EM_SETMODIFY, 0, 0
    invoke set_status, &s_saved
    mov eax, 1
    EPILOG 1216
.nopath:
    invoke msgbox, rsi, &s_nopath, MB_OK|MB_ICONWARNING
.fail:
    xor eax, eax
    EPILOG 1216

; ---------------------------------------------------------------- layout
%macro PLACE 2
    mov ecx, %2
    call scl
    mov ebx, eax
    ; wrap to the next toolbar row when the control would leave the client area
    cmp r12d, r14d
    je %%fit
    lea eax, [r12+rbx]
    cmp eax, [rsp+104]
    jbe %%fit
    mov r12d, r14d
    lea r13d, [r13+r15]
    add r13d, r14d
%%fit:
    MVID rsi, %1, r12, r13, rbx, r15
    lea r12d, [r12+rbx]
    add r12d, r14d
%endmacro

tv_layout:
    PROLOG 192
    mov rsi, rcx
    invoke GetClientRect, rsi, &rsp+96
    mov ecx, 4
    call scl
    mov r14d, eax
    mov ecx, 26
    call scl
    mov r15d, eax
    mov edx, [rsp+104]
    invoke pb_layout, rsi, rdx, r14
    mov r13d, eax
    mov r12d, r14d
    PLACE ID_COMPARE, 96
    PLACE ID_BTN_PREV, 40
    PLACE ID_BTN_NEXT, 40
    PLACE ID_BTN_CLR, 56
    PLACE ID_BTN_CRL, 56
    PLACE ID_BTN_SVL, 90
    PLACE ID_BTN_SVR, 100
    PLACE ID_BTN_ALN, 100
    PLACE ID_BTN_ALC, 100
    PLACE ID_BTN_XLS, 90
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_CHK_CASE, 110
    PLACE ID_CHK_SPC, 90
    PLACE ID_CHK_TRIM, 120
    PLACE ID_CHK_CMT, 90
    PLACE ID_EDIT_CMT, 56
    PLACE ID_CHK_SYN, 100
    PLACE ID_CHK_IDS, 140
    PLACE ID_LBL_INFO, 520
    lea r13d, [r13+r15]
    add r13d, r14d
    mov r12d, r14d
    PLACE ID_ED_LFIND, 40
    PLACE ID_ED_FIND, 160
    PLACE ID_ED_NEXT, 70
    PLACE ID_ED_PREV, 70
    PLACE ID_ED_LREPL, 56
    PLACE ID_ED_REPL, 160
    PLACE ID_ED_RONE, 70
    PLACE ID_ED_RALL, 100
    PLACE ID_ED_CASE, 110
    PLACE ID_LBL_RULES, 160
    PLACE ID_EDIT_RULES, 220
    lea r13d, [r13+r15]
    add r13d, r14d                   ; y of the panes
    mov ebp, [rsp+108]
    sub ebp, r13d
    jns .h
    xor ebp, ebp
.h:
    mov ecx, 46
    call scl
    mov r14d, eax                    ; gutter width
    mov ecx, 14
    call scl
    mov r12d, eax                    ; overview map width
    mov eax, [rsp+104]
    mov ecx, r14d
    shl ecx, 1
    sub eax, ecx
    sub eax, r12d
    sub eax, 8
    jns .pw
    xor eax, eax
.pw:
    shr eax, 1
    mov ebx, eax                     ; pane width
    MVID rsi, ID_GUL, 0, r13, r14, rbp
    MVID rsi, ID_RA, r14, r13, rbx, rbp
    mov edi, r14d
    add edi, ebx
    add edi, 4
    MVID rsi, ID_GUR, rdi, r13, r14, rbp
    add edi, r14d
    MVID rsi, ID_RB, rdi, r13, rbx, rbp
    add edi, ebx
    add edi, 2
    MVID rsi, ID_MINI, rdi, r13, r12, rbp
    EPILOG 192

; ---------------------------------------------------------------- options string
tv_collect:
    PROLOG 640
    mov rsi, rcx
    mov rdi, rdx
    mov rbx, r8
    invoke pb_collect, rsi, rdi
    invoke get_chk, rsi, ID_CHK_CASE
    mov r12d, eax
    invoke get_chk, rsi, ID_CHK_SPC
    mov r13d, eax
    invoke get_chk, rsi, ID_CHK_TRIM
    mov r14d, eax
    invoke get_chk, rsi, ID_CHK_CMT
    mov r15d, eax
    invoke get_chk, rsi, ID_CHK_SYN
    mov ebp, eax
    invoke GetDlgItem, rsi, ID_EDIT_CMT
    invoke GetWindowTextW, rax, &rsp+96, 60
    invoke _snwprintf, &rdi+SS_OPTS, 1023, &f_opts, r12, r13, r14, r15, rbp, &rsp+96
    EPILOG 640

tv_opts_apply:
    PROLOG 64
    mov rsi, rcx
    lea rbx, [rdx+SS_OPTS]
    cmp word [rbx], 0
    je .d
    movzx r8d, word [rbx]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_CASE, r8
    movzx r8d, word [rbx+2]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_SPC, r8
    movzx r8d, word [rbx+4]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_TRIM, r8
    movzx r8d, word [rbx+6]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_CMT, r8
    lea r12, [rbx+10]
    cmp word [rbx+8], '|'
    je .oldfmt
    movzx r8d, word [rbx+8]
    sub r8d, '0'
    invoke set_chk_id, rsi, ID_CHK_SYN, r8
    lea r12, [rbx+12]
.oldfmt:
    invoke GetDlgItem, rsi, ID_EDIT_CMT
    invoke SetWindowTextW, rax, r12
.d:
    EPILOG 64

; set_chk_id(parent, id, value)
set_chk_id:
    PROLOG 64
    mov rbx, r8
    invoke GetDlgItem, rcx, rdx
    invoke SendMessageW, rax, BM_SETCHECK, rbx, 0
    EPILOG 64

tv_theme:
    PROLOG 64
    mov rsi, rcx
    mov rbx, rdx
    invoke rich_theme, [rbx+TV_HA]
    invoke rich_theme, [rbx+TV_HB]
    invoke theme_checks, rsi
    EPILOG 64

; ---------------------------------------------------------------- window procedure
tv_proc:
    PROLOG 320
    mov rsi, rcx
    mov edi, edx
    mov r12, r8
    mov r13, r9
    cmp edi, WM_CREATE
    je .create
    cmp edi, WM_ERASEBKGND
    je .erase
    invoke GetWindowLongPtrW, rsi, GWLP_USERDATA
    test rax, rax
    jz .def
    mov r14, rax
    mov r15, [r14+SS_DATA]
    test r15, r15
    jz .def
    cmp edi, WM_SIZE
    je .size
    cmp edi, WM_COMMAND
    je .command
    cmp edi, WM_NOTIFY
    je .notify
    cmp edi, BC_RUN
    je .run
    cmp edi, BC_NEXTDIFF
    je .b_next
    cmp edi, BC_BOOKMARK
    je .m_bm
    cmp edi, BC_NEXTMARK
    je .m_next
    cmp edi, BC_PREVMARK
    je .m_prev
    cmp edi, BC_PREVDIFF
    je .b_prev
    cmp edi, BC_COLLECT
    je .collect
    cmp edi, BC_ISDIRTY
    je .isdirty
    cmp edi, BC_SAVE
    je .saveall
    cmp edi, BC_THEME
    je .theme
    cmp edi, WM_APP+50
    je .redraw
    cmp edi, WM_CTLCOLOREDIT
    je .cedit
    cmp edi, WM_CTLCOLORSTATIC
    je .cstatic
    cmp edi, WM_CTLCOLORBTN
    je .cstatic
    cmp edi, WM_SETFOCUS
    je .focus
    cmp edi, WM_DESTROY
    je .destroy
.def:
    invoke DefWindowProcW, rsi, rdi, r12, r13
    EPILOG 320

.create:
    mov r14, [r13]
    invoke SetWindowLongPtrW, rsi, GWLP_USERDATA, r14
    invoke calloc, 1, TV_SIZE
    mov r15, rax
    mov [r14+SS_DATA], r15
    mov dword [r15+TV_TXA+16], 1     ; defaults for new files: UTF-8, CRLF
    mov dword [r15+TV_TXA+20], 1
    mov dword [r15+TV_TXB+16], 1
    mov dword [r15+TV_TXB+20], 1
    mov dword [r15+TV_TXA+24], 1
    mov dword [r15+TV_TXB+24], 1
    mov qword [r15+TV_CUR], -1
    invoke pb_create, rsi
    invoke mk_btn, rsi, &s_b_prev, ID_BTN_PREV
    invoke mk_btn, rsi, &s_b_next, ID_BTN_NEXT
    invoke mk_btn, rsi, &s_b_clr, ID_BTN_CLR
    invoke mk_btn, rsi, &s_b_crl, ID_BTN_CRL
    invoke mk_btn, rsi, &s_b_svl, ID_BTN_SVL
    invoke mk_btn, rsi, &s_b_svr, ID_BTN_SVR
    invoke mk_btn, rsi, &s_b_aln, ID_BTN_ALN
    invoke mk_btn, rsi, &s_b_alc, ID_BTN_ALC
    invoke mk_btn, rsi, &s_b_xls, ID_BTN_XLS
    invoke icon_set, rsi, ID_BTN_PREV, &g_prev, &s_b_prev
    invoke icon_set, rsi, ID_BTN_NEXT, &g_next, &s_b_next
    invoke icon_set, rsi, ID_BTN_CLR, &g_copyr, &s_b_clr
    invoke icon_set, rsi, ID_BTN_CRL, &g_copyl, &s_b_crl
    invoke mk_chk, rsi, &s_c_case, ID_CHK_CASE, 0
    invoke mk_chk, rsi, &s_c_spc, ID_CHK_SPC, 0
    invoke mk_chk, rsi, &s_c_trim, ID_CHK_TRIM, 0
    invoke mk_chk, rsi, &s_c_cmt, ID_CHK_CMT, 0
    invoke mk_edit, rsi, ID_EDIT_CMT
    invoke GetDlgItem, rsi, ID_EDIT_CMT
    invoke SetWindowTextW, rax, &s_prefix
    invoke mk_chk, rsi, &s_c_syn, ID_CHK_SYN, 1
    invoke mk_lbl, rsi, &s_ed_lfind, ID_ED_LFIND
    invoke mk_edit, rsi, ID_ED_FIND
    invoke mk_btn, rsi, &s_ed_next, ID_ED_NEXT
    invoke mk_btn, rsi, &s_ed_prev, ID_ED_PREV
    invoke mk_lbl, rsi, &s_ed_lrepl, ID_ED_LREPL
    invoke mk_edit, rsi, ID_ED_REPL
    invoke mk_btn, rsi, &s_ed_rone, ID_ED_RONE
    invoke mk_btn, rsi, &s_ed_rall, ID_ED_RALL
    invoke mk_chk, rsi, &s_ed_case, ID_ED_CASE, 0
    invoke mk_chk, rsi, &s_c_ids, ID_CHK_IDS, 0
    invoke mk_lbl, rsi, &s_l_rules, ID_LBL_RULES
    invoke mk_edit, rsi, ID_EDIT_RULES
    invoke mk_lbl, rsi, &s_info0, ID_LBL_INFO
    invoke rich_create, rsi, ID_RA
    mov [r15+TV_HA], rax
    invoke rich_create, rsi, ID_RB
    mov [r15+TV_HB], rax
    mov qword [r15+TV_FORCE], -1
    mov [r15+TV_GT0], r15
    mov qword [r15+TV_GT0+8], 0
    mov [r15+TV_GT0+16], rsi
    mov [r15+TV_GT1], r15
    mov qword [r15+TV_GT1+8], 1
    mov [r15+TV_GT1+16], rsi
    invoke CreateWindowExW, 0, &cn_gutter, 0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 10, 10, rsi, ID_GUL, [g_hinst], &r15+TV_GT0
    mov [r15+TV_HGL], rax
    invoke CreateWindowExW, 0, &cn_gutter, 0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 10, 10, rsi, ID_GUR, [g_hinst], &r15+TV_GT1
    mov [r15+TV_HGR], rax
    invoke CreateWindowExW, 0, &cn_mini, 0, WS_CHILD|WS_VISIBLE|WS_CLIPSIBLINGS, 0, 0, 10, 10, rsi, ID_MINI, [g_hinst], r15
    mov [r15+TV_HMM], rax
    invoke pb_load, rsi, r14
    invoke tv_opts_apply, rsi, r14
    invoke tv_theme, rsi, r15
    xor eax, eax
    EPILOG 320

.erase:
    invoke GetClientRect, rsi, &rsp+96
    invoke FillRect, r12, &rsp+96, [th_panelbrush]
    mov eax, 1
    EPILOG 320

.size:
    invoke tv_layout, rsi
    jmp .zero

.run:
    invoke text_compare, rsi, r15, r14
    jmp .zero
.redraw:
    invoke tv_redraw_aux, r15
    jmp .zero
.collect:
    invoke tv_collect, rsi, r14, r15
    jmp .zero
.saveall:
    invoke SendMessageW, [r15+TV_HA], EM_GETMODIFY, 0, 0
    test eax, eax
    jz .sv2
    invoke tv_save, rsi, r15, 0
.sv2:
    invoke SendMessageW, [r15+TV_HB], EM_GETMODIFY, 0, 0
    test eax, eax
    jz .zero
    invoke tv_save, rsi, r15, 1
    jmp .zero
.theme:
    invoke tv_theme, rsi, r15
    cmp dword [r15+TV_LDA], 0
    je .zero
    invoke text_compare, rsi, r15, r14
    jmp .zero

.command:
    movzx ebx, r12w
    mov eax, r12d
    shr eax, 16
    mov ebp, eax
    cmp ebx, ID_RA
    je .rich
    cmp ebx, ID_RB
    je .rich
    cmp ebx, ID_COMPARE
    je .run
    cmp ebx, ID_BROWSE_A
    je .browse
    cmp ebx, ID_BROWSE_B
    je .browse
    cmp ebx, ID_SWAP
    je .browse
    cmp ebx, ID_BTN_PREV
    je .b_prev
    cmp ebx, ID_BTN_NEXT
    je .b_next
    cmp ebx, ID_BTN_CLR
    je .b_clr
    cmp ebx, ID_BTN_CRL
    je .b_crl
    cmp ebx, ID_BTN_SVL
    je .b_svl
    cmp ebx, ID_BTN_SVR
    je .b_svr
    cmp ebx, ID_BTN_ALN
    je .b_aln
    cmp ebx, ID_BTN_ALC
    je .b_alc
    cmp ebx, ID_BTN_XLS
    je .b_xls
    cmp ebx, ID_CHK_SYN
    je .b_syn
    cmp ebx, ID_ED_NEXT
    je .f_next
    cmp ebx, ID_ED_PREV
    je .f_prev
    cmp ebx, ID_ED_RONE
    je .f_rep
    cmp ebx, ID_ED_RALL
    je .f_all
    cmp ebx, ID_CHK_IDS
    je .doreq
    cmp ebx, ID_CHK_CASE
    jb .def
    cmp ebx, ID_CHK_CMT
    ja .def
.doreq:
    cmp dword [r15+TV_LDA], 0
    je .zero
    invoke text_compare, rsi, r15, r14
    jmp .zero
.browse:
    invoke pb_command, rsi, rbx, 0
    jmp .zero
.b_syn:
    invoke tv_syn_apply, rsi, r15
    jmp .zero
.m_bm:
    invoke tv_bm_toggle, rsi, r15
    mov eax, 1
    EPILOG 320
.m_next:
    invoke tv_bm_step, rsi, r15, 1
    mov eax, 1
    EPILOG 320
.m_prev:
    invoke tv_bm_step, rsi, r15, 0
    mov eax, 1
    EPILOG 320
.f_next:
    invoke tv_find_cmd, rsi, r15, 0
    jmp .zero
.f_prev:
    invoke tv_find_cmd, rsi, r15, 1
    jmp .zero
.f_rep:
    invoke tv_find_cmd, rsi, r15, 2
    jmp .zero
.f_all:
    invoke tv_find_cmd, rsi, r15, 3
    jmp .zero
.b_prev:
    invoke tv_step, rsi, r15, 0
    jmp .zero
.b_next:
    invoke tv_step, rsi, r15, 1
    jmp .zero
.b_clr:
    invoke tv_copy, rsi, r15, r14, 0
    jmp .zero
.b_crl:
    invoke tv_copy, rsi, r15, r14, 1
    jmp .zero
.b_svl:
    invoke tv_save, rsi, r15, 0
    jmp .zero
.b_svr:
    invoke tv_save, rsi, r15, 1
    jmp .zero
.b_aln:
    invoke tv_align_set, rsi, r15, r14
    jmp .zero
.b_alc:
    invoke tv_align_clear, rsi, r15, r14
    jmp .zero
.b_xls:
    invoke text_export, rsi, r15
    jmp .zero
.rich:
    cmp ebp, EN_VSCROLL
    jne .zero
    invoke PostMessageW, rsi, WM_APP+50, 0, 0
    cmp qword [r15+TV_GUARD], 0
    jne .zero
    mov rdi, [r15+TV_HA]
    mov rbp, [r15+TV_HB]
    cmp ebx, ID_RA
    je .sync
    xchg rdi, rbp
.sync:
    invoke SendMessageW, rdi, EM_GETFIRSTVISIBLELINE, 0, 0
    mov ebx, eax
    invoke SendMessageW, rbp, EM_GETFIRSTVISIBLELINE, 0, 0
    sub ebx, eax
    jz .zero
    movsxd rbx, ebx
    mov qword [r15+TV_GUARD], 1
    invoke SendMessageW, rbp, EM_LINESCROLL, 0, rbx
    mov qword [r15+TV_GUARD], 0
    jmp .zero

.notify:
    mov eax, [r13+NMH_CODE]
    cmp eax, 0x0702                  ; EN_SELCHANGE
    jne .def
    mov rax, [r13+NMH_ID]
    xor ecx, ecx
    cmp eax, ID_RB
    jne .sl
    mov ecx, 1
.sl:
    mov [r15+TV_LASTP], rcx
    jmp .zero

.cedit:
    invoke ctl_color, r12, 0
    EPILOG 320
.cstatic:
    invoke ctl_color, r12, 1
    EPILOG 320
.focus:
    invoke SetFocus, [r15+TV_HA]
    jmp .zero
.isdirty:
    invoke SendMessageW, [r15+TV_HA], EM_GETMODIFY, 0, 0
    mov ebx, eax
    invoke SendMessageW, [r15+TV_HB], EM_GETMODIFY, 0, 0
    or eax, ebx
    EPILOG 320
.destroy:
    invoke free, [r15+TV_ROWS]
    invoke free, [r15+TV_BLK]
    invoke free, [r15+TV_BUFA]
    invoke free, [r15+TV_BUFB]
    invoke free, [r15+TV_TXA]
    invoke free, [r15+TV_TXB]
    invoke free, r15
    mov qword [r14+SS_DATA], 0
.zero:
    xor eax, eax
    EPILOG 320

; tv_find_cmd(hwnd, tv, kind 0 next / 1 previous / 2 replace / 3 replace all): find / replace in the last focused pane
tv_find_cmd:
    PROLOG 192
    mov rsi, rcx
    mov rbx, rdx
    mov r12d, r8d
    mov rax, [rbx+TV_HA]
    cmp qword [rbx+TV_LASTP], 0
    je .have
    mov rax, [rbx+TV_HB]
.have:
    mov [rsp+96+E_HR], rax
    cmp r12d, 1
    ja .rep
    mov r8d, 1
    test r12d, r12d
    jz .go
    xor r8d, r8d
.go:
    invoke ed_find, rsi, &rsp+96, r8
    EPILOG 192
.rep:
    lea r8d, [r12-2]
    invoke ed_replace, rsi, &rsp+96, r8
    EPILOG 192
