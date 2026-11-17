; props_doc.inc - version / registry / media compare result -> Word document (.docx)

section .rdata
WSTR pe_t_ver, "파일 버전 비교 결과"
WSTR pe_t_reg, "레지스트리 비교 결과"
WSTR pe_t_med, "미디어 태그 비교 결과"
WSTR pe_f_a, "A (왼쪽): %s"
WSTR pe_f_b, "B (오른쪽): %s"
WSTR pe_h_item, "항목"
WSTR pe_h_a, "A (왼쪽)"
WSTR pe_h_b, "B (오른쪽)"
WSTR pe_h_st, "상태"
WSTR pe_norun, "먼저 비교를 실행하세요."
WSTR s_b_doc, "내보내기…"
WSTR pe_def_ver, "버전비교결과"
WSTR pe_def_reg, "레지스트리비교결과"
WSTR pe_def_med, "미디어비교결과"
pe_widths: dd 3400, 4900, 4900, 1300
pe_xl_widths: dd 25, 35, 35, 14
pe_xl_styles: dd XS_NORMAL, XS_ORANGE, XS_RED, XS_GREEN

WSTR s_pe_th, `<tr><th>항목</th><th>A (왼쪽)</th><th>B (오른쪽)</th><th>상태</th></tr>\n`
WSTR s_pe_td, `<td>`
WSTR s_pe_tde, `</td>`
WSTR s_pe_tre, `</tr>\n`
WSTR s_tr_diff, `<tr class="diff">`
WSTR s_tr_lonly, `<tr class="lonly">`
WSTR s_tr_ronly, `<tr class="ronly">`
WSTR s_tr_type, `<tr class="type">`
WSTR s_tr_same, `<tr class="same">`
WSTR s_pd_mid, `</h3><p>`
WSTR s_pd_endp, `</p>`
align 8
pe_htm_tr: dq s_tr_same, s_tr_type, s_tr_diff, s_tr_lonly

section .text

%define PE_DX   96                   ; DX ctx (40)
%define PE_PA   144                  ; wchar[520]
%define PE_PB   1184                 ; wchar[520]
%define PE_OUT  2224                 ; wchar[520]
%define PE_LINE 3264                 ; wchar[600]

; pp_docx_path(hwnd, P, sess, path) -> eax 1 when written
pp_docx_path:
    PROLOG 4528
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov r13, r9
    invoke dw_begin, &rsp+PE_DX
    lea rdx, [pe_t_med]
    mov rax, [r12+SS_TYPE]
    cmp rax, T_VER
    jne .nv
    lea rdx, [pe_t_ver]
.nv:
    cmp rax, T_REG
    jne .nr
    lea rdx, [pe_t_reg]
.nr:
    invoke dw_para, &rsp+PE_DX, rdx, 32, 1
    mov word [rsp+PE_PA], 0
    mov word [rsp+PE_PB], 0
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &rsp+PE_PA, 519
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke GetWindowTextW, rax, &rsp+PE_PB, 519
    invoke _snwprintf, &rsp+PE_LINE, 590, &pe_f_a, &rsp+PE_PA
    invoke dw_para, &rsp+PE_DX, &rsp+PE_LINE, 20, 0
    invoke _snwprintf, &rsp+PE_LINE, 590, &pe_f_b, &rsp+PE_PB
    invoke dw_para, &rsp+PE_DX, &rsp+PE_LINE, 20, 0
    mov word [rsp+PE_LINE], 0
    invoke GetDlgItem, rsi, ID_LBLI
    invoke GetWindowTextW, rax, &rsp+PE_LINE, 590
    invoke dw_para, &rsp+PE_DX, &rsp+PE_LINE, 20, 0
    invoke dw_tbl_begin, &rsp+PE_DX, &pe_widths, 4
    invoke dw_row_begin, &rsp+PE_DX, 1
    invoke dw_cell, &rsp+PE_DX, &pe_h_item, &dwc_gray, 1
    invoke dw_cell, &rsp+PE_DX, &pe_h_a, &dwc_gray, 1
    invoke dw_cell, &rsp+PE_DX, &pe_h_b, &dwc_gray, 1
    invoke dw_cell, &rsp+PE_DX, &pe_h_st, &dwc_gray, 1
    invoke dw_row_end, &rsp+PE_DX
    xor r14d, r14d
