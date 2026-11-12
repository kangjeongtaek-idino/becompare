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
WSTR s_b_doc, "워드 저장"
pe_widths: dd 3400, 4900, 4900, 1300

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

; pp_export(hwnd, P, sess)
pp_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    cmp qword [rbx+P_N], 0
    je .norun
    invoke ex_pick, rsi, &rsp+400, 1
    test eax, eax
    jz .d
    invoke pp_docx_path, rsi, rbx, r12, &rsp+400
    invoke ex_done, rsi, rax
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &pe_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
