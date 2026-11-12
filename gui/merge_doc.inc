; merge_doc.inc - text merge result -> Word document (.docx): summary, conflict list, merged output

%define MD_MAXLINES 50000

section .rdata
WSTR md_title, "텍스트 병합 결과"
WSTR md_f_a, "왼쪽: %s"
WSTR md_f_b, "오른쪽: %s"
WSTR md_f_c, "기준(Base): %s"
WSTR md_f_o, "출력: %s"
WSTR md_h_conf, "충돌 목록"
WSTR md_h_out, "병합 결과 (충돌 구간은 색으로 표시)"
WSTR md_c_no, "번호"
WSTR md_c_rng, "출력 줄"
WSTR md_c_left, "왼쪽 내용"
WSTR md_c_right, "오른쪽 내용"
WSTR md_c_line, "줄"
WSTR md_c_text, "내용"
WSTR md_f_u, "%u"
WSTR md_f_rng, "%u–%u"
WSTR md_sep, " / "
WSTR md_norun, "먼저 병합(F5)을 실행하세요."
WSTR s_b_doc, "워드 저장"
md_wc: dd 900, 1500, 6300, 6300
md_wo: dd 1100, 14000

section .text

; md_join(rcx=sb, rdx=mg, r8d=from, r9d=to): lines from..to-1 of the scanned output joined with " / "
md_join:
    PROLOG 64
    mov rbx, rcx
    mov rsi, rdx
    mov r12d, r8d
    mov r13d, r9d
    xor r14d, r14d
.l:
    cmp r12d, r13d
    jae .d
    test r14d, r14d
    jz .ns
    invoke sb_append, rbx, &md_sep
.ns:
    mov rax, [rsi+MG_SCL]
    mov rdx, [rax+r12*8]
    invoke sb_append, rbx, rdx
    inc r14d
    inc r12d
    jmp .l
.d:
    EPILOG 64

%macro MD_PATH 3                      ; control id, format, skip when empty
    mov word [rsp+MD_PB], 0
    invoke GetDlgItem, rsi, %1
    invoke GetWindowTextW, rax, &rsp+MD_PB, 519
%if %3
    cmp word [rsp+MD_PB], 0
    je %%skip
%endif
    invoke _snwprintf, &rsp+MD_LINE, 540, %2, &rsp+MD_PB
    invoke dw_para, &rsp+MD_DX, &rsp+MD_LINE, 20, 0
%%skip:
%endmacro

%define MD_DX   96                   ; DX ctx (40)
%define MD_SB   136                  ; sb (24)
%define MD_PB   176                  ; wchar[520]
%define MD_LINE 1216                 ; wchar[560]
%define MD_NUM  2336                 ; wchar[32]

; mg_docx_path(hwnd, mg, sess, path) -> eax 1 when written
mg_docx_path:
    PROLOG 2432
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    mov r13, r9
    invoke mg_scan, rbx
    invoke dw_begin, &rsp+MD_DX
    invoke dw_para, &rsp+MD_DX, &md_title, 32, 1
    mov word [rsp+MD_LINE], 0
    invoke GetDlgItem, rsi, ID_LBL_INFO
    invoke GetWindowTextW, rax, &rsp+MD_LINE, 500
    invoke dw_para, &rsp+MD_DX, &rsp+MD_LINE, 20, 0
    MD_PATH ID_EDIT_A, &md_f_a, 0
    MD_PATH ID_EDIT_B, &md_f_b, 0
    MD_PATH ID_EDIT_C, &md_f_c, 1
    MD_PATH ID_EDIT_O, &md_f_o, 1
    cmp qword [rbx+MG_CONF+8], 0
    je .noconf
    invoke dw_para, &rsp+MD_DX, &md_h_conf, 26, 1
    invoke dw_tbl_begin, &rsp+MD_DX, &md_wc, 4
    invoke dw_row_begin, &rsp+MD_DX, 1
    invoke dw_cell, &rsp+MD_DX, &md_c_no, &dwc_gray, 1
    invoke dw_cell, &rsp+MD_DX, &md_c_rng, &dwc_gray, 1
    invoke dw_cell, &rsp+MD_DX, &md_c_left, &dwc_gray, 1
    invoke dw_cell, &rsp+MD_DX, &md_c_right, &dwc_gray, 1
    invoke dw_row_end, &rsp+MD_DX
    xor r14d, r14d