.row:
    cmp r14, [rbx+P_NVIS]
    jae .fin
    mov rcx, [rbx+P_VIS]
    mov eax, [rcx+r14*4]
    shl rax, 5
    add rax, [rbx+P_ROWS]
    mov r15, rax
    xor edi, edi                      ; fill
    mov eax, [r15+R_ST]
    cmp eax, 1
    jne .f2
    lea rdi, [dwc_orange]
.f2:
    cmp eax, 2
    jne .f3
    lea rdi, [dwc_red]
.f3:
    cmp eax, 3
    jne .f4
    lea rdi, [dwc_green]
.f4:
    invoke dw_row_begin, &rsp+PE_DX, 0
    invoke dw_cell, &rsp+PE_DX, [r15+R_NAME], 0, 0
    invoke dw_cell, &rsp+PE_DX, [r15+R_A], rdi, 0
    invoke dw_cell, &rsp+PE_DX, [r15+R_B], rdi, 0
    mov eax, [r15+R_ST]
    lea rcx, [st_names]
    mov rdx, [rcx+rax*8]
    invoke dw_cell, &rsp+PE_DX, rdx, rdi, 0
    invoke dw_row_end, &rsp+PE_DX
    inc r14
    jmp .row
.fin:
    invoke dw_tbl_end, &rsp+PE_DX
    invoke dw_save, &rsp+PE_DX, r13
    mov ebx, eax
    invoke sb_free, &rsp+PE_DX
    mov eax, ebx
    EPILOG 4528

; pp_xlsx_path(hwnd, P, sess, path) -> eax 1 when written
pp_xlsx_path:
    PROLOG 4528
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov r13, r9
    invoke xw_begin, &rsp+PE_DX
    invoke xw_cell, &rsp+PE_DX, &pe_h_item, XS_HEAD
    invoke xw_cell, &rsp+PE_DX, &pe_h_a, XS_HEAD
    invoke xw_cell, &rsp+PE_DX, &pe_h_b, XS_HEAD
    invoke xw_cell, &rsp+PE_DX, &pe_h_st, XS_HEAD
    invoke xw_endrow, &rsp+PE_DX
    xor r14d, r14d
.row:
    cmp r14, [rbx+P_NVIS]
    jae .fin
    cmp r14, XW_MAXROWS
    jae .fin
    mov rcx, [rbx+P_VIS]
    mov eax, [rcx+r14*4]
    shl rax, 5
    add rax, [rbx+P_ROWS]
    mov r15, rax
    mov eax, [r15+R_ST]
    lea rcx, [pe_xl_styles]
    mov edi, [rcx+rax*4]
    invoke xw_cell, &rsp+PE_DX, [r15+R_NAME], 0
    invoke xw_cell, &rsp+PE_DX, [r15+R_A], rdi
    invoke xw_cell, &rsp+PE_DX, [r15+R_B], rdi
    mov eax, [r15+R_ST]
    lea rcx, [st_names]
    mov rdx, [rcx+rax*8]
    invoke xw_cell, &rsp+PE_DX, rdx, rdi
    invoke xw_endrow, &rsp+PE_DX
    inc r14
    jmp .row
.fin:
    invoke xw_save, &rsp+PE_DX, r13, &pe_xl_widths, 4
    mov ebx, eax
    invoke sb_free, &rsp+PE_DX
    mov eax, ebx
    EPILOG 4528

; pp_html_path(hwnd, P, sess, path) -> eax 1 when written
pp_html_path:
    PROLOG 4528
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov r13, r9
    invoke sb_init, &rsp+PE_DX
    lea rdx, [pe_t_med]
    mov rax, [r12+SS_TYPE]
    cmp rax, T_VER
    jne .nv
    lea rdx, [pe_t_ver]
.nv:
    cmp rax, T_REG
    jne .nr
    lea rdx, [pe_t_reg]
