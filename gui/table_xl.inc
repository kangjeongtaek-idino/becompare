; table_xl.inc - table compare result -> Excel workbook (.xlsx): the rows shown in the list (honours "차이만 표시")

section .rdata
WSTR s_tb_hstat, "상태"
WSTR s_tb_same, "같음"
WSTR s_tb_chg, "변경"
WSTR s_tb_lonly, "왼쪽만"
WSTR s_tb_ronly, "오른쪽만"
WSTR s_tb_arrow, " → "
WSTR s_tb_norun, "먼저 비교를 실행하세요."
WSTR s_b_xls, "내보내기…"
WSTR s_tb_title, "표 비교 결과"
WSTR s_tb_defname, "표비교결과"
tb_stat: dq s_tb_same, s_tb_chg, s_tb_lonly, s_tb_ronly
tb_doc_fill: dq 0, dwc_orange, dwc_red, dwc_green

WSTR s_tb_th1, `<tr><th>상태</th><th>`
WSTR s_tb_th2, `</th>`
WSTR s_tb_th3, `<th>`
WSTR s_tb_the, `</th></tr>\n`
WSTR s_tr_same, `<tr class="same">`
WSTR s_tr_chg, `<tr class="diff">`
WSTR s_tr_lonly, `<tr class="lonly">`
WSTR s_tr_ronly, `<tr class="ronly">`
WSTR s_td_start, `<td>`
WSTR s_td_num, `<td class="num">`
WSTR s_td_diff, `<td class="diff">`
WSTR s_td_lonly, `<td class="lonly">`
WSTR s_td_ronly, `<td class="ronly">`
WSTR s_td_end, `</td>`
WSTR s_tr_end, `</tr>\n`
tb_htm_tr: dq s_tr_same, s_tr_chg, s_tr_lonly, s_tr_ronly

section .text

%define BX_XL   96                   ; XL ctx (40)
%define BX_SB   136                  ; sb (24)
%define BX_W    160                  ; widths ptr
%define BX_PATH 168
%define BX_BUF  176                  ; wchar[32]

; tb_xlsx_path(tb, path) -> eax 1 when written
tb_xlsx_path:
    PROLOG 320
    mov rbx, rcx
    mov [rsp+BX_PATH], rdx
    mov rax, [rbx+TB_NCOLS]
    lea rcx, [rax*4+16]
    call malloc
    mov [rsp+BX_W], rax
    test rax, rax
    jz .fail
    mov dword [rax], 10
    mov dword [rax+4], 10
    mov rcx, [rbx+TB_NCOLS]
    xor edx, edx
.w:
    cmp rdx, rcx
    jae .wd
    mov dword [rax+rdx*4+8], 22
    inc rdx
    jmp .w
.wd:
    invoke xw_begin, &rsp+BX_XL
    invoke xw_cell, &rsp+BX_XL, &s_tb_hstat, XS_HEAD
    invoke xw_cell, &rsp+BX_XL, &s_col0, XS_HEAD
    xor r14d, r14d
.hc:
    cmp r14, [rbx+TB_NCOLS]
    jae .hd
    mov rax, [rbx+TB_COLMAP]
    mov r15d, [rax+r14*4]            ; source column
    lea rsi, [rsp+BX_BUF]
    test dword [rbx+TB_FLAGS], 1
    jz .fb
    cmp qword [rbx+TB_SA+SD_NROWS], 0
    je .fb
    invoke side_cell, &rbx+TB_SA, 0, r15
    cmp word [rax], 0
    je .fb
    mov rsi, rax
    jmp .hput
.fb:
    lea eax, [r15+1]
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_col, rax
.hput:
    invoke xw_cell, &rsp+BX_XL, rsi, XS_HEAD
    inc r14
    jmp .hc
.hd:
    invoke xw_endrow, &rsp+BX_XL
    xor r12d, r12d
