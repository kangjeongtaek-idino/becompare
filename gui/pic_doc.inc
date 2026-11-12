; pic_doc.inc - picture compare result -> Word document (.docx): files, sizes, tolerance, differing pixels

section .rdata
WSTR pd_title, "이미지 비교 결과"
WSTR pd_h_item, "항목"
WSTR pd_h_val, "값"
WSTR pd_r_a, "A 파일"
WSTR pd_r_b, "B 파일"
WSTR pd_r_sa, "A 크기(픽셀)"
WSTR pd_r_sb, "B 크기(픽셀)"
WSTR pd_r_area, "비교 영역(픽셀)"
WSTR pd_r_tol, "RGB 허용 오차"
WSTR pd_r_nd, "다른 픽셀 수"
WSTR pd_r_pct, "다른 픽셀 비율"
WSTR pd_f_s, "%u × %u"
WSTR pd_f_u, "%llu"
WSTR pd_f_pct, "%llu.%02llu%%"
WSTR pd_f_res, "※ 비교 영역은 두 이미지를 변환(회전·뒤집기)한 뒤 맞춘 영역입니다."
WSTR pd_norun, "먼저 비교를 실행하세요."
WSTR s_b_doc, "워드 저장"
pd_widths: dd 3600, 10900

section .text

%define PD_DX   96                   ; DX ctx (40)
%define PD_LINE 144                  ; wchar[560]

%macro PD_ROW 1                       ; label; the value text is in PD_LINE
    invoke dw_row_begin, &rsp+PD_DX, 0
    invoke dw_cell, &rsp+PD_DX, &%1, &dwc_gray, 1
    invoke dw_cell, &rsp+PD_DX, &rsp+PD_LINE, 0, 0
    invoke dw_row_end, &rsp+PD_DX
%endmacro

; px_docx_path(hwnd, px, path) -> eax 1 when written
px_docx_path:
    PROLOG 1296
    mov rsi, rcx
    mov rbx, rdx
    mov r13, r8
    invoke dw_begin, &rsp+PD_DX
    invoke dw_para, &rsp+PD_DX, &pd_title, 32, 1
    mov word [rsp+PD_LINE], 0
    invoke GetDlgItem, rsi, ID_LBL_INFO
    invoke GetWindowTextW, rax, &rsp+PD_LINE, 500
    invoke dw_para, &rsp+PD_DX, &rsp+PD_LINE, 20, 0
    invoke dw_tbl_begin, &rsp+PD_DX, &pd_widths, 2
    invoke dw_row_begin, &rsp+PD_DX, 1
    invoke dw_cell, &rsp+PD_DX, &pd_h_item, &dwc_gray, 1
    invoke dw_cell, &rsp+PD_DX, &pd_h_val, &dwc_gray, 1
    invoke dw_row_end, &rsp+PD_DX
    invoke wcscpy, &rsp+PD_LINE, &rbx+PX_PATHA
    PD_ROW pd_r_a
    invoke wcscpy, &rsp+PD_LINE, &rbx+PX_PATHB
    PD_ROW pd_r_b
    mov ebp, [rbx+PX_A+IM_W]
    mov edi, [rbx+PX_A+IM_H]
    invoke _snwprintf, &rsp+PD_LINE, 500, &pd_f_s, rbp, rdi
    PD_ROW pd_r_sa
    mov ebp, [rbx+PX_B+IM_W]
    mov edi, [rbx+PX_B+IM_H]
    invoke _snwprintf, &rsp+PD_LINE, 500, &pd_f_s, rbp, rdi
    PD_ROW pd_r_sb
    mov ebp, [rbx+PX_W]
    mov edi, [rbx+PX_H]
    invoke _snwprintf, &rsp+PD_LINE, 500, &pd_f_s, rbp, rdi
    PD_ROW pd_r_area
    invoke _snwprintf, &rsp+PD_LINE, 500, &pd_f_u, [rbx+PX_TOL]
    PD_ROW pd_r_tol
    invoke _snwprintf, &rsp+PD_LINE, 500, &pd_f_u, [rbx+PX_NDIFF]
    PD_ROW pd_r_nd
    ; ratio in hundredths of a percent, like the info line
    mov eax, [rbx+PX_W]
    mov ecx, [rbx+PX_H]
    imul rax, rcx
    mov r8, rax
    xor r12d, r12d
    xor r14d, r14d
    test r8, r8
    jz .pct
    mov rax, [rbx+PX_NDIFF]
    imul rax, 10000
    xor edx, edx
    div r8
    xor edx, edx
    mov ecx, 100
    div rcx
    mov r12, rax
    mov r14, rdx
.pct:
    invoke _snwprintf, &rsp+PD_LINE, 500, &pd_f_pct, r12, r14
    PD_ROW pd_r_pct
    invoke dw_tbl_end, &rsp+PD_DX
    invoke dw_para, &rsp+PD_DX, &pd_f_res, 16, 0
    invoke dw_save, &rsp+PD_DX, r13
    mov ebx, eax
    invoke sb_free, &rsp+PD_DX
    mov eax, ebx
    EPILOG 1296

; px_export(hwnd, px)
px_export:
    PROLOG 1200
    mov rsi, rcx
    mov rbx, rdx
    cmp qword [rbx+PX_LOADED], 0
    je .norun
    invoke ex_pick, rsi, &rsp+400, 1
    test eax, eax
    jz .d
    invoke px_docx_path, rsi, rbx, &rsp+400
    invoke ex_done, rsi, rax
.d:
    EPILOG 1200
.norun:
    invoke msgbox, rsi, &pd_norun, MB_OK|MB_ICONINFO
    EPILOG 1200
