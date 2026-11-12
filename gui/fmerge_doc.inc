; fmerge_doc.inc - folder merge decision list -> Word document (.docx)

section .rdata
WSTR fd_title, "폴더 병합 결과"
WSTR fd_f_a, "왼쪽: %s"
WSTR fd_f_b, "오른쪽: %s"
WSTR fd_f_c, "Base(공통 조상): %s"
WSTR fd_f_o, "출력 폴더: %s"
WSTR fd_norun, "먼저 비교를 실행하세요."
WSTR s_b_doc, "워드 저장"
fd_widths: dd 5000, 1500, 1800, 1950, 1950, 1950
fd_fill: dq 0, dwc_blue, dwc_green, dwc_red, dwc_blue, dwc_green, dwc_orange, 0, dwc_orange

section .text

%define FD_DX   96                   ; DX ctx (40)
%define FD_LINE 144                  ; wchar[560]

; fm_docx_path(hwnd, fm, path) -> eax 1 when written
fm_docx_path:
    PROLOG 1296
    mov rsi, rcx
    mov rbx, rdx
    mov r13, r8
    invoke dw_begin, &rsp+FD_DX
    invoke dw_para, &rsp+FD_DX, &fd_title, 32, 1
    mov word [rsp+FD_LINE], 0
    invoke GetDlgItem, rsi, ID_LBLI
    invoke GetWindowTextW, rax, &rsp+FD_LINE, 500
    invoke dw_para, &rsp+FD_DX, &rsp+FD_LINE, 20, 0
    invoke _snwprintf, &rsp+FD_LINE, 540, &fd_f_a, &rbx+FM_ROOT
    invoke dw_para, &rsp+FD_DX, &rsp+FD_LINE, 20, 0
    invoke _snwprintf, &rsp+FD_LINE, 540, &fd_f_b, &rbx+FM_ROOT+1040
    invoke dw_para, &rsp+FD_DX, &rsp+FD_LINE, 20, 0
    cmp word [rbx+FM_ROOT+2080], 0
    je .nobase
    invoke _snwprintf, &rsp+FD_LINE, 540, &fd_f_c, &rbx+FM_ROOT+2080
    invoke dw_para, &rsp+FD_DX, &rsp+FD_LINE, 20, 0
.nobase:
    cmp word [rbx+FM_OUT], 0
    je .noout
    invoke _snwprintf, &rsp+FD_LINE, 540, &fd_f_o, &rbx+FM_OUT
    invoke dw_para, &rsp+FD_DX, &rsp+FD_LINE, 20, 0
.noout:
    invoke dw_tbl_begin, &rsp+FD_DX, &fd_widths, 6
    invoke dw_row_begin, &rsp+FD_DX, 1
    xor r12d, r12d
.h:
    cmp r12d, 6
    jae .hd
    lea rax, [col_text]
    mov rdx, [rax+r12*8]
    invoke dw_cell, &rsp+FD_DX, rdx, &dwc_gray, 1
    inc r12d
    jmp .h
.hd:
    invoke dw_row_end, &rsp+FD_DX
    xor r14d, r14d
.row:
    cmp r14, [rbx+FM_NVIS]
    jae .fin
    invoke fm_entry, rbx, r14
    mov r15, rax
    mov eax, [r15+FE_ST]
    lea rcx, [fd_fill]
    mov rdi, [rcx+rax*8]
    invoke dw_row_begin, &rsp+FD_DX, 0
    xor r12d, r12d
.c:
    cmp r12d, 6
    jae .cd
    invoke fm_cell, rbx, r15, r12
    xor ebp, ebp
    test r12d, r12d
    jz .nf
    mov rbp, rdi
.nf:
    invoke dw_cell, &rsp+FD_DX, rax, rbp, 0
    inc r12d
    jmp .c
.cd:
    invoke dw_row_end, &rsp+FD_DX
    inc r14
    jmp .row
.fin:
    invoke dw_tbl_end, &rsp+FD_DX
    invoke dw_save, &rsp+FD_DX, r13
    mov ebx, eax
    invoke sb_free, &rsp+FD_DX
    mov eax, ebx
    EPILOG 1296

; fm_export(hwnd, fm)
fm_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+FM_ENT+8], 0
    je .norun
    invoke ex_pick, rsi, &rsp+400, 1
    test eax, eax
    jz .d
    invoke fm_docx_path, rsi, rbx, &rsp+400
    invoke ex_done, rsi, rax
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &fd_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