.row:
    cmp r12, [rbx+TB_NVIS]
    jae .fin
    cmp r12, XW_MAXROWS               ; a sheet holds 1,048,576 rows; more makes Excel reject the file
    jae .fin
    mov rcx, [rbx+TB_VIS]
    mov eax, [rcx+r12*4]
    imul rax, R_SIZE
    add rax, [rbx+TB_RES]
    mov r13, rax
    mov eax, [r13+R_ST]
    xor ebp, ebp                      ; row style
    xor ecx, ecx                      ; status text index
    cmp eax, 2
    jne .s3
    mov ebp, XS_RED
    mov ecx, 2
    jmp .sdone
.s3:
    cmp eax, 3
    jne .sc
    mov ebp, XS_GREEN
    mov ecx, 3
    jmp .sdone
.sc:
    cmp dword [r13+R_NCHG], 0
    je .sdone
    mov ecx, 1
.sdone:
    lea rdx, [tb_stat]
    mov rdx, [rdx+rcx*8]
    invoke xw_cell, &rsp+BX_XL, rdx, rbp
    ; row label like the list's first column
    mov eax, [r13+R_ST]
    mov r14d, [r13+R_A]
    inc r14d
    mov r15d, [r13+R_B]
    inc r15d
    cmp eax, 2
    je .la
    cmp eax, 3
    je .lb
    cmp r14d, r15d
    je .l1
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_row2, r14, r15
    jmp .lput
.l1:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_row1, r14
    jmp .lput
.la:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_rowa, r14
    jmp .lput
.lb:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_rowb, r15
.lput:
    invoke xw_cell, &rsp+BX_XL, &rsp+BX_BUF, rbp
    xor r14d, r14d                    ; displayed column
.col:
    cmp r14, [rbx+TB_NCOLS]
    jae .rowend
    mov rax, [rbx+TB_COLMAP]
    mov r15d, [rax+r14*4]
    mov eax, [r13+R_ST]
    cmp eax, 2
    je .onlya
    cmp eax, 3
    je .onlyb
    invoke side_cell, &rbx+TB_SA, #[r13+R_A], r15
    mov rsi, rax
    invoke side_cell, &rbx+TB_SB, #[r13+R_B], r15
    mov rdi, rax
    invoke cell_differs, rbx, rsi, rdi
    test eax, eax
    jz .plain
    invoke sb_init, &rsp+BX_SB
    invoke sb_append, &rsp+BX_SB, rsi
    invoke sb_append, &rsp+BX_SB, &s_tb_arrow
    invoke sb_append, &rsp+BX_SB, rdi
    invoke xw_cell, &rsp+BX_XL, [rsp+BX_SB], XS_ORANGE
    invoke sb_free, &rsp+BX_SB
    jmp .cnext
.plain:
    invoke xw_cell, &rsp+BX_XL, rsi, XS_NORMAL
    jmp .cnext
.onlya:
    invoke side_cell, &rbx+TB_SA, #[r13+R_A], r15
    invoke xw_cell, &rsp+BX_XL, rax, rbp
    jmp .cnext
.onlyb:
    invoke side_cell, &rbx+TB_SB, #[r13+R_B], r15
    invoke xw_cell, &rsp+BX_XL, rax, rbp
.cnext:
    inc r14
    jmp .col
.rowend:
    invoke xw_endrow, &rsp+BX_XL
    inc r12
    jmp .row
.fin:
    mov rax, [rbx+TB_NCOLS]
    add eax, 2
    invoke xw_save, &rsp+BX_XL, [rsp+BX_PATH], [rsp+BX_W], rax
    mov ebx, eax
    invoke sb_free, &rsp+BX_XL
    invoke free, [rsp+BX_W]
    mov eax, ebx
    EPILOG 320
.fail:
    xor eax, eax
    EPILOG 320

; tb_docx_path(tb, path) -> eax 1 when written
tb_docx_path:
    PROLOG 320
    mov rbx, rcx
    mov [rsp+BX_PATH], rdx
    mov rax, [rbx+TB_NCOLS]
    lea rcx, [rax*4+16]
    call malloc
    mov [rsp+BX_W], rax
    test rax, rax
    jz .fail
    mov dword [rax], 1200
    mov dword [rax+4], 1000
    mov rcx, [rbx+TB_NCOLS]
    xor edx, edx