.c:
    cmp r14, [rbx+MG_CONF+8]
    jae .cd
    lea rax, [r14+r14*2]
    shl rax, 2
    add rax, [rbx+MG_CONF]
    mov r15, rax
    invoke dw_row_begin, &rsp+MD_DX, 0
    lea eax, [r14+1]
    invoke _snwprintf, &rsp+MD_NUM, 30, &md_f_u, rax
    invoke dw_cell, &rsp+MD_DX, &rsp+MD_NUM, 0, 0
    mov ebp, [r15]
    inc ebp
    mov edi, [r15+8]
    inc edi
    invoke _snwprintf, &rsp+MD_NUM, 30, &md_f_rng, rbp, rdi
    invoke dw_cell, &rsp+MD_DX, &rsp+MD_NUM, 0, 0
    invoke sb_init, &rsp+MD_SB
    mov r8d, [r15]
    inc r8d
    mov r9d, [r15+4]
    invoke md_join, &rsp+MD_SB, rbx, r8, r9
    lea rdx, [s_empty]
    cmp qword [rsp+MD_SB+8], 0
    je .l1
    mov rdx, [rsp+MD_SB]
.l1:
    invoke dw_cell, &rsp+MD_DX, rdx, &dwc_red, 0
    invoke sb_free, &rsp+MD_SB
    invoke sb_init, &rsp+MD_SB
    mov r8d, [r15+4]
    inc r8d
    mov r9d, [r15+8]
    invoke md_join, &rsp+MD_SB, rbx, r8, r9
    lea rdx, [s_empty]
    cmp qword [rsp+MD_SB+8], 0
    je .l2
    mov rdx, [rsp+MD_SB]
.l2:
    invoke dw_cell, &rsp+MD_DX, rdx, &dwc_green, 0
    invoke sb_free, &rsp+MD_SB
    invoke dw_row_end, &rsp+MD_DX
    inc r14
    jmp .c
.cd:
    invoke dw_tbl_end, &rsp+MD_DX
.noconf:
    invoke dw_para, &rsp+MD_DX, &md_h_out, 26, 1
    invoke dw_tbl_begin, &rsp+MD_DX, &md_wo, 2
    invoke dw_row_begin, &rsp+MD_DX, 1
    invoke dw_cell, &rsp+MD_DX, &md_c_line, &dwc_gray, 1
    invoke dw_cell, &rsp+MD_DX, &md_c_text, &dwc_gray, 1
    invoke dw_row_end, &rsp+MD_DX
    xor r14d, r14d                    ; line
    xor r15d, r15d                    ; conflict cursor
.ln:
    cmp r14, [rbx+MG_SCL+8]
    jae .lnd
    cmp r14, MD_MAXLINES
    jae .lnd
.adv:
    cmp r15, [rbx+MG_CONF+8]
    jae .noadv
    lea rax, [r15+r15*2]
    shl rax, 2
    add rax, [rbx+MG_CONF]
    cmp r14d, [rax+8]
    jbe .noadv
    inc r15
    jmp .adv
.noadv:
    xor edi, edi                      ; fill
    cmp r15, [rbx+MG_CONF+8]
    jae .fill
    lea rax, [r15+r15*2]
    shl rax, 2
    add rax, [rbx+MG_CONF]
    cmp r14d, [rax]
    jb .fill
    lea rdi, [dwc_gray]               ; marker lines
    cmp r14d, [rax]
    je .fill
    cmp r14d, [rax+4]
    je .fill
    cmp r14d, [rax+8]
    je .fill
    lea rdi, [dwc_red]
    cmp r14d, [rax+4]
    jb .fill
    lea rdi, [dwc_green]
.fill:
    invoke dw_row_begin, &rsp+MD_DX, 0
    lea eax, [r14+1]
    invoke _snwprintf, &rsp+MD_NUM, 30, &md_f_u, rax
    invoke dw_cell, &rsp+MD_DX, &rsp+MD_NUM, rdi, 0
    mov rax, [rbx+MG_SCL]
    mov rdx, [rax+r14*8]
    invoke dw_cell, &rsp+MD_DX, rdx, rdi, 0
    invoke dw_row_end, &rsp+MD_DX
    inc r14
    jmp .ln
.lnd:
    invoke dw_tbl_end, &rsp+MD_DX
    invoke dw_save, &rsp+MD_DX, r13
    mov ebx, eax
    invoke sb_free, &rsp+MD_DX
    mov eax, ebx
    EPILOG 2432

; mg_export(hwnd, mg, sess)
mg_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    mov r12, r8
    cmp qword [rbx+MG_SCTEXT], 0
    je .norun
    invoke ex_pick, rsi, &rsp+400, 1
    test eax, eax
    jz .d
    invoke mg_docx_path, rsi, rbx, r12, &rsp+400
    invoke ex_done, rsi, rax
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &md_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
