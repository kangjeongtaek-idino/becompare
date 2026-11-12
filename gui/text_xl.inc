; text_xl.inc - text compare result -> Excel workbook (.xlsx): one row per aligned row of the compare view

section .rdata
WSTR s_tx_h0, "상태"
WSTR s_tx_h1, "왼쪽 줄"
WSTR s_tx_h2, "왼쪽 내용"
WSTR s_tx_h3, "오른쪽 줄"
WSTR s_tx_h4, "오른쪽 내용"
WSTR s_tx_same, "같음"
WSTR s_tx_chg, "변경"
WSTR s_tx_lonly, "왼쪽만"
WSTR s_tx_ronly, "오른쪽만"
WSTR s_tx_norun, "먼저 비교를 실행하세요."
WSTR s_b_xls, "엑셀 저장"
WSTR f_tx_u, "%u"
tx_stat: dq s_tx_same, s_tx_chg, s_tx_lonly, s_tx_ronly
tx_widths: dd 10, 8, 70, 8, 70

section .text

; tx_seg(ctx, cursor*, style): emit the next CR-delimited segment of a pane buffer as a cell and advance the cursor
tx_seg:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov r12d, r8d
    mov rdi, [rsi]
    xor r13d, r13d
    xor r14d, r14d
    test rdi, rdi
    jz .null
    mov r13, rdi
.f:
    movzx eax, word [r13]
    test eax, eax
    jz .e
    cmp eax, 13
    je .e
    add r13, 2
    jmp .f
.e:
    movzx r14d, word [r13]
    mov word [r13], 0
    cmp word [rdi], FILLER            ; a lone filler stands for "no line on this side"
    jne .go
    cmp word [rdi+2], 0
    jne .go
.null:
    lea rdi, [s_empty]
.go:
    invoke xw_cell, rbx, rdi, r12
    test r13, r13
    jz .d
    mov [r13], r14w
    test r14d, r14d
    jz .nx
    add r13, 2
.nx:
    mov [rsi], r13
.d:
    EPILOG 64

%define TX_XL  96                    ; XL ctx (40)
%define TX_BUF 144                   ; wchar[32]
%define TX_PA  208                   ; cursor A
%define TX_PB  216                   ; cursor B

; text_xlsx_path(tv, path) -> eax 1 when written
text_xlsx_path:
    PROLOG 320
    mov rbx, rcx
    mov rdi, rdx
    invoke xw_begin, &rsp+TX_XL
    invoke xw_cell, &rsp+TX_XL, &s_tx_h0, XS_HEAD
    invoke xw_cell, &rsp+TX_XL, &s_tx_h1, XS_HEAD
    invoke xw_cell, &rsp+TX_XL, &s_tx_h2, XS_HEAD
    invoke xw_cell, &rsp+TX_XL, &s_tx_h3, XS_HEAD
    invoke xw_cell, &rsp+TX_XL, &s_tx_h4, XS_HEAD
    invoke xw_endrow, &rsp+TX_XL
    mov rax, [rbx+TV_BUFA]
    mov [rsp+TX_PA], rax
    mov rax, [rbx+TV_BUFB]
    mov [rsp+TX_PB], rax
    xor r12d, r12d
.l:
    cmp r12, [rbx+TV_NROWS]
    jae .fin
    mov rax, r12
    shl rax, 4
    add rax, [rbx+TV_ROWS]
    mov r13, rax
    mov eax, [r13]                   ; 0 same, 1 changed, 2 only left, 3 only right
    and eax, 3
    mov r14d, XS_MONO                ; left style
    mov r15d, XS_MONO                ; right style
    cmp eax, 1
    jne .n1
    mov r14d, XS_MONOORANGE
    mov r15d, XS_MONOORANGE
.n1:
    cmp eax, 2
    jne .n2
    mov r14d, XS_MONORED
.n2:
    cmp eax, 3
    jne .n3
    mov r15d, XS_MONOGREEN
.n3:
    lea rcx, [tx_stat]
    mov rdx, [rcx+rax*8]
    invoke xw_cell, &rsp+TX_XL, rdx, XS_NORMAL
    mov word [rsp+TX_BUF], 0
    mov eax, [r13+4]
    cmp eax, -1
    je .la
    inc eax
    invoke _snwprintf, &rsp+TX_BUF, 32, &f_tx_u, rax
.la:
    invoke xw_numcell, &rsp+TX_XL, &rsp+TX_BUF, XS_NORMAL
    invoke tx_seg, &rsp+TX_XL, &rsp+TX_PA, r14
    mov word [rsp+TX_BUF], 0
    mov eax, [r13+8]
    cmp eax, -1
    je .lb
    inc eax
    invoke _snwprintf, &rsp+TX_BUF, 32, &f_tx_u, rax
.lb:
    invoke xw_numcell, &rsp+TX_XL, &rsp+TX_BUF, XS_NORMAL
    invoke tx_seg, &rsp+TX_XL, &rsp+TX_PB, r15
    invoke xw_endrow, &rsp+TX_XL
    inc r12
    jmp .l
.fin:
    invoke xw_save, &rsp+TX_XL, rdi, &tx_widths, 5
    mov ebx, eax
    invoke sb_free, &rsp+TX_XL
    mov eax, ebx
    EPILOG 320

; text_export(hwnd, tv)
text_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+TV_NROWS], 0
    je .norun
    invoke ex_pick, rsi, &rsp+400, 0
    test eax, eax
    jz .d
    invoke text_xlsx_path, rbx, &rsp+400
    invoke ex_done, rsi, rax
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &s_tx_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