.w:
    cmp rdx, rcx
    jae .wd
    mov dword [rax+rdx*4+8], 2200
    inc rdx
    jmp .w
.wd:
    invoke dw_begin, &rsp+BX_XL
    invoke dw_para, &rsp+BX_XL, &s_tb_title, 32, 1
    mov rax, [rbx+TB_NCOLS]
    add eax, 2
    invoke dw_tbl_begin, &rsp+BX_XL, [rsp+BX_W], rax
    invoke dw_row_begin, &rsp+BX_XL, 1
    invoke dw_cell, &rsp+BX_XL, &s_tb_hstat, &dwc_gray, 1
    invoke dw_cell, &rsp+BX_XL, &s_col0, &dwc_gray, 1
    xor r14d, r14d
.hc:
    cmp r14, [rbx+TB_NCOLS]
    jae .hd
    mov rax, [rbx+TB_COLMAP]
    mov r15d, [rax+r14*4]
    lea rsi, [rsp+BX_BUF]
    test dword [rbx+TB_FLAGS], 1
    jz .fb
    cmp qword [rbx+TB_SA+SD_NROWS], 0
    je .fb
    invoke side_cell, &rbx+TB_SA, 0, r15
    cmp word [rax], 0
    je .fb
    mov rsi, rax
    jmp .hput
.fb:
    lea eax, [r15+1]
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_col, rax
.hput:
    invoke dw_cell, &rsp+BX_XL, rsi, &dwc_gray, 1
    inc r14
    jmp .hc
.hd:
    invoke dw_row_end, &rsp+BX_XL
    xor r12d, r12d
.row:
    cmp r12, [rbx+TB_NVIS]
    jae .fin
    cmp r12, 50000
    jae .fin
    mov rcx, [rbx+TB_VIS]
    mov eax, [rcx+r12*4]
    imul rax, R_SIZE
    add rax, [rbx+TB_RES]
    mov r13, rax
    mov eax, [r13+R_ST]
    xor ebp, ebp
    xor ecx, ecx
    cmp eax, 2
    jne .s3
    lea rbp, [dwc_red]
    mov ecx, 2
    jmp .sdone
.s3:
    cmp eax, 3
    jne .sc
    lea rbp, [dwc_green]
    mov ecx, 3
    jmp .sdone
.sc:
    cmp dword [r13+R_NCHG], 0
    je .sdone
    lea rbp, [dwc_orange]
    mov ecx, 1
.sdone:
    invoke dw_row_begin, &rsp+BX_XL, 0
    lea rdx, [tb_stat]
    mov rdx, [rdx+rcx*8]
    invoke dw_cell, &rsp+BX_XL, rdx, rbp, 0
    mov eax, [r13+R_ST]
    mov r14d, [r13+R_A]
    inc r14d
    mov r15d, [r13+R_B]
    inc r15d
    cmp eax, 2
    je .la
    cmp eax, 3
    je .lb
    cmp r14d, r15d
    je .l1
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_row2, r14, r15
    jmp .lput
.l1:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_row1, r14
    jmp .lput
.la:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_rowa, r14
    jmp .lput
.lb:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_rowb, r15
.lput:
    invoke dw_cell, &rsp+BX_XL, &rsp+BX_BUF, rbp, 0
    xor r14d, r14d
.col:
    cmp r14, [rbx+TB_NCOLS]
    jae .rowend
    mov rax, [rbx+TB_COLMAP]
    mov r15d, [rax+r14*4]
    mov eax, [r13+R_ST]
    cmp eax, 2
    je .onlya
    cmp eax, 3
    je .onlyb
    invoke side_cell, &rbx+TB_SA, #[r13+R_A], r15
    mov rsi, rax
    invoke side_cell, &rbx+TB_SB, #[r13+R_B], r15
    mov rdi, rax
    invoke cell_differs, rbx, rsi, rdi
    test eax, eax
    jz .plain
    invoke sb_init, &rsp+BX_SB
    invoke sb_append, &rsp+BX_SB, rsi
    invoke sb_append, &rsp+BX_SB, &s_tb_arrow
    invoke sb_append, &rsp+BX_SB, rdi
    invoke dw_cell, &rsp+BX_XL, [rsp+BX_SB], &dwc_orange, 0
    invoke sb_free, &rsp+BX_SB
    jmp .cnext
