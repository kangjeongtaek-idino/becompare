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
WSTR s_b_xls, "내보내기…"
WSTR s_tx_title, "텍스트 비교 결과"
WSTR s_tx_defname, "텍스트비교결과"
WSTR f_tx_u, "%u"
tx_stat: dq s_tx_same, s_tx_chg, s_tx_lonly, s_tx_ronly
tx_widths: dd 10, 8, 70, 8, 70
tx_docx_widths: dd 1200, 1000, 6400, 1000, 6400
tx_doc_fill: dq 0, dwc_orange, dwc_red, dwc_green

WSTR s_tx_th, `<tr><th>상태</th><th>왼쪽 줄</th><th>왼쪽 내용</th><th>오른쪽 줄</th><th>오른쪽 내용</th></tr>\n`
WSTR s_tr_same, `<tr class="same">`
WSTR s_tr_chg, `<tr class="diff">`
WSTR s_tr_lonly, `<tr class="lonly">`
WSTR s_tr_ronly, `<tr class="ronly">`
WSTR s_td_start, `<td>`
WSTR s_td_num, `<td class="num">`
WSTR s_td_end, `</td>`
WSTR s_tr_end, `</tr>\n`
tx_htm_tr: dq s_tr_same, s_tr_chg, s_tr_lonly, s_tr_ronly

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
    cmp r12, XW_MAXROWS               ; a sheet holds 1,048,576 rows; more makes Excel reject the file
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

; tx_seg_docx(ctx, cursor*, fill, bold)
tx_seg_docx:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov r12, r8
    mov r13d, r9d
    mov rdi, [rsi]
    xor r14, r14
    xor r15d, r15d
    test rdi, rdi
    jz .null
    mov r14, rdi
.f:
    movzx eax, word [r14]
    test eax, eax
    jz .e
    cmp eax, 13
    je .e
    add r14, 2
    jmp .f
.e:
    movzx r15d, word [r14]
    mov word [r14], 0
    cmp word [rdi], FILLER
    jne .go
    cmp word [rdi+2], 0
    jne .go
.null:
    lea rdi, [s_empty]
.go:
    invoke dw_cell, rbx, rdi, r12, r13
    test r14, r14
    jz .d
    mov [r14], r15w
    test r15d, r15d
    jz .nx
    add r14, 2
.nx:
    mov [rsi], r14
.d:
    EPILOG 64

; tx_seg_html(sb, cursor*)
tx_seg_html:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov rdi, [rsi]
    xor r14, r14
    xor r15d, r15d
    test rdi, rdi
    jz .null
    mov r14, rdi
.f:
    movzx eax, word [r14]
    test eax, eax
    jz .e
    cmp eax, 13
    je .e
    add r14, 2
    jmp .f
.e:
    movzx r15d, word [r14]
    mov word [r14], 0
    cmp word [rdi], FILLER
    jne .go
    cmp word [rdi+2], 0
    jne .go
.null:
    lea rdi, [s_empty]
.go:
    invoke sb_append_esc, rbx, rdi
    test r14, r14
    jz .d
    mov [r14], r15w
    test r15d, r15d
    jz .nx
    add r14, 2
.nx:
    mov [rsi], r14
.d:
    EPILOG 64

; text_docx_path(tv, path) -> eax 1 when written
text_docx_path:
    PROLOG 320
    mov rbx, rcx
    mov rdi, rdx
    invoke dw_begin, &rsp+TX_XL
    invoke dw_para, &rsp+TX_XL, &s_tx_title, 32, 1
    invoke dw_tbl_begin, &rsp+TX_XL, &tx_docx_widths, 5
    invoke dw_row_begin, &rsp+TX_XL, 1
    invoke dw_cell, &rsp+TX_XL, &s_tx_h0, &dwc_gray, 1
    invoke dw_cell, &rsp+TX_XL, &s_tx_h1, &dwc_gray, 1
    invoke dw_cell, &rsp+TX_XL, &s_tx_h2, &dwc_gray, 1
    invoke dw_cell, &rsp+TX_XL, &s_tx_h3, &dwc_gray, 1
    invoke dw_cell, &rsp+TX_XL, &s_tx_h4, &dwc_gray, 1
    invoke dw_row_end, &rsp+TX_XL
    mov rax, [rbx+TV_BUFA]
    mov [rsp+TX_PA], rax
    mov rax, [rbx+TV_BUFB]
    mov [rsp+TX_PB], rax
    xor r12d, r12d
.l:
    cmp r12, [rbx+TV_NROWS]
    jae .fin
    cmp r12, 50000
    jae .fin
    mov rax, r12
    shl rax, 4
    add rax, [rbx+TV_ROWS]
    mov r13, rax
    mov eax, [r13]
    and eax, 3
    lea rcx, [tx_doc_fill]
    mov r14, [rcx+rax*8]
    invoke dw_row_begin, &rsp+TX_XL, 0
    lea rcx, [tx_stat]
    mov rdx, [rcx+rax*8]
    invoke dw_cell, &rsp+TX_XL, rdx, r14, 0
    mov word [rsp+TX_BUF], 0
    mov eax, [r13+4]
    cmp eax, -1
    je .la
    inc eax
    invoke _snwprintf, &rsp+TX_BUF, 32, &f_tx_u, rax