.nr:
    mov rbp, rdx
    invoke sb_append, &rsp+PE_DX, &s_htm_doctype
    invoke sb_append, &rsp+PE_DX, rbp
    invoke sb_append, &rsp+PE_DX, &s_htm_style
    invoke sb_append, &rsp+PE_DX, rbp
    mov word [rsp+PE_PA], 0
    mov word [rsp+PE_PB], 0
    invoke GetDlgItem, rsi, ID_EDIT_A
    invoke GetWindowTextW, rax, &rsp+PE_PA, 519
    invoke GetDlgItem, rsi, ID_EDIT_B
    invoke GetWindowTextW, rax, &rsp+PE_PB, 519
    cmp word [rsp+PE_PA], 0
    je .nopath
    invoke _snwprintf, &rsp+PE_LINE, 590, &pe_f_a, &rsp+PE_PA
    invoke sb_append, &rsp+PE_DX, &s_pd_mid
    invoke sb_append_esc, &rsp+PE_DX, &rsp+PE_LINE
    invoke sb_append, &rsp+PE_DX, &s_pd_endp
    invoke _snwprintf, &rsp+PE_LINE, 590, &pe_f_b, &rsp+PE_PB
    invoke sb_append, &rsp+PE_DX, &s_pd_mid
    invoke sb_append_esc, &rsp+PE_DX, &rsp+PE_LINE
    invoke sb_append, &rsp+PE_DX, &s_pd_endp
.nopath:
    invoke sb_append, &rsp+PE_DX, &s_htm_tbl_begin
    invoke sb_append, &rsp+PE_DX, &s_pe_th
    xor r14d, r14d
.row:
    cmp r14, [rbx+P_NVIS]
    jae .fin_h
    cmp r14, 50000
    jae .fin_h
    mov rcx, [rbx+P_VIS]
    mov eax, [rcx+r14*4]
    shl rax, 5
    add rax, [rbx+P_ROWS]
    mov r15, rax
    mov eax, [r15+R_ST]
    lea rcx, [pe_htm_tr]
    mov rdi, [rcx+rax*8]
    invoke sb_append, &rsp+PE_DX, rdi
    invoke sb_append, &rsp+PE_DX, &s_pe_td
    invoke sb_append_esc, &rsp+PE_DX, [r15+R_NAME]
    invoke sb_append, &rsp+PE_DX, &s_pe_tde
    invoke sb_append, &rsp+PE_DX, &s_pe_td
    invoke sb_append_esc, &rsp+PE_DX, [r15+R_A]
    invoke sb_append, &rsp+PE_DX, &s_pe_tde
    invoke sb_append, &rsp+PE_DX, &s_pe_td
    invoke sb_append_esc, &rsp+PE_DX, [r15+R_B]
    invoke sb_append, &rsp+PE_DX, &s_pe_tde
    invoke sb_append, &rsp+PE_DX, &s_pe_td
    mov eax, [r15+R_ST]
    lea rcx, [st_names]
    mov rdx, [rcx+rax*8]
    invoke sb_append_esc, &rsp+PE_DX, rdx
    invoke sb_append, &rsp+PE_DX, &s_pe_tde
    invoke sb_append, &rsp+PE_DX, &s_pe_tre
    inc r14
    jmp .row
.fin_h:
    invoke sb_append, &rsp+PE_DX, &s_htm_tbl_end
    invoke write_utf8, r13, [rsp+PE_DX], [rsp+PE_DX+8], 1
    mov ebx, eax
    invoke sb_free, &rsp+PE_DX
    mov eax, ebx
    EPILOG 4528

; pp_export(hwnd, P, sess)
pp_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    cmp qword [rbx+P_N], 0
    je .norun
    lea rdx, [pe_def_med]
    mov rax, [r12+SS_TYPE]
    cmp rax, T_VER
    jne .nv
    lea rdx, [pe_def_ver]
.nv:
    cmp rax, T_REG
    jne .nr
    lea rdx, [pe_def_reg]
.nr:
    mov r8, rdx
    invoke ex_menu_choose, rsi, ID_BTN_DOC, r8, &rsp+400
    test eax, eax
    jz .d
    cmp eax, 1
    je .xlsx
    cmp eax, 2
    je .docx
    invoke pp_html_path, rsi, rbx, r12, &rsp+400
    jmp .res
.xlsx:
    invoke pp_xlsx_path, rsi, rbx, r12, &rsp+400
    jmp .res
.docx:
    invoke pp_docx_path, rsi, rbx, r12, &rsp+400
.res:
    invoke ex_done_open, rsi, rax, &rsp+400
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &pe_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