.plain:
    invoke dw_cell, &rsp+BX_XL, rsi, 0, 0
    jmp .cnext
.onlya:
    invoke side_cell, &rbx+TB_SA, #[r13+R_A], r15
    invoke dw_cell, &rsp+BX_XL, rax, rbp, 0
    jmp .cnext
.onlyb:
    invoke side_cell, &rbx+TB_SB, #[r13+R_B], r15
    invoke dw_cell, &rsp+BX_XL, rax, rbp, 0
.cnext:
    inc r14
    jmp .col
.rowend:
    invoke dw_row_end, &rsp+BX_XL
    inc r12
    jmp .row
.fin:
    invoke dw_tbl_end, &rsp+BX_XL
    invoke dw_save, &rsp+BX_XL, [rsp+BX_PATH]
    mov ebx, eax
    invoke sb_free, &rsp+BX_XL
    invoke free, [rsp+BX_W]
    mov eax, ebx
    EPILOG 320
.fail:
    xor eax, eax
    EPILOG 320

; tb_html_path(tb, path) -> eax 1 when written
tb_html_path:
    PROLOG 320
    mov rbx, rcx
    mov [rsp+BX_PATH], rdx
    invoke sb_init, &rsp+BX_XL
    invoke sb_append, &rsp+BX_XL, &s_htm_doctype
    invoke sb_append, &rsp+BX_XL, &s_tb_title
    invoke sb_append, &rsp+BX_XL, &s_htm_style
    invoke sb_append, &rsp+BX_XL, &s_tb_title
    invoke sb_append, &rsp+BX_XL, &s_htm_tbl_begin
    invoke sb_append, &rsp+BX_XL, &s_tb_th1
    invoke sb_append, &rsp+BX_XL, &s_col0
    invoke sb_append, &rsp+BX_XL, &s_tb_th2
    xor r14d, r14d
.hc:
    cmp r14, [rbx+TB_NCOLS]
    jae .hd
    mov rax, [rbx+TB_COLMAP]
    mov r15d, [rax+r14*4]
    lea rsi, [rsp+BX_BUF]
    test dword [rbx+TB_FLAGS], 1
    jz .fb
    cmp qword [rbx+TB_SA+SD_NROWS], 0
    je .fb
    invoke side_cell, &rbx+TB_SA, 0, r15
    cmp word [rax], 0
    je .fb
    mov rsi, rax
    jmp .hput
.fb:
    lea eax, [r15+1]
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_col, rax
.hput:
    invoke sb_append, &rsp+BX_XL, &s_tb_th3
    invoke sb_append_esc, &rsp+BX_XL, rsi
    invoke sb_append, &rsp+BX_XL, &s_tb_th2
    inc r14
    jmp .hc
.hd:
    invoke sb_append, &rsp+BX_XL, &s_tr_end
    xor r12d, r12d
.row:
    cmp r12, [rbx+TB_NVIS]
    jae .fin
    cmp r12, 100000
    jae .fin
    mov rcx, [rbx+TB_VIS]
    mov eax, [rcx+r12*4]
    imul rax, R_SIZE
    add rax, [rbx+TB_RES]
    mov r13, rax
    mov eax, [r13+R_ST]
    xor ecx, ecx
    cmp eax, 2
    jne .s3
    mov ecx, 2
    jmp .sdone
.s3:
    cmp eax, 3
    jne .sc
    mov ecx, 3
    jmp .sdone
.sc:
    cmp dword [r13+R_NCHG], 0
    je .sdone
    mov ecx, 1