.la:
    invoke dw_cell, &rsp+TX_XL, &rsp+TX_BUF, r14, 0
    invoke tx_seg_docx, &rsp+TX_XL, &rsp+TX_PA, r14, 0
    mov word [rsp+TX_BUF], 0
    mov eax, [r13+8]
    cmp eax, -1
    je .lb
    inc eax
    invoke _snwprintf, &rsp+TX_BUF, 32, &f_tx_u, rax
.lb:
    invoke dw_cell, &rsp+TX_XL, &rsp+TX_BUF, r14, 0
    invoke tx_seg_docx, &rsp+TX_XL, &rsp+TX_PB, r14, 0
    invoke dw_row_end, &rsp+TX_XL
    inc r12
    jmp .l
.fin:
    invoke dw_tbl_end, &rsp+TX_XL
    invoke dw_save, &rsp+TX_XL, rdi
    mov ebx, eax
    invoke sb_free, &rsp+TX_XL
    mov eax, ebx
    EPILOG 320

; text_html_path(tv, path) -> eax 1 when written
text_html_path:
    PROLOG 320
    mov rbx, rcx
    mov rdi, rdx
    invoke sb_init, &rsp+TX_XL
    invoke sb_append, &rsp+TX_XL, &s_htm_doctype
    invoke sb_append, &rsp+TX_XL, &s_tx_title
    invoke sb_append, &rsp+TX_XL, &s_htm_style
    invoke sb_append, &rsp+TX_XL, &s_tx_title
    invoke sb_append, &rsp+TX_XL, &s_htm_tbl_begin
    invoke sb_append, &rsp+TX_XL, &s_tx_th
    mov rax, [rbx+TV_BUFA]
    mov [rsp+TX_PA], rax
    mov rax, [rbx+TV_BUFB]
    mov [rsp+TX_PB], rax
    xor r12d, r12d
.l:
    cmp r12, [rbx+TV_NROWS]
    jae .fin
    cmp r12, 100000
    jae .fin
    mov rax, r12
    shl rax, 4
    add rax, [rbx+TV_ROWS]
    mov r13, rax
    mov eax, [r13]
    and eax, 3
    lea rcx, [tx_htm_tr]
    mov rdx, [rcx+rax*8]
    invoke sb_append, &rsp+TX_XL, rdx
    invoke sb_append, &rsp+TX_XL, &s_td_start
    lea rcx, [tx_stat]
    mov rdx, [rcx+rax*8]
    invoke sb_append, &rsp+TX_XL, rdx
    invoke sb_append, &rsp+TX_XL, &s_td_end
    invoke sb_append, &rsp+TX_XL, &s_td_num
    mov eax, [r13+4]
    cmp eax, -1
    je .la
    inc eax
    invoke _snwprintf, &rsp+TX_BUF, 32, &f_tx_u, rax
    invoke sb_append, &rsp+TX_XL, &rsp+TX_BUF
.la:
    invoke sb_append, &rsp+TX_XL, &s_td_end
    invoke sb_append, &rsp+TX_XL, &s_td_start
    invoke tx_seg_html, &rsp+TX_XL, &rsp+TX_PA
    invoke sb_append, &rsp+TX_XL, &s_td_end
    invoke sb_append, &rsp+TX_XL, &s_td_num
    mov eax, [r13+8]
    cmp eax, -1
    je .lb
    inc eax
    invoke _snwprintf, &rsp+TX_BUF, 32, &f_tx_u, rax
    invoke sb_append, &rsp+TX_XL, &rsp+TX_BUF
.lb:
    invoke sb_append, &rsp+TX_XL, &s_td_end
    invoke sb_append, &rsp+TX_XL, &s_td_start
    invoke tx_seg_html, &rsp+TX_XL, &rsp+TX_PB
    invoke sb_append, &rsp+TX_XL, &s_td_end
    invoke sb_append, &rsp+TX_XL, &s_tr_end
    inc r12
    jmp .l
.fin:
    invoke sb_append, &rsp+TX_XL, &s_htm_tbl_end
    invoke write_utf8, rdi, [rsp+TX_XL], [rsp+TX_XL+8], 1
    mov ebx, eax
    invoke sb_free, &rsp+TX_XL
    mov eax, ebx
    EPILOG 320

; text_export(hwnd, tv)
text_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    invoke tv_unfold, rsi, rbx            ; the export is always the full comparison
    cmp qword [rbx+TV_NROWS], 0
    je .norun
    invoke ex_menu_choose, rsi, ID_BTN_XLS, &s_tx_defname, &rsp+400
    test eax, eax
    jz .d
    cmp eax, 1
    je .xlsx
    cmp eax, 2
    je .docx
    invoke text_html_path, rbx, &rsp+400
    jmp .res
.xlsx:
    invoke text_xlsx_path, rbx, &rsp+400
    jmp .res
.docx:
    invoke text_docx_path, rbx, &rsp+400
.res:
    invoke ex_done_open, rsi, rax, &rsp+400
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &s_tx_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