.sdone:
    lea rdx, [tb_htm_tr]
    mov rdx, [rdx+rcx*8]
    invoke sb_append, &rsp+BX_XL, rdx
    invoke sb_append, &rsp+BX_XL, &s_td_start
    lea rdx, [tb_stat]
    mov rdx, [rdx+rcx*8]
    invoke sb_append, &rsp+BX_XL, rdx
    invoke sb_append, &rsp+BX_XL, &s_td_end
    mov eax, [r13+R_ST]
    mov r14d, [r13+R_A]
    inc r14d
    mov r15d, [r13+R_B]
    inc r15d
    cmp eax, 2
    je .la
    cmp eax, 3
    je .lb
    cmp r14d, r15d
    je .l1
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_row2, r14, r15
    jmp .lput
.l1:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_row1, r14
    jmp .lput
.la:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_rowa, r14
    jmp .lput
.lb:
    invoke _snwprintf, &rsp+BX_BUF, 30, &f_rowb, r15
.lput:
    invoke sb_append, &rsp+BX_XL, &s_td_num
    invoke sb_append, &rsp+BX_XL, &rsp+BX_BUF
    invoke sb_append, &rsp+BX_XL, &s_td_end
    xor r14d, r14d
.col:
    cmp r14, [rbx+TB_NCOLS]
    jae .rowend
    mov rax, [rbx+TB_COLMAP]
    mov r15d, [rax+r14*4]
    mov eax, [r13+R_ST]
    cmp eax, 2
    je .onlya
    cmp eax, 3
    je .onlyb
    invoke side_cell, &rbx+TB_SA, #[r13+R_A], r15
    mov rsi, rax
    invoke side_cell, &rbx+TB_SB, #[r13+R_B], r15
    mov rdi, rax
    invoke cell_differs, rbx, rsi, rdi
    test eax, eax
    jz .plain
    invoke sb_append, &rsp+BX_XL, &s_td_diff
    invoke sb_append_esc, &rsp+BX_XL, rsi
    invoke sb_append, &rsp+BX_XL, &s_tb_arrow
    invoke sb_append_esc, &rsp+BX_XL, rdi
    invoke sb_append, &rsp+BX_XL, &s_td_end
    jmp .cnext
.plain:
    invoke sb_append, &rsp+BX_XL, &s_td_start
    invoke sb_append_esc, &rsp+BX_XL, rsi
    invoke sb_append, &rsp+BX_XL, &s_td_end
    jmp .cnext
.onlya:
    invoke sb_append, &rsp+BX_XL, &s_td_lonly
    invoke side_cell, &rbx+TB_SA, #[r13+R_A], r15
    invoke sb_append_esc, &rsp+BX_XL, rax
    invoke sb_append, &rsp+BX_XL, &s_td_end
    jmp .cnext
.onlyb:
    invoke sb_append, &rsp+BX_XL, &s_td_ronly
    invoke side_cell, &rbx+TB_SB, #[r13+R_B], r15
    invoke sb_append_esc, &rsp+BX_XL, rax
    invoke sb_append, &rsp+BX_XL, &s_td_end
.cnext:
    inc r14
    jmp .col
.rowend:
    invoke sb_append, &rsp+BX_XL, &s_tr_end
    inc r12
    jmp .row
.fin:
    invoke sb_append, &rsp+BX_XL, &s_htm_tbl_end
    invoke write_utf8, [rsp+BX_PATH], [rsp+BX_XL], [rsp+BX_XL+8], 1
    mov ebx, eax
    invoke sb_free, &rsp+BX_XL
    mov eax, ebx
    EPILOG 320

; tb_export(hwnd, tb)
tb_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+TB_COLMAP], 0
    je .norun
    invoke ex_menu_choose, rsi, ID_BTN_XLS, &s_tb_defname, &rsp+400
    test eax, eax
    jz .d
    cmp eax, 1
    je .xlsx
    cmp eax, 2
    je .docx
    invoke tb_html_path, rbx, &rsp+400
    jmp .res
.xlsx:
    invoke tb_xlsx_path, rbx, &rsp+400
    jmp .res
.docx:
    invoke tb_docx_path, rbx, &rsp+400
.res:
    invoke ex_done_open, rsi, rax, &rsp+400
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &s_tb_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
